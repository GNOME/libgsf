/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-zip.c :
 *
 * Copyright (C) 2002 Jody Goldberg (jody@gnome.org)
 *                    Tambet Ingo   (tambet@ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf-infile-impl.h>
#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-zip-impl.h>

#include <string.h>
#include <zlib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "libgsf:zip"

typedef enum {
	ZIP_STORED =          0,
	ZIP_SHRUNK =          1,
	ZIP_REDUCEDx1 =       2,
	ZIP_REDUCEDx2 =       3,
	ZIP_REDUCEDx3 =       4,
	ZIP_REDUCEDx4 =       5,
	ZIP_IMPLODED  =       6,
	ZIP_TOKENIZED =       7,
	ZIP_DEFLATED =        8,
	ZIP_DEFLATED_BETTER = 9,
	ZIP_IMPLODED_BETTER = 10
} ZipCompressionMethod;

typedef struct {	
	char                 *name;
	ZipCompressionMethod  compr_method;
	guint32               crc32;
	size_t                csize;
	size_t                usize;
	off_t                 offset;
	off_t                 data_offset;

	guint32               restlen;
	guint32               crestlen;
	z_stream              stream;
} ZipDirent;

typedef struct {
	guint16   entries;
	guint32   dir_pos;

	int ref_count;
} ZipInfo;

struct _GsfInfileZip {
	GsfInfile parent;

	GsfInput  *input;
	ZipInfo	  *info;
	GList     *dirent_list;

	ZipDirent *dirent;
};

typedef struct {
	GsfInfileClass  parent_class;
} GsfInfileZipClass;

#define GSF_INFILE_ZIP_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GSF_INFILE_ZIP_TYPE, GsfInfileZipClass))
#define GSF_IS_INFILE_ZIP_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GSF_INFILE_ZIP_TYPE))

static gsf_off_t
zip_find_trailer (GsfInfileZip *zip)
{
	static guint8 const trailer_signature[] =
		{ 'P', 'K', 0x05, 0x06 };
	gsf_off_t offset, trailer_offset, filesize;
	size_t maplen;
	guint8 const *data;

	filesize = gsf_input_size (zip->input);
	if (filesize < ZIP_TRAILER_SIZE)
		return -1;

	trailer_offset = filesize;
	maplen = filesize;
	if (maplen != filesize) { /* Check for overflow */
		g_warning ("File too large");
		return -1;
	}
	maplen &= (ZIP_BUF_SIZE - 1);
	if (maplen == 0)
		maplen = ZIP_BUF_SIZE;
	offset = filesize - maplen; /* offset is now BUFSIZ aligned */

	while (TRUE) {
		guchar *p, *s;

		if ((gsf_input_seek (zip->input, offset, GSF_SEEK_SET)) < 0)
			return -1;

		if ((data = gsf_input_read (zip->input, maplen, NULL)) == NULL)
			return -1;

		p = (guchar *) data;
        
		for (s = p + maplen - 1; (s >= p); s--, trailer_offset--) {
			if ((*s == 'P') &&
			    (p + maplen - 1 - s > ZIP_TRAILER_SIZE - 2) &&
			    !memcmp (s, trailer_signature, sizeof (trailer_signature))) {
				return --trailer_offset;
			}
		}
        
		/* not found in currently mapped block, so update it if
		 * there is some room in before. The requirements are..
		 * (a) mappings should overlap so that trailer can cross BUFSIZ-boundary
		 * (b) trailer cannot be farther away than 64K from fileend
		 */

		/* outer loop cond */
		if (offset <= 0)
			return -1;

		/* outer loop step */
		offset -= ZIP_BUF_SIZE / 2;
		maplen = ZIP_BUF_SIZE;

		if (filesize - offset > 64 * 1024)
			return -1;
	} /*outer loop*/

	return -1;
}

static ZipDirent *
zip_dirent_new (GsfInfileZip *zip, gsf_off_t *offset)
{
	static guint8 const dirent_signature[] =
		{ 'P', 'K', 0x01, 0x02 };
	ZipDirent *dirent;
	guint8 const *data;
	guint16 name_len, extras_len, comment_len, compr_method;
	guint32 crc32, csize, usize, off;
	gchar *name;

	/* Read data and check the header */
	if (gsf_input_seek (zip->input, *offset, GSF_SEEK_SET) ||
	    NULL == (data = gsf_input_read (zip->input, ZIP_DIRENT_SIZE, NULL)) ||
	    0 != memcmp (data, dirent_signature, sizeof (dirent_signature))) {
		return NULL;
	}

	name_len =      GSF_LE_GET_GUINT16 (data + ZIP_DIRENT_NAME_SIZE);
	extras_len =    GSF_LE_GET_GUINT16 (data + ZIP_DIRENT_EXTRAS_SIZE);
	comment_len =   GSF_LE_GET_GUINT16 (data + ZIP_DIRENT_COMMENT_SIZE);

	compr_method =  GSF_LE_GET_GUINT16 (data + ZIP_DIRENT_COMPR_METHOD);
	crc32 =         GSF_LE_GET_GUINT32 (data + ZIP_DIRENT_CRC32);
	csize =         GSF_LE_GET_GUINT32 (data + ZIP_DIRENT_CSIZE);
	usize =         GSF_LE_GET_GUINT32 (data + ZIP_DIRENT_USIZE);
	off =           GSF_LE_GET_GUINT32 (data + ZIP_DIRENT_OFFSET);

	name = (gchar *) g_malloc ((gulong) (name_len + 1));
	if ((data = gsf_input_read (zip->input, name_len, NULL)) == NULL) {
		g_free (name);
		return NULL;
	}

	memcpy (name, data, name_len);
	name[name_len] = '\0';

	dirent = g_new0 (ZipDirent, 1);
	dirent->name = name;

	dirent->compr_method =  compr_method;
	dirent->crc32 =         crc32;
	dirent->csize =         csize;
	dirent->usize =         usize;
	dirent->offset =        off;
	dirent->restlen =       usize;
	dirent->crestlen =      csize;

	*offset += ZIP_DIRENT_SIZE + name_len + extras_len + comment_len;

	return dirent;
}

static void
zip_dirent_free (ZipDirent *dirent)
{
	g_return_if_fail (dirent != NULL);

	(void) inflateEnd (&dirent->stream);
	if (dirent->name != NULL) {
		g_free (dirent->name);
		dirent->name = NULL;
	}
	g_free (dirent);
}

/*****************************************************************************/
static ZipInfo *
zip_info_ref (ZipInfo *info)
{
	info->ref_count++;
	return info;
}

static void
zip_info_unref (ZipInfo *info)
{
	if (info->ref_count-- != 1)
		return;

	g_free (info);
}

/**
 * zip_dup :
 * @src :
 *
 * Utility routine to _partially_ replicate a file.  It does NOT init the dirent_list.
 *
 * Return value: the partial duplicate.
 **/
static GsfInfileZip *
zip_dup (GsfInfileZip const *src)
{
	GsfInfileZip	*dst;

	g_return_val_if_fail (src != NULL, NULL);

	dst = g_object_new (GSF_INFILE_ZIP_TYPE, NULL);
	dst->input = gsf_input_dup (src->input, NULL);
	dst->info  = zip_info_ref (src->info);

	return dst;
}

/**
 * zip_init_info :
 * @zip :
 * @err : optionally NULL
 *
 * Read zip headers and do some sanity checking
 * along the way.
 *
 * Return value: TRUE on error setting @err if it is supplied.
 **/
static gboolean
zip_init_info (GsfInfileZip *zip, GError **err)
{
	static guint8 const header_signature[] =
		{ 'P', 'K', 0x03, 0x04 };
	guint8 const *header, *trailer;
	guint16 entries, i;
	guint32 dir_pos;
	ZipInfo *info;
	gsf_off_t offset;

	/* Check the header */
	if (gsf_input_seek (zip->input, (gsf_off_t) 0, GSF_SEEK_SET) ||
	    NULL == (header = gsf_input_read (zip->input, ZIP_HEADER_SIZE, NULL)) ||
	    0 != memcmp (header, header_signature, sizeof (header_signature))) {
		g_set_error (err, gsf_input_error (), 0, "No Zip signature");
	}

	/* Find and check the trailing header */
	offset = zip_find_trailer (zip);
	if (offset < 0) {
		g_set_error (err, gsf_input_error (), 0, "No Zip trailer");
		return TRUE;
	}

	if (gsf_input_seek (zip->input, offset, GSF_SEEK_SET) ||
	    NULL == (trailer = gsf_input_read (zip->input, ZIP_TRAILER_SIZE, NULL))) {
		g_set_error (err, gsf_input_error (), 0, "Error reading Zip signature");
		return TRUE;
	}

	entries      = GSF_LE_GET_GUINT32 (trailer + ZIP_TRAILER_ENTRIES);
	dir_pos      = GSF_LE_GET_GUINT32 (trailer + ZIP_TRAILER_DIR_POS);

	info = g_new0 (ZipInfo, 1);
	zip->info = info;

	info->ref_count    = 1;
	info->entries      = entries;
	info->dir_pos      = dir_pos;

	/* Read the directory */
	for (i = 0, offset = dir_pos; i < entries; i++) {
		ZipDirent *d;

		d = zip_dirent_new (zip, &offset);
		if (d == NULL) {
			g_set_error (err, gsf_input_error (), 0, "Error reading zip dirent");
			return TRUE;
		}

		zip->dirent_list = g_list_append (zip->dirent_list, d);
	}

	return FALSE;
}

/* GsfInput class functions */

static GsfInput *
gsf_infile_zip_dup (GsfInput *src_input, GError **err)
{
	GsfInfileZip const *src = GSF_INFILE_ZIP (src_input);
	GsfInfileZip *dst = zip_dup (src);

	if (dst == NULL) {
		g_set_error (err, gsf_input_error (), 0, "Something went wrong in zip_dup.");
		return NULL;
	}

	dst->dirent = src->dirent;

	return GSF_INPUT (dst);
}

static gboolean
zip_update_stream_in (GsfInfileZip const *zip)
{
	ZipDirent *dirent;
	guint32 read_now;
	guint8 const *data;

	dirent = zip->dirent;

	if (dirent->crestlen == 0)
		return FALSE;

	read_now = (dirent->crestlen > ZIP_BLOCK_SIZE) ? ZIP_BLOCK_SIZE : dirent->crestlen;

	gsf_input_seek (zip->input, (gsf_off_t) (dirent->data_offset + dirent->stream.total_in), GSF_SEEK_SET);
	if ((data = gsf_input_read (zip->input, read_now, NULL)) == NULL)
		return FALSE;

	dirent->crestlen -= read_now;
	dirent->stream.next_in  = (unsigned char *) data;      /* next input byte */
	dirent->stream.avail_in = read_now;  /* number of bytes available at next_in */

	return TRUE;
}

static guint8 const *
gsf_infile_zip_read (GsfInput *input, size_t num_bytes, guint8 *buffer)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (input);
	ZipDirent    *dirent = zip->dirent;

	static guint8 *buf;

	if (buf == NULL)
		buf = g_malloc (256);

	if (dirent->restlen < num_bytes)
		return NULL;

	switch (dirent->compr_method) {
	case ZIP_STORED:
		dirent->restlen -= num_bytes;
		return gsf_input_read (input, num_bytes, buffer);
		break;
	case ZIP_DEFLATED:

		if (buffer == NULL)
			buffer = buf;

		dirent->stream.avail_out = num_bytes;
		dirent->stream.next_out = (unsigned char *)buffer;

		do {
			int err;
			int startlen;

			if (dirent->crestlen > 0 && dirent->stream.avail_in == 0)
				if (!zip_update_stream_in (zip))
					break;

			startlen = dirent->stream.total_out;
			err = inflate(&dirent->stream, Z_NO_FLUSH);

			if (err == Z_STREAM_END) 
				dirent->restlen = 0;
			else if (err == Z_OK)
				dirent->restlen -= (dirent->stream.total_out - startlen);
			else
				break;

		} while (dirent->restlen && dirent->stream.avail_out);

		return buffer;
		break;

	default:
		break;
	}

	return NULL;
}

static gboolean
gsf_infile_zip_seek (GsfInput *input, gsf_off_t offset, GsfSeekType whence)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (input);

	(void) zip;
	(void) offset; 
	(void) whence;

	return FALSE;
}

/* GsfInfile class functions */

/*****************************************************************************/


static GsfInput *
gsf_infile_zip_new_child (GsfInfileZip *parent, ZipDirent *dirent)
{
	GsfInfileZip *child;
	static guint8 const header_signature[] =
		{ 'P', 'K', 0x03, 0x04 };
	guint8 const *data;
	guint16 name_len, extras_len;

	child = zip_dup (parent);

	gsf_input_set_size (GSF_INPUT (child), (gsf_off_t) dirent->usize);
	gsf_input_set_name (GSF_INPUT (child), dirent->name);
	gsf_input_set_container (GSF_INPUT (child), GSF_INFILE (parent));

	child->dirent = dirent;

	/* skip local header
	 * should test tons of other info, but trust that those are correct
	 **/

	if (gsf_input_seek (child->input, (gsf_off_t) dirent->offset,
			    GSF_SEEK_SET) ||
	    NULL == (data = gsf_input_read (child->input, ZIP_FILE_HEADER_SIZE, NULL)) ||
	    0 != memcmp (data, header_signature, sizeof (header_signature))) {
		g_object_unref (child);
		return NULL;
	}

	name_len =   GSF_LE_GET_GUINT16 (data + ZIP_FILE_HEADER_NAME_SIZE);
	extras_len = GSF_LE_GET_GUINT16 (data + ZIP_FILE_HEADER_EXTRAS_SIZE);

	dirent->data_offset = dirent->offset + ZIP_FILE_HEADER_SIZE + name_len + extras_len;

	if (dirent->compr_method != ZIP_STORED) {
		int err;

		if (!zip_update_stream_in (child)) {
			g_object_unref (child);
			return NULL;
		}

		err = inflateInit2 (&child->dirent->stream, -MAX_WBITS);
		if (err != Z_OK) {
			g_object_unref (child);
			return NULL;
		}
	}

	return GSF_INPUT (child);
}

static GsfInput *
gsf_infile_zip_child_by_index (GsfInfile *infile, int target)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (infile);
	GList *p;

	for (p = zip->dirent_list; p != NULL; p = p->next)
		if (target-- <= 0)
			return gsf_infile_zip_new_child (zip, p->data);
	return NULL;
}

static char const *
gsf_infile_zip_name_by_index (GsfInfile *infile, int target)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (infile);
	GList *p;

	for (p = zip->dirent_list; p != NULL; p = p->next)
		if (target-- <= 0)
			return ((ZipDirent *)p->data)->name;
	return NULL;
}

static GsfInput *
gsf_infile_zip_child_by_name (GsfInfile *infile, char const *name)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (infile);
	GList *p;

	for (p = zip->dirent_list; p != NULL; p = p->next) {
		ZipDirent *dirent = p->data;
		if (dirent->name != NULL && !strcmp (name, dirent->name))
			return gsf_infile_zip_new_child (zip, p->data);
	}
	return NULL;
}

static int
gsf_infile_zip_num_children (GsfInfile *infile)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (infile);

	return g_list_length (zip->dirent_list);
}

static void
gsf_infile_zip_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInfileZip *zip = GSF_INFILE_ZIP (obj);
	GList *p;

	if (zip->input != NULL) {
		g_object_unref (G_OBJECT (zip->input));
		zip->input = NULL;
	}
	if (zip->info != NULL) {
		zip_info_unref (zip->info);
		zip->info = NULL;
	}
	for (p = zip->dirent_list; p != NULL; p = p->next)
		zip_dirent_free ((ZipDirent *) p->data);

	g_list_free (zip->dirent_list);

	parent_class = g_type_class_peek (GSF_INFILE_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static void
gsf_infile_zip_init (GObject *obj)
{
	GsfInfileZip *zip = (GsfInfileZip *)obj;
	zip->input =   NULL;
	zip->info  =   NULL;
	zip->dirent =  NULL;
}

static void
gsf_infile_zip_class_init (GObjectClass *gobject_class)
{
	GsfInputClass  *input_class  = GSF_INPUT_CLASS (gobject_class);
	GsfInfileClass *infile_class = GSF_INFILE_CLASS (gobject_class);

	gobject_class->finalize		= gsf_infile_zip_finalize;
	input_class->Dup		= gsf_infile_zip_dup;
	input_class->Read		= gsf_infile_zip_read;
	input_class->Seek		= gsf_infile_zip_seek;
	infile_class->num_children	= gsf_infile_zip_num_children;
	infile_class->name_by_index	= gsf_infile_zip_name_by_index;
	infile_class->child_by_index	= gsf_infile_zip_child_by_index;
	infile_class->child_by_name	= gsf_infile_zip_child_by_name;
}

GSF_CLASS (GsfInfileZip, gsf_infile_zip,
	   gsf_infile_zip_class_init, gsf_infile_zip_init,
	   GSF_INFILE_TYPE)

/**
 * gsf_infile_zip_new :
 * @source :
 * @err   :
 *
 * Opens the root directory of a Zip file.
 * NOTE : adds a reference to @source
 *
 * Returns : the new zip file handler
 **/
GsfInfile *
gsf_infile_zip_new (GsfInput *source, GError **err)
{
	GsfInfileZip *zip;

	g_return_val_if_fail (GSF_IS_INPUT (source), NULL);

	zip = g_object_new (GSF_INFILE_ZIP_TYPE, NULL);
	g_object_ref (G_OBJECT (source));
	zip->input = source;
	gsf_input_set_size (GSF_INPUT (zip), (gsf_off_t) 0);

	if (zip_init_info (zip, err)) {
		g_object_unref (G_OBJECT (zip));
		return NULL;
	}

	return GSF_INFILE (zip);
}
