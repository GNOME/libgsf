/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-zip.c :
 *
 * Copyright (C) 2002-2006 Jody Goldberg (jody@gnome.org)
 *                    	   Tambet Ingo   (tambet@ximian.com)
 * Copyright (C) 2014 Morten Welinder (terra@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf.h>
#include <gsf/gsf-zip-impl.h>
#include <glib/gi18n-lib.h>

#include <string.h>
#include <zlib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "libgsf:zip"

enum {
	PROP_0,
	PROP_SOURCE,
	PROP_COMPRESSION_LEVEL,
	PROP_INTERNAL_PARENT,
	PROP_ZIP64
};

static GObjectClass *parent_class;

typedef struct {
	guint32     entries;
	gsf_off_t   dir_pos;
	GPtrArray  *dirents;
	GsfZipVDir *vdir;

	int ref_count;
} ZipInfo;

struct _GsfInfileZip {
	GsfInfile base;

	GsfInput  *source;
	ZipInfo	  *info;
	gboolean  zip64;

	GsfZipVDir   *vdir;

	z_stream  *stream;
	gsf_off_t restlen;
	gsf_off_t crestlen;

	guint8   *buf;
	size_t    buf_size;
	gsf_off_t seek_skipped;

	GError *err;
	GsfInfileZip *dup_parent;
};

typedef struct {
	GsfInfileClass  parent_class;
} GsfInfileZipClass;

#define GSF_INFILE_ZIP_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GSF_INFILE_ZIP_TYPE, GsfInfileZipClass))
#define GSF_IS_INFILE_ZIP_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GSF_INFILE_ZIP_TYPE))


static GDateTime *
zip_make_modtime (guint32 dostime)
{
	if (dostime == 0)
		return NULL;
	else {
		gint year = (dostime >> 25) + 1980;
		gint month = (dostime >> 21) & 0x0f;
		gint day = (dostime >> 16) & 0x1f;
		gint hour = (dostime >> 11) & 0x0f;
		gint minute = (dostime >> 5) & 0x3f;
		gint second = (dostime & 0x1f) * 2;
		GDateTime *modtime =
			g_date_time_new_utc (year, month, day,
					     hour, minute, second);
		return modtime;
	}
}

static GsfZipVDir *
vdir_child_by_name (GsfZipVDir *vdir, char const *name)
{
	unsigned ui;

	for (ui = 0; ui < vdir->children->len; ui++) {
		GsfZipVDir *child = g_ptr_array_index (vdir->children, ui);
		if (strcmp (child->name, name) == 0)
			return child;
	}
	return NULL;
}

static GsfZipVDir *
vdir_child_by_index (GsfZipVDir *vdir, int target)
{
	return (unsigned)target < vdir->children->len
		? g_ptr_array_index (vdir->children, target)
		: NULL;
}

static void
vdir_insert (GsfZipVDir *vdir, char const * name, GsfZipDirent *dirent)
{
	char const *p;
	char *dirname;
	GsfZipVDir *child;

	p = strchr (name, ZIP_NAME_SEPARATOR);
	if (p) {	/* A directory */
		dirname = g_strndup (name, (gsize) (p - name));
		child = vdir_child_by_name (vdir, dirname);
		if (!child) {
			child = gsf_zip_vdir_new (dirname, TRUE, NULL);
			gsf_zip_vdir_add_child (vdir, child);
		}
		g_free (dirname);
		if (*(p+1) != '\0') {
			name = p+1;
			vdir_insert (child, name, dirent);
		}
	} else { /* A simple file name */
		child = gsf_zip_vdir_new (name, FALSE, dirent);
		gsf_zip_vdir_add_child (vdir, child);
	}
}

static gsf_off_t
zip_find_trailer (GsfInfileZip *zip, guint32 sig, gssize size)
{
	gsf_off_t offset, trailer_offset, filesize;
	gsf_off_t maplen;
	guint8 const *data;
	guchar sig1 = sig & 0xff;

	filesize = gsf_input_size (zip->source);
	if (filesize < size)
		return -1;

	trailer_offset = filesize;
	maplen = filesize & (ZIP_BUF_SIZE - 1);
	if (maplen == 0)
		maplen = ZIP_BUF_SIZE;
	offset = filesize - maplen; /* offset is now BUFSIZ aligned */

	while (TRUE) {
		guchar *p, *s;

		if (gsf_input_seek (zip->source, offset, G_SEEK_SET))
			return -1;

		if ((data = gsf_input_read (zip->source, maplen, NULL)) == NULL)
			return -1;

		p = (guchar *) data;

		for (s = p + maplen - 1; (s >= p); s--, trailer_offset--) {
			if (*s == sig1 &&
			    p + maplen - 1 - s > size - 2 &&
			    GSF_LE_GET_GUINT32 (s) == sig) {
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
		maplen = MIN (filesize - offset, ZIP_BUF_SIZE);
		trailer_offset = offset + maplen;

		if (filesize - offset > 64 * 1024)
			return -1;
	} /*outer loop*/

	return -1;
}

static guint8 const *
zip_dirent_extra_field (guint8 const *extra, size_t elen,
			guint16 typ, guint32 *pflen)
{
	while (TRUE) {
		guint16 ftyp, flen;

		if (elen == 0) {
			*pflen = 0;
			return NULL;
		}

		if (elen < 4)
			goto bad;

		ftyp = GSF_LE_GET_GUINT16 (extra);
		flen = GSF_LE_GET_GUINT16 (extra + 2);
		if (flen > elen - 4)
			goto bad;

		extra += 4;
		elen -= 4;
		if (ftyp == typ) {
			/* Found the extended data.  */
			*pflen = flen;
			return extra;
		}
		extra += flen;
		elen -= flen;
	}

bad:
	*pflen = 0;
	return NULL;
}


static GsfZipDirent *
zip_dirent_new_in (GsfInfileZip *zip, gsf_off_t *offset)
{
	GsfZipDirent *dirent;
	guint8 const *data, *variable, *extra;
	guint16 compr_method, flags;
	guint32 dostime, crc32, disk_start, name_len, extras_len, comment_len, vlen, elen;
	gsf_off_t off, csize, usize;
	gchar *name;
	guint8 header[ZIP_DIRENT_SIZE];
	gboolean zip64;

	/* Read fixed-length part of data and check the header */
	data = header;
	if (gsf_input_seek (zip->source, *offset, G_SEEK_SET) ||
	    !gsf_input_read (zip->source, ZIP_DIRENT_SIZE, header) ||
	    GSF_LE_GET_GUINT32 (data) != ZIP_DIRENT_SIGNATURE) {
		return NULL;
	}

	name_len =      GSF_LE_GET_GUINT16 (data + ZIP_DIRENT_NAME_SIZE);
	extras_len =    GSF_LE_GET_GUINT16 (data + ZIP_DIRENT_EXTRAS_SIZE);
	comment_len =   GSF_LE_GET_GUINT16 (data + ZIP_DIRENT_COMMENT_SIZE);
	vlen = name_len + extras_len + comment_len;

	/* Read variable part */
	variable = gsf_input_read (zip->source, vlen, NULL);
	if (!variable && vlen > 0)
		return NULL;
	if (FALSE && variable) gsf_mem_dump (variable, vlen);

	extra = zip_dirent_extra_field (variable + name_len, extras_len,
					ZIP_DIRENT_EXTRA_FIELD_ZIP64, &elen);
	zip64 = (extra != NULL);

	flags =         GSF_LE_GET_GUINT32 (data + ZIP_DIRENT_FLAGS);
	compr_method =  GSF_LE_GET_GUINT16 (data + ZIP_DIRENT_COMPR_METHOD);
	dostime =       GSF_LE_GET_GUINT32 (data + ZIP_DIRENT_DOSTIME);
	crc32 =         GSF_LE_GET_GUINT32 (data + ZIP_DIRENT_CRC32);
	csize =         GSF_LE_GET_GUINT32 (data + ZIP_DIRENT_CSIZE);
	usize =         GSF_LE_GET_GUINT32 (data + ZIP_DIRENT_USIZE);
	off =           GSF_LE_GET_GUINT32 (data + ZIP_DIRENT_OFFSET);
	disk_start =    GSF_LE_GET_GUINT16 (data + ZIP_DIRENT_DISKSTART);

	if (usize == 0xffffffffu && elen >= 8) {
		usize = GSF_LE_GET_GUINT64 (extra);
		extra += 8;
		elen -= 8;
	}
	if (csize == 0xffffffffu && elen >= 8) {
		csize = GSF_LE_GET_GUINT64 (extra);
		extra += 8;
		elen -= 8;
	}
	if (off == 0xffffffffu && elen >= 8) {
		off = GSF_LE_GET_GUINT64 (extra);
		extra += 8;
		elen -= 8;
	}
	if (disk_start == 0xffffu && elen >= 4) {
		disk_start = GSF_LE_GET_GUINT32 (extra);
		extra += 4;
		elen -= 4;
	}

	name = g_new (gchar, name_len + 1);
	memcpy (name, variable, name_len);
	name[name_len] = '\0';

	dirent = gsf_zip_dirent_new ();
	dirent->name = name;

	dirent->flags = flags;
	dirent->compr_method =  compr_method;
	dirent->crc32 =         crc32;
	dirent->csize =         csize;
	dirent->usize =         usize;
	dirent->offset =        off;
	dirent->dostime =       dostime;
	dirent->zip64 =         zip64;
#if 0
	g_print ("%s = 0x%x @ %" GSF_OFF_T_FORMAT "\n", name, off, *offset);
#endif

	*offset += ZIP_DIRENT_SIZE + vlen;

	return dirent;
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
	unsigned ui;

	if (info->ref_count-- != 1)
		return;

	gsf_zip_vdir_free (info->vdir, FALSE);
	for (ui = 0; ui < info->dirents->len; ui++) {
		GsfZipDirent *e = g_ptr_array_index (info->dirents, ui);
		gsf_zip_dirent_free (e);
	}
	g_ptr_array_free (info->dirents, TRUE);

	g_free (info);
}

/* Returns a partial duplicate. */
static GsfInfileZip *
zip_dup (GsfInfileZip const *src, GError **err)
{
	GsfInfileZip *dst;

	g_return_val_if_fail (src != NULL, NULL);

	dst = g_object_new (GSF_INFILE_ZIP_TYPE,
			    "internal-parent", src,
			    NULL);

	if (dst->err) {
		if (err)
			*err = g_error_copy (dst->err);
		g_object_unref (dst);
		return NULL;
	}

	return dst;
}

/**
 * zip_read_dirents:
 * @zip : #GsfInfileZip
 *
 * Read zip headers and do some sanity checking
 * along the way.
 *
 * Returns: %TRUE on error setting zip->err.
 **/
static gboolean
zip_read_dirents (GsfInfileZip *zip)
{
	guint8 const *data, *locator;
	guint32 entries, i;
	ZipInfo *info;
	gsf_off_t dir_pos, offset;

	/* Find and check the trailing header */
	offset = zip_find_trailer (zip, ZIP_TRAILER_SIGNATURE, ZIP_TRAILER_SIZE);
	if (offset < ZIP_ZIP64_LOCATOR_SIZE ||
	    gsf_input_seek (zip->source, offset - ZIP_ZIP64_LOCATOR_SIZE, G_SEEK_SET))
		goto bad;

	locator = gsf_input_read (zip->source, ZIP_TRAILER_SIZE + ZIP_ZIP64_LOCATOR_SIZE, NULL);
	if (!locator)
		goto bad;
	data = locator + ZIP_ZIP64_LOCATOR_SIZE;

	entries      = GSF_LE_GET_GUINT16 (data + ZIP_TRAILER_ENTRIES);
	dir_pos      = GSF_LE_GET_GUINT32 (data + ZIP_TRAILER_DIR_POS);

	if (GSF_LE_GET_GUINT32 (locator) == ZIP_ZIP64_LOCATOR_SIGNATURE) {
		guint32 disk, disks;
		gsf_off_t zip64_eod_offset;

		zip->zip64 = TRUE;

		data = locator;
		disk = GSF_LE_GET_GUINT32 (data + ZIP_ZIP64_LOCATOR_DISK);
		zip64_eod_offset = GSF_LE_GET_GUINT64 (data + ZIP_ZIP64_LOCATOR_OFFSET);
		disks = GSF_LE_GET_GUINT32 (data + ZIP_ZIP64_LOCATOR_DISKS);

		if (disk != 0 || disks != 1)
			goto bad;

		if (gsf_input_seek (zip->source, zip64_eod_offset, G_SEEK_SET))
			goto bad;
		data = gsf_input_read (zip->source, ZIP_TRAILER64_SIZE, NULL);
		if (!data ||
		    GSF_LE_GET_GUINT32 (data) != ZIP_TRAILER64_SIGNATURE)
			goto bad;

		entries = GSF_LE_GET_GUINT64 (data + ZIP_TRAILER64_ENTRIES);
		dir_pos = GSF_LE_GET_GUINT64 (data + ZIP_TRAILER64_DIR_POS);
	}

	info = g_new0 (ZipInfo, 1);
	zip->info = info;

	info->dirents = g_ptr_array_new ();
	info->ref_count    = 1;
	info->entries      = entries;
	info->dir_pos      = dir_pos;

	/* Read the directory */
	for (i = 0, offset = dir_pos; i < entries; i++) {
		GsfZipDirent *d;

		d = zip_dirent_new_in (zip, &offset);
		if (d == NULL) {
			zip->err = g_error_new (gsf_input_error_id (), 0,
						_("Error reading zip dirent"));
			return TRUE;
		}

		g_ptr_array_add (info->dirents, d);
	}

	return FALSE;

bad:
	zip->err = g_error_new (gsf_input_error_id (), 0,
				_("Broken zip file structure"));
	return TRUE;
}

static void
zip_build_vdirs (GsfInfileZip *zip)
{
	unsigned ui;
	ZipInfo *info = zip->info;

	info->vdir = gsf_zip_vdir_new ("", TRUE, NULL);
	for (ui = 0; ui < info->dirents->len; ui++) {
		GsfZipDirent *dirent = g_ptr_array_index (info->dirents, ui);
		vdir_insert (info->vdir, dirent->name, dirent);
	}
}

/**
 * zip_init_info :
 * @zip : #GsfInfileZip
 *
 * Read zip headers and do some sanity checking
 * along the way.
 *
 * Returns: TRUE on error setting zip->err.
 **/
static gboolean
zip_init_info (GsfInfileZip *zip)
{
	gboolean ret;

	ret = zip_read_dirents (zip);
	if (ret != FALSE)
		return ret;
	zip_build_vdirs (zip);

	return FALSE;
}

/* returns TRUE on error */
static gboolean
zip_child_init (GsfInfileZip *child, GError **errmsg)
{
	guint8 const *data = NULL;
	guint16 name_len, extras_len;
	const char *err = NULL;

	GsfZipDirent *dirent = child->vdir->dirent;

	/* skip local header
	 * should test tons of other info, but trust that those are correct
	 */

	if (gsf_input_seek (child->source, dirent->offset, G_SEEK_SET)) {
		err = _("Error seeking to zip header");
	} else if (NULL == (data = gsf_input_read (child->source, ZIP_HEADER_SIZE, NULL)))
		err = _("Error reading zip header");
	else if (GSF_LE_GET_GUINT32 (data) != ZIP_HEADER_SIGNATURE) {
		err = _("Error incorrect zip header");
		g_printerr ("Header is 0x%x\n", GSF_LE_GET_GUINT32 (data));
		g_printerr ("Expected 0x%x\n", ZIP_HEADER_SIGNATURE);
	}

	if (NULL != err) {
		if (errmsg != NULL)
			*errmsg = g_error_new_literal (gsf_input_error_id (), 0, err);
		return TRUE;
	}

	/* Throw clang a bone.  */
	g_assert (data != NULL);

	name_len =   GSF_LE_GET_GUINT16 (data + ZIP_HEADER_NAME_SIZE);
	extras_len = GSF_LE_GET_GUINT16 (data + ZIP_HEADER_EXTRAS_SIZE);

	dirent->data_offset = dirent->offset + ZIP_HEADER_SIZE + name_len + extras_len;
	child->restlen  = dirent->usize;
	child->crestlen = dirent->csize;

	if (dirent->compr_method != GSF_ZIP_STORED) {
		int err;

		if (!child->stream)
			child->stream = g_new0 (z_stream, 1);

		err = inflateInit2 (child->stream, -MAX_WBITS);
		if (err != Z_OK) {
			if (errmsg != NULL)
				*errmsg = g_error_new (gsf_input_error_id (), 0,
						       _("problem uncompressing stream"));
			return TRUE;
		}
	}

	return FALSE;
}

/* GsfInput class functions */

static GsfInput *
gsf_infile_zip_dup (GsfInput *src_input, GError **err)
{
	GsfInfileZip const *src = GSF_INFILE_ZIP (src_input);
	GsfInfileZip *dst = zip_dup (src, err);

	if (dst == NULL)
		return NULL;

	dst->vdir = src->vdir;

	if (dst->vdir->dirent && zip_child_init (dst, err)) {
		g_object_unref (dst);
		return NULL;
	}

	return GSF_INPUT (dst);
}

static gboolean
zip_update_stream_in (GsfInfileZip *zip)
{
	guint32 read_now;
	guint8 const *data;
	gsf_off_t pos;

	if (zip->crestlen == 0)
		return FALSE;

	read_now = MIN (zip->crestlen, ZIP_BLOCK_SIZE);

	pos = zip->vdir->dirent->data_offset + zip->stream->total_in;
	if (gsf_input_seek (zip->source, pos, G_SEEK_SET))
		return FALSE;
	if ((data = gsf_input_read (zip->source, read_now, NULL)) == NULL)
		return FALSE;

	zip->crestlen -= read_now;
	zip->stream->next_in  = (unsigned char *) data;      /* next input byte */
	zip->stream->avail_in = read_now;  /* number of bytes available at next_in */

	return TRUE;
}

static guint8 const *
gsf_infile_zip_read (GsfInput *input, size_t num_bytes, guint8 *buffer)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (input);
	GsfZipVDir *vdir = zip->vdir;
	gsf_off_t pos;

	if (zip->restlen < (gsf_off_t)num_bytes)
		return NULL;

	switch (vdir->dirent->compr_method) {
	case GSF_ZIP_STORED:
		zip->restlen -= num_bytes;
		pos = zip->vdir->dirent->data_offset + input->cur_offset;
		if (gsf_input_seek (zip->source, pos, G_SEEK_SET))
			return NULL;
		return gsf_input_read (zip->source, num_bytes, buffer);

	case GSF_ZIP_DEFLATED:
		if (buffer == NULL) {
			if (zip->buf_size < num_bytes) {
				zip->buf_size = MAX (num_bytes, 256);
				g_free (zip->buf);
				zip->buf = g_new (guint8, zip->buf_size);
			}
			buffer = zip->buf;
		}

		zip->stream->avail_out = num_bytes;
		zip->stream->next_out = (unsigned char *)buffer;

		do {
			int err;
			gsf_off_t startlen;

			if (zip->crestlen > 0 && zip->stream->avail_in == 0)
				if (!zip_update_stream_in (zip))
					break;

			startlen = zip->stream->total_out;
			err = inflate(zip->stream, Z_NO_FLUSH);

			if (err == Z_STREAM_END)
				zip->restlen = 0;
			else if (err == Z_OK)
				zip->restlen -= (zip->stream->total_out - startlen);
			else
				return NULL;  /* Error, probably corrupted */

		} while (zip->restlen && zip->stream->avail_out);

		return buffer;

	default:
		break;
	}

	return NULL;
}

static gboolean
gsf_infile_zip_seek (GsfInput *input, gsf_off_t offset, GSeekType whence)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (input);
	/* Global flag -- we don't want one per stream.  */
	static gboolean warned = FALSE;
	gsf_off_t pos = offset;

	/* Note, that pos has already been sanity checked.  */
	switch (whence) {
	case G_SEEK_SET : break;
	case G_SEEK_CUR : pos += input->cur_offset;	break;
	case G_SEEK_END : pos += input->size;		break;
	default : return TRUE;
	}

	if (zip->stream) {
		(void) inflateEnd (zip->stream);
		memset (zip->stream, 0, sizeof (z_stream));
	}

	if (zip_child_init (zip, NULL)) {
		g_warning ("failure initializing zip child");
		return TRUE;
	}

	input->cur_offset = 0;
	if (gsf_input_seek_emulate (input, pos))
		return TRUE;

	zip->seek_skipped += pos;
	if (!warned &&
	    zip->seek_skipped != pos && /* Don't warn for single seek.  */
	    zip->seek_skipped >= 1000000) {
		warned = TRUE;
		g_warning ("Seeking in zip child streams is awfully slow.");
	}

	return FALSE;
}

/* GsfInfile class functions */

/*****************************************************************************/

static GsfInput *
gsf_infile_zip_new_child (GsfInfileZip *parent, GsfZipVDir *vdir, GError **err)
{
	GsfInfileZip *child;
	GsfZipDirent *dirent = vdir->dirent;
	child = zip_dup (parent, err);

	if (child == NULL)
		return NULL;

	gsf_input_set_name (GSF_INPUT (child), vdir->name);
	gsf_input_set_container (GSF_INPUT (child), GSF_INFILE (parent));

	child->vdir = vdir;

	if (dirent) {
		gsf_input_set_size (GSF_INPUT (child), dirent->usize);

		if (dirent->dostime) {
			GDateTime *modtime = zip_make_modtime (dirent->dostime);
			gsf_input_set_modtime (GSF_INPUT (child), modtime);
			g_date_time_unref (modtime);
		}

		if (zip_child_init (child, err) != FALSE) {
			g_object_unref (child);
			return NULL;
		}
	} else
		gsf_input_set_size (GSF_INPUT (child), 0);

	return GSF_INPUT (child);
}

static GsfInput *
gsf_infile_zip_child_by_index (GsfInfile *infile, int target, GError **err)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (infile);
	GsfZipVDir *child_vdir = vdir_child_by_index (zip->vdir, target);

	if (child_vdir)
		return gsf_infile_zip_new_child (zip, child_vdir, err);

	return NULL;
}

static char const *
gsf_infile_zip_name_by_index (GsfInfile *infile, int target)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (infile);
	GsfZipVDir *child_vdir = vdir_child_by_index (zip->vdir, target);

	if (child_vdir)
		return child_vdir->name;

	return NULL;
}

static GsfInput *
gsf_infile_zip_child_by_name (GsfInfile *infile, char const *name, GError **err)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (infile);
	GsfZipVDir *child_vdir = vdir_child_by_name (zip->vdir, name);

	if (child_vdir)
		return gsf_infile_zip_new_child (zip, child_vdir, err);

	return NULL;
}

static int
gsf_infile_zip_num_children (GsfInfile *infile)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (infile);

	g_return_val_if_fail (zip->vdir != NULL, -1);

	if (!zip->vdir->is_directory)
		return -1;
	return zip->vdir->children->len;
}

static void
gsf_infile_zip_set_source (GsfInfileZip *zip, GsfInput *src)
{
	if (src)
		src = gsf_input_proxy_new (src);
	if (zip->source)
		g_object_unref (zip->source);
	zip->source = src;
}

static void
gsf_infile_zip_finalize (GObject *obj)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (obj);

	if (zip->info != NULL) {
		zip_info_unref (zip->info);
		zip->info = NULL;
	}

	if (zip->stream) {
		(void) inflateEnd (zip->stream);
		g_free (zip->stream);
		zip->stream = NULL;
	}

	g_free (zip->buf);
	zip->buf = NULL;

	gsf_infile_zip_set_source (zip, NULL);
	g_clear_error (&zip->err);

	parent_class->finalize (obj);
}


static GObject*
gsf_infile_zip_constructor (GType                  type,
			    guint                  n_construct_properties,
			    GObjectConstructParam *construct_params)
{
	GsfInfileZip *zip;

	zip = (GsfInfileZip *)(parent_class->constructor (type,
							  n_construct_properties,
							  construct_params));
	if (zip->dup_parent) {
		/* Special call from zip_dup.  */
		zip->source = gsf_input_dup (zip->dup_parent->source, &zip->err);
		zip->info = zip_info_ref (zip->dup_parent->info);
		zip->zip64 = zip->dup_parent->zip64;
		zip->dup_parent = NULL;
	} else {
		if (!zip_init_info (zip))
			zip->vdir = zip->info->vdir;
	}

	return (GObject *)zip;
}

static void
gsf_infile_zip_init (GObject *obj)
{
	GsfInfileZip *zip = (GsfInfileZip *)obj;
	zip->source = NULL;
	zip->info = NULL;
	zip->zip64 = FALSE;
	zip->vdir = NULL;
	zip->stream = NULL;
	zip->restlen = 0;
	zip->crestlen = 0;
	zip->buf = NULL;
	zip->buf_size = 0;
	zip->seek_skipped = 0;
	zip->err = NULL;
}

static void
gsf_infile_zip_get_property (GObject     *object,
			     guint        property_id,
			     GValue      *value,
			     GParamSpec  *pspec)
{
	GsfInfileZip *zip = (GsfInfileZip *)object;

	switch (property_id) {
	case PROP_SOURCE:
		g_value_set_object (value, zip->source);
		break;
	case PROP_COMPRESSION_LEVEL:
		g_value_set_int (value,
				 zip->vdir->dirent
				 ? zip->vdir->dirent->compr_method
				 : 0);
		break;
	case PROP_ZIP64:
                g_value_set_boolean (value, zip->zip64);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gsf_infile_zip_set_property (GObject      *object,
			     guint         property_id,
			     GValue const *value,
			     GParamSpec   *pspec)
{
	GsfInfileZip *zip = (GsfInfileZip *)object;

	switch (property_id) {
	case PROP_SOURCE:
		gsf_infile_zip_set_source (zip, g_value_get_object (value));
		break;
	case PROP_INTERNAL_PARENT:
		zip->dup_parent = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void
gsf_infile_zip_class_init (GObjectClass *gobject_class)
{
	GsfInputClass  *input_class  = GSF_INPUT_CLASS (gobject_class);
	GsfInfileClass *infile_class = GSF_INFILE_CLASS (gobject_class);

	gobject_class->constructor      = gsf_infile_zip_constructor;
	gobject_class->finalize		= gsf_infile_zip_finalize;
	gobject_class->get_property     = gsf_infile_zip_get_property;
	gobject_class->set_property     = gsf_infile_zip_set_property;

	input_class->Dup		= gsf_infile_zip_dup;
	input_class->Read		= gsf_infile_zip_read;
	input_class->Seek		= gsf_infile_zip_seek;
	infile_class->num_children	= gsf_infile_zip_num_children;
	infile_class->name_by_index	= gsf_infile_zip_name_by_index;
	infile_class->child_by_index	= gsf_infile_zip_child_by_index;
	infile_class->child_by_name	= gsf_infile_zip_child_by_name;

	parent_class = g_type_class_peek_parent (gobject_class);

	g_object_class_install_property
		(gobject_class,
		 PROP_SOURCE,
		 g_param_spec_object ("source",
				      _("Source"),
				      _("The archive being interpreted"),
				      GSF_INPUT_TYPE,
				      G_PARAM_STATIC_STRINGS |
				      G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GsfInfileZip:compression-level:
	 *
	 * Controls the level of compression used for new members.
	 */
	g_object_class_install_property
		(gobject_class,
		 PROP_COMPRESSION_LEVEL,
		 g_param_spec_int ("compression-level",
				   _("Compression Level"),
				   _("The level of compression used, zero meaning none"),
				   0, 10,
				   0,
				   G_PARAM_STATIC_STRINGS |
				   G_PARAM_READABLE));

	g_object_class_install_property
		(gobject_class,
		 PROP_INTERNAL_PARENT,
		 g_param_spec_object ("internal-parent",
				      "",
				      "Internal use only",
				      GSF_INFILE_ZIP_TYPE,
				      G_PARAM_STATIC_STRINGS |
				      G_PARAM_WRITABLE |
				      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property
		(gobject_class,
		 PROP_ZIP64,
		 g_param_spec_boolean ("zip64",
				       _("Zip64"),
				       _("Whether zip64 is being used"),
				       FALSE,
				       G_PARAM_STATIC_STRINGS |
				       G_PARAM_READABLE));
}

GSF_CLASS (GsfInfileZip, gsf_infile_zip,
	   gsf_infile_zip_class_init, gsf_infile_zip_init,
	   GSF_INFILE_TYPE)

/**
 * gsf_infile_zip_new:
 * @source: A base #GsfInput
 * @err: (allow-none): place to store a #GError if anything goes wrong
 *
 * Opens the root directory of a Zip file.
 * <note>This adds a reference to @source.</note>
 *
 * Returns: the new zip file handler
 **/
GsfInfile *
gsf_infile_zip_new (GsfInput *source, GError **err)
{
	GsfInfileZip *zip;

	g_return_val_if_fail (GSF_IS_INPUT (source), NULL);

	zip = g_object_new (GSF_INFILE_ZIP_TYPE,
			    "source", source,
			    NULL);

	if (zip->err) {
		if (err)
			*err = g_error_copy (zip->err);
		g_object_unref (zip);
		return NULL;
	}

	return GSF_INFILE (zip);
}
