/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-msvba.c :
 *
 * Copyright (C) 2002-2003 Jody Goldberg (jody@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/* Info extracted from
 *	svx/source/msfilter/msvbasic.cxx
 *	Costin Raiu, Kaspersky Labs, 'Apple of Discord'
 *	Virus bulletin's bontchev.pdf, svajcer.pdf
 */
#include <gsf-config.h>
#include <gsf/gsf-infile-impl.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-infile-msvba.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-utils.h>

#include <stdio.h>
#include <string.h>

typedef struct {
	char	 *name;
	guint32   offset;
} MSVBADirent;

struct _GsfInfileMSVBA {
	GsfInfile parent;

	GsfInfile *source;
	GList *children;
};

typedef struct {
	GsfInfileClass  parent_class;
} GsfInfileMSVBAClass;

#define GSF_INFILE_MSVBA_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GSF_INFILE_MSVBA_TYPE, GsfInfileMSVBAClass))
#define GSF_IS_INFILE_MSVBA_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GSF_INFILE_MSVBA_TYPE))

#define VBA56_DIRENT_RECORD_COUNT (2 + /* magic */		\
				   4 + /* version */		\
				   2 + /* 0x00 0xff */		\
				  22)  /* unknown */
#define VBA56_DIRENT_HEADER_SIZE (VBA56_DIRENT_RECORD_COUNT +	\
				  2 +  /* type1 record count */	\
				  2)   /* unknown */
#define VBA_COMPRESSION_WINDOW 4096

static guint8 *
vba_inflate (GsfInput *input, gsf_off_t offset, int *size)
{
	GByteArray *res;
	unsigned	i, win_pos, pos = 0;
	unsigned	mask, shift, distance;
	guint8		flag, buffer [VBA_COMPRESSION_WINDOW];
	guint8 const   *tmp;
	guint16		token, len;
	gboolean	clean = TRUE;

	if (gsf_input_seek (input, offset+3, G_SEEK_SET))
		return NULL;

	res = g_byte_array_new ();

	/* explaination from libole2/ms-ole-vba.c */
	/* The first byte is a flag byte.  Each bit in this byte
	 * determines what the next byte is.  If the bit is zero,
	 * the next byte is a character.  Otherwise the  next two
	 * bytes contain the number of characters to copy from the
	 * umcompresed buffer and where to copy them from (offset,
	 * length).
	 */
	while (NULL != gsf_input_read (input, 1, &flag))
		for (mask = 1; mask < 0x100 ; mask <<= 1)
			if (flag & mask) {
				if (NULL == (tmp = gsf_input_read (input, 2, NULL)))
					break;
				win_pos = pos % VBA_COMPRESSION_WINDOW;
				if (win_pos <= 0x80) {
					if (win_pos <= 0x20)
						shift = (win_pos <= 0x10) ? 12 : 11;
					else
						shift = (win_pos <= 0x40) ? 10 : 9;
				} else {
					if (win_pos <= 0x200)
						shift = (win_pos <= 0x100) ? 8 : 7;
					else if (win_pos <= 0x800)
						shift = (win_pos <= 0x400) ? 6 : 5;
					else
						shift = 4;
				}

				token = GSF_LE_GET_GUINT16 (tmp);
				len = (token & ((1 << shift) - 1)) + 3;
				distance = token >> shift;
				clean = TRUE;

				for (i = 0; i < len; i++) {
					unsigned srcpos = (pos - distance - 1) % VBA_COMPRESSION_WINDOW;
					guint8 c = buffer [srcpos];
					buffer [pos++ % VBA_COMPRESSION_WINDOW] = c;
				}
			} else {
				if ((pos != 0) && ((pos % VBA_COMPRESSION_WINDOW) == 0) && clean) {
					(void) gsf_input_read (input, 2, NULL);
					clean = FALSE;
					g_byte_array_append (res, buffer, VBA_COMPRESSION_WINDOW);
					break;
				}
				if (NULL != gsf_input_read (input, 1, buffer + (pos % VBA_COMPRESSION_WINDOW)))
					pos++;
				clean = TRUE;
			}

	if (pos % VBA_COMPRESSION_WINDOW)
		g_byte_array_append (res, buffer, pos % VBA_COMPRESSION_WINDOW);
	*size = res->len;
	return g_byte_array_free (res, FALSE);
}

static guint8 const *
vba3_dirent_read (guint8 const *data, int *size)
{
	static guint16 const magic [] = { 0x19, 0x47, 0x1a, 0x32 };
	int name_len, i, j, offset = 0;
	guint16 tmp16;

	g_return_val_if_fail (*size >= 2, NULL);

	/* make sure there is enough to read the first size */
	for (offset = 0, i = 0 ; i < 4; i++) {
		char *name = NULL;

		/* check for the magic numbers */
		tmp16 = GSF_LE_GET_GUINT16 (data + offset);
		if (tmp16 != magic [i]) {
			/* TODO : Need to find the record count somewhere.
			 * for now the last record seems to have 0x10 00 00 00 00 00 */
			if (i != 0 || tmp16 != 0x10)
				g_warning ("exiting with %hx", tmp16);
			return NULL;
		}

		/* be very careful reading the name size */
		offset += 2;
		g_return_val_if_fail ((offset + 4) < *size, NULL);
		name_len = GSF_LE_GET_GUINT32 (data + offset);
		offset += 4;
		g_return_val_if_fail ((offset + name_len) < *size, NULL);

		if (i % 2) {  /* unicode */
			gunichar2 *uni_name = g_new0 (gunichar2, name_len/2 + 1);

			/* be wary about endianness */
			for (j = 0 ; j < name_len ; j += 2)
				uni_name [j/2] = GSF_LE_GET_GUINT16 (data + offset + j);
			name = g_utf16_to_utf8 (uni_name, -1, NULL, NULL, NULL);
			g_free (uni_name);
		} else /* ascii */
			name = g_strndup (data + offset, (unsigned)name_len);

		if (i == 0)
			printf ("%s\t: ", name);
		g_free (name);
		offset += name_len;
	}

	/* the rest of the dirent appears constant */
	g_return_val_if_fail ((offset + 32) < *size, NULL);

/*
 *    1c 00 00 00
 *    00 00
 *    48 00
 *    00 00 00 00
 *    31 00
 *    04 00 00 00
 */
	printf ("src offset = 0x%x\n", GSF_LE_GET_GUINT32 (data + offset + 18));
/*
 *    1e 00
 *    04 00 00 00
 *    00 00 00 00
 *    2c 00
 *    02 00 00 00
 */
	printf ("\t var1 = 0x%hx\n", GSF_LE_GET_GUINT16 (data + offset + 38));
	printf ("\t var2 = 0x%hx\n", GSF_LE_GET_GUINT16 (data + offset + 40));
/*
 *    00 00 00 00
 *    2b 00 00 00
 *    00 00
 */

	*size -= offset + 52;
	return data + offset + 52;
}

/**
 * vba_init_info :
 * @vba :
 * @err : optionally NULL
 *
 * Read an VBA dirctory and its project file.
 * along the way.
 *
 * Return value: FALSE on error setting @err if it is supplied.
 **/
static gboolean
vba3_dir_read (GsfInfileMSVBA *vba, GError **err)
{
	static struct {
		size_t   const offset;
		gboolean const is_unicode;
	} const magic [] = {
		{ 0x28, FALSE },			/* VBAProject */
		{ 0x46, FALSE },	{ 0x02, TRUE },	/* stdole */
		{ 0x06, FALSE },			/* OLE.Automation */
		{ 0x08, FALSE },	{ 0x02, TRUE },	/* MSForms */
		{ 0x02, FALSE },			/* object library */
		{ 0x06, FALSE }
	};

	GsfInput *dir;
	size_t inflated_size;
	guint8 *inflated;
	unsigned offset, name_len, i, j;
	char *name;

	dir = gsf_infile_child_by_name (vba->source, "dir");
	if (dir == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"Can't find the VBA directory stream.");
		return FALSE;
	}

	inflated = vba_inflate (dir, (gsf_off_t) 0, &inflated_size);
	if (inflated != NULL) {

		offset = 0;
		for (i = 0; i < G_N_ELEMENTS (magic); i++) {
			/* be very careful reading the name size */
			offset += magic [i].offset;
			g_return_val_if_fail ((offset + 4) < inflated_size, FALSE);
			name_len = GSF_LE_GET_GUINT32 (inflated + offset);
			offset += 4;
			g_return_val_if_fail ((offset + name_len) < inflated_size, FALSE);

			if (magic [i].is_unicode) {  /* unicode */
				gunichar2 *uni_name = g_new0 (gunichar2, name_len/2 + 1);

				/* be wary about endianness */
				for (j = 0 ; j < name_len ; j += 2)
					uni_name [j/2] = GSF_LE_GET_GUINT16 (inflated + offset + j);
				name = g_utf16_to_utf8 (uni_name, -1, NULL, NULL, NULL);
				g_free (uni_name);
			} else /* ascii */
				name = g_strndup (inflated + offset, (unsigned)name_len);

			offset += name_len;
			puts (name);
		}
#if 0
#warning figure out this offset
		size = inflated_size - 0x333;
		data = inflated + 0x333;
		printf ("SIZE == 0x%x\n", size);
		gsf_mem_dump (inflated, inflated_size);

		while (NULL != (data = vba3_dirent_read (data, &size)))
			;
#endif
		g_free (inflated);
	}
	g_object_unref (G_OBJECT (dir));
	return TRUE;
}

/**
 * vba_init_info :
 * @vba :
 * @err : optionally NULL
 *
 * Read an VBA dirctory and its project file.
 * along the way.
 *
 * Return value: FALSE on error setting @err if it is supplied.
 **/
static gboolean
vba56_dir_read (GsfInfileMSVBA *vba, GError **err)
{
	/* NOTE : This seems constant, find some confirmation */
	static guint8 const signature[]	  = { 0xcc, 0x61 };
	static struct {
		guint8 const signature[4];
		char const * const name;
		int const vba_version;
		gboolean const is_mac;
	} const  versions [] = {
		{ { 0x5e, 0x00, 0x00, 0x01 }, "Office 97",		5, FALSE },
		{ { 0x5f, 0x00, 0x00, 0x01 }, "Office 97 SR1",		5, FALSE },
		{ { 0x65, 0x00, 0x00, 0x01 }, "Office 2000 alpha?",	6, FALSE },
		{ { 0x6b, 0x00, 0x00, 0x01 }, "Office 2000 beta?",	6, FALSE },
		{ { 0x6d, 0x00, 0x00, 0x01 }, "Office 2000",		6, FALSE },
		{ { 0x70, 0x00, 0x00, 0x01 }, "Office XP beta 1/2",	6, FALSE },
		{ { 0x73, 0x00, 0x00, 0x01 }, "Office XP",		6, FALSE },
		{ { 0x60, 0x00, 0x00, 0x0e }, "MacOffice 98",		5, TRUE },
		{ { 0x62, 0x00, 0x00, 0x0e }, "MacOffice 2001",		5, TRUE }
	};

	guint8 const *data;
	unsigned i, count, len;
	gunichar2 *uni_name;
	char *name;
	GsfInput *dir;

	dir = gsf_infile_child_by_name (vba->source, "_VBA_PROJECT");
	if (dir == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"Can't find the VBA directory stream.");
		return FALSE;
	}

	if (gsf_input_seek (dir, (gsf_off_t) 0, G_SEEK_SET) ||
	    NULL == (data = gsf_input_read (dir, VBA56_DIRENT_HEADER_SIZE, NULL)) ||
	    0 != memcmp (data, signature, sizeof (signature))) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"No VBA signature");
		return FALSE;
	}

	for (i = 0 ; i < G_N_ELEMENTS (versions); i++)
		if (!memcmp (data+2, versions[i].signature, 4))
			break;

	if (i >= G_N_ELEMENTS (versions)) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"Unknown VBA version signature 0x%x%x%x%x",
				data[2], data[3], data[4], data[5]);
		return FALSE;
	}

	puts (versions[i].name);

	/* these depend strings seem to come in 2 blocks */
	count = GSF_LE_GET_GUINT16 (data + VBA56_DIRENT_RECORD_COUNT);
	for (; count > 0 ; count--) {
		if (NULL == ((data = gsf_input_read (dir, 2, NULL))))
			break;
		len = GSF_LE_GET_GUINT16 (data);
		if (NULL == ((data = gsf_input_read (dir, len, NULL)))) {
			printf ("len == 0x%x ??\n", len);
			break;
		}

		uni_name = g_new0 (gunichar2, len/2 + 1);

		/* be wary about endianness */
		for (i = 0 ; i < len ; i += 2)
			uni_name [i/2] = GSF_LE_GET_GUINT16 (data + i);
		name = g_utf16_to_utf8 (uni_name, -1, NULL, NULL, NULL);
		g_free (uni_name);

		printf ("%d %s\n", count, name);

		/* ignore this blob ???? */
		if (!strncmp ("*\\G", name, 3)) {
			if (NULL == ((data = gsf_input_read (dir, 12, NULL)))) {
				printf ("len == 0x%x ??\n", len);
				break;
			}
		}

		g_free (name);
	}

	g_return_val_if_fail (count == 0, FALSE);

	return TRUE;
}

static void
vba_dirent_free (MSVBADirent *data, gpointer ignore)
{
	(void) ignore;
	g_free (data->name);
	g_free (data);
}

static void
gsf_infile_msvba_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (obj);

	if (vba->source != NULL) {
		g_object_unref (G_OBJECT (vba->source));
		vba->source = NULL;
	}
	if (vba->children != NULL) {
		g_list_foreach (vba->children, (GFunc) vba_dirent_free, NULL);
		g_list_free (vba->children);
		vba->children = NULL;
	}

	parent_class = g_type_class_peek (GSF_INFILE_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static GsfInput *
gsf_infile_msvba_dup (GsfInput *src_input, GError **err)
{
	GsfInfileMSVBA const *src = GSF_INFILE_MSVBA (src_input);
	GsfInfileMSVBA *dst = NULL;

	(void) src;
	(void) err;
#warning TODO
	return GSF_INPUT (dst);
}

static guint8 const *
gsf_infile_msvba_read (GsfInput *input, size_t num_bytes, guint8 *buffer)
{
	/* no data at this level */
	(void)input; (void)num_bytes; (void)buffer;
	return NULL;
}

static gboolean
gsf_infile_msvba_seek (GsfInput *input, gsf_off_t offset, GSeekType whence)
{
	/* no data at this level */
	(void)input; (void)offset; (void)whence;
	return offset != 0;
}

static GsfInput *
gsf_infile_msvba_new_child (GsfInfileMSVBA *parent, MSVBADirent *dirent)
{
	GsfInputMemory *child = NULL;

	(void) parent;
	(void) dirent;
#warning TODO
	return GSF_INPUT (child);
}

static GsfInput *
gsf_infile_msvba_child_by_index (GsfInfile *infile, int target)
{
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (infile);
	GList *p;

	for (p = vba->children; p != NULL ; p = p->next)
		if (target-- <= 0)
			return gsf_infile_msvba_new_child (vba, p->data);
	return NULL;
}

static char const *
gsf_infile_msvba_name_by_index (GsfInfile *infile, int target)
{
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (infile);
	GList *p;

	for (p = vba->children; p != NULL ; p = p->next)
		if (target-- <= 0)
			return ((MSVBADirent *)p->data)->name;
	return NULL;
}

static GsfInput *
gsf_infile_msvba_child_by_name (GsfInfile *infile, char const *name)
{
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (infile);
	GList *p;

	for (p = vba->children; p != NULL ; p = p->next) {
		MSVBADirent *dirent = p->data;
		if (dirent->name != NULL && !strcmp (name, dirent->name))
			return gsf_infile_msvba_new_child (vba, dirent);
	}
	return NULL;
}

static int
gsf_infile_msvba_num_children (GsfInfile *infile)
{
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (infile);
	g_return_val_if_fail (vba != NULL, 0);
	return g_list_length (vba->children);
}

static void
gsf_infile_msvba_init (GObject *obj)
{
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (obj);

	vba->source		= NULL;
	vba->children		= NULL;
}

static void
gsf_infile_msvba_class_init (GObjectClass *gobject_class)
{
	GsfInputClass  *input_class  = GSF_INPUT_CLASS (gobject_class);
	GsfInfileClass *infile_class = GSF_INFILE_CLASS (gobject_class);

	gobject_class->finalize		= gsf_infile_msvba_finalize;
	input_class->Dup		= gsf_infile_msvba_dup;
	input_class->Read		= gsf_infile_msvba_read;
	input_class->Seek		= gsf_infile_msvba_seek;
	infile_class->num_children	= gsf_infile_msvba_num_children;
	infile_class->name_by_index	= gsf_infile_msvba_name_by_index;
	infile_class->child_by_index	= gsf_infile_msvba_child_by_index;
	infile_class->child_by_name	= gsf_infile_msvba_child_by_name;
}

GSF_CLASS (GsfInfileMSVBA, gsf_infile_msvba,
	   gsf_infile_msvba_class_init, gsf_infile_msvba_init,
	   GSF_INFILE_TYPE)

GsfInfileMSVBA *
gsf_infile_msvba_new (GsfInfile *source, GError **err)
{
	GsfInfileMSVBA *vba;

	g_return_val_if_fail (GSF_IS_INFILE (source), NULL);

	vba = g_object_new (GSF_INFILE_MSVBA_TYPE, NULL);
	g_object_ref (G_OBJECT (source));
	vba->source = source;
	gsf_input_set_size (GSF_INPUT (vba), (gsf_off_t) 0);

	/* find the name offset pairs */
	if (vba56_dir_read (vba, err) || vba3_dir_read (vba, err))
		return vba;

	if (err != NULL && *err == NULL) {
		*err = g_error_new (gsf_input_error (), 0,
				"Unable to parse VBA header");
	}

	g_object_unref (G_OBJECT (vba));
	return NULL;
}
