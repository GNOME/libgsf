/*
 * gsf-infile-msvba.c :
 *
 * Copyright (C) 2002-2006 Jody Goldberg (jody@gnome.org)
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

/* Info extracted from
 *	svx/source/msfilter/msvbasic.cxx
 *	Costin Raiu, Kaspersky Labs, 'Apple of Discord'
 *	Virus bulletin's bontchev.pdf, svajcer.pdf
 *
 * and lots and lots of reading.  There are lots of pieces missing still
 * but the structure seems to hold together.
 */
#include <gsf-config.h>
#include <gsf/gsf-infile-msvba.h>
#include <gsf/gsf.h>
#include <glib/gi18n-lib.h>

#include <string.h>

static GObjectClass *parent_class;

struct _GsfInfileMSVBA {
	GsfInfile parent;

	GsfInfile	*source;
	GList		*children;

	GHashTable	*modules;
};
typedef GsfInfileClass GsfInfileMSVBAClass;

#define GSF_INFILE_MSVBA_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GSF_INFILE_MSVBA_TYPE, GsfInfileMSVBAClass))
#define GSF_IS_INFILE_MSVBA_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GSF_INFILE_MSVBA_TYPE))

/**
 * gsf_vba_inflate:
 * @input: stream to read from
 * @offset: offset into it for start byte of compressed stream
 * @size: size of the returned array
 * @add_null_terminator: whenever add or not null at the end of array
 * 
 * Decompresses VBA stream.
 * 
 * Return value: A pointer to guint8 array
 **/
guint8 *
gsf_vba_inflate (GsfInput *input, gsf_off_t offset, int *size, gboolean add_null_terminator)
{
	guint8 sig;
	GByteArray *res;
	gsf_off_t length;

	res = g_byte_array_new();

	gsf_input_read (input, 1, &sig);
	if (1 != sig) /* should start with 0x01 */
		return NULL;
	offset++;

	length = gsf_input_size (input);
	while (offset < length) {
		GsfInput *chunk;
		guint16 chunk_hdr;
		guint8 const *tmp;

		tmp = gsf_input_read (input, 2, NULL);
		if (!tmp)
			break;

		chunk_hdr = GSF_LE_GET_GUINT16 (tmp);
		offset += 2;

		if (0xB000 == (chunk_hdr&0xF000) && (chunk_hdr&0xFFF) > 0 && (length - offset < 4094)){
			if (length < offset + (chunk_hdr&0xFFF))
				break;
			chunk = gsf_input_proxy_new_section (input, offset, (gsf_off_t) (chunk_hdr&0xFFF) + 1);
			offset += (chunk_hdr&0xFFF) + 1;
		} else {
			if (length < offset + 4094){
				chunk = gsf_input_proxy_new_section (input, offset, length-offset);
				offset = length;
			} else {
				chunk = gsf_input_proxy_new_section (input, offset, 4094);
				offset += 4094;
			}
		}
		if (chunk) {
			GByteArray *tmpres = gsf_msole_inflate (chunk, 0);
			gsf_input_seek (input, offset, G_SEEK_CUR);
			g_byte_array_append (res, tmpres->data, tmpres->len);
			g_byte_array_free (tmpres, TRUE);
			g_object_unref (chunk);
		}
	}
	
	if (res == NULL)
		return NULL;
	if (add_null_terminator)
		g_byte_array_append (res, "", 1);
	*size = res->len;

	return g_byte_array_free (res, FALSE);
}

static void
vba_extract_module_source (GsfInfileMSVBA *vba, char const *name, guint32 src_offset)
{
	GsfInput *module;
	guint8 *code;
	int inflated_size;

	g_return_if_fail (name != NULL);

	module = gsf_infile_child_by_name (vba->source, name);
	if (module == NULL)
		return;
	code = gsf_vba_inflate (module, (gsf_off_t) src_offset, &inflated_size, FALSE);

	if (code != NULL) {
		if (NULL == vba->modules)
			vba->modules = g_hash_table_new_full (g_str_hash, g_str_equal,
				(GDestroyNotify)g_free, (GDestroyNotify)g_free);
		g_hash_table_insert (vba->modules, g_strdup (name), code);
	} else
		g_warning ("Problems extracting the source for %s @ %u", name, src_offset);

	g_object_unref (module);
}

/**
 * vba_dir_read:
 * @vba: #GsfInfileMSVBA
 * @err: (allow-none): place to store a #GError if anything goes wrong
 *
 * Read an VBA dirctory and its project file.
 * along the way.
 *
 * Returns: %FALSE on error setting @err if it is supplied.
 **/
static gboolean
vba_dir_read (GsfInfileMSVBA *vba, GError **err)
{
	int inflated_size, element_count = -1;
	char const *msg = NULL;
	char *name, *elem_stream = NULL;
	guint32 len;
	guint16 tag;
	guint8   *inflated_data, *end, *ptr;
	GsfInput *dir;
	gboolean  failed = TRUE;

	/* 0. get the stream */
	dir = gsf_infile_child_by_name (vba->source, "dir");
	if (dir == NULL) {
		msg = _("Can't find the VBA directory stream");
		goto fail_stream;
	}

	/* 1. decompress it */
	ptr = inflated_data = gsf_vba_inflate (dir, 0, &inflated_size, TRUE);
	if (inflated_data == NULL)
		goto fail_compression;
	end = inflated_data + inflated_size;

	/* 2. GUESS : based on several xls with macros and XL8GARY this looks like a
	 * series of sized records.  Be _extra_ careful */
	do {
		/* I have seen
		 * type		len	data
		 *  1		 4	 1 0 0 0
		 *  2		 4	 9 4 0 0
		 *  3		 2	 4 e4
		 *  4		<var>	 project name
		 *  5		 0
		 *  6		 0
		 *  7		 4
		 *  8		 4
		 *  0x3d	 0
		 *  0x40	 0
		 *  0x14	 4	 9 4 0 0
		 *
		 *  0x0f == number of elements
		 *  0x1c == (Size 0)
		 *  0x1e == (Size 4)
		 *  0x48 == (Size 0)
		 *  0x31 == stream offset of the compressed source !
		 *
		 *  0x16 == an ascii dependency name
		 *  0x3e == a unicode dependency name
		 *  0x33 == a classid for a dependency with no trialing data
		 *
		 *  0x2f == a dummy classid
		 *  0x30 == a classid
		 *  0x0d == the classid
		 *  0x2f, and 0x0d appear contain
		 * 	uint32 classid_size;
		 * 	<classid>
		 *	00 00 00 00 00 00
		 *	and sometimes some trailing junk
		 **/
		if ((ptr + 6) > end) {
			msg = _("vba project header problem");
			goto fail_content;
		}
		tag = GSF_LE_GET_GUINT16 (ptr);
		len = GSF_LE_GET_GUINT32 (ptr + 2);

		ptr += 6;
		if ((ptr + len) > end) {
			msg = _("vba project header problem");
			goto fail_content;
		}

		switch (tag) {
		case 4:
			name = g_strndup (ptr, len);
#ifdef OLD_VBA_DUMP
			puts ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
			printf ("<project name=\"%s\">", name);
#endif
			g_free (name);
			break;
		case 9:
			/* this seems to have an extra two bytes that are not
			 * part of the length ..?? */
			len += 2;
			break;
		case 0xf  :
			if (len != 2) {
				g_warning ("element count is not what we expected");
				break;
			}
			if (element_count >= 0) {
				g_warning ("More than one element count ??");
				break;
			}
			element_count = GSF_LE_GET_GUINT16 (ptr);
			break;

		/* dependencies */
		case 0x0d : break;
		case 0x2f : break;
		case 0x30 : break;
		case 0x33 : break;
		case 0x3e : break;
		case 0x16:
#if 0
			name = g_strndup (ptr, len);
			g_print ("Depend Name : '%s'\n", name);
			g_free (name);
#endif
			break;

		/* elements */
		case 0x47 : break;
		case 0x32 : break;
		case 0x1a:
#if 0
			name = g_strndup (ptr, len);
			g_print ("Element Name : '%s'\n", name);
			g_free (name);
#endif
			break;
		case 0x19:
			g_free (elem_stream);
			elem_stream = g_strndup (ptr, len);
			break;

		case 0x31:
			if (len != 4) {
				g_warning ("source offset property is not what we expected");
				break;
			}
			vba_extract_module_source (vba, elem_stream,
				GSF_LE_GET_GUINT32 (ptr));
			g_free (elem_stream); elem_stream = NULL;
			element_count--;
			break;

		default :
#if 0
			g_print ("tag %hx : len %u\n", tag, len);
			gsf_mem_dump (ptr, len);
#endif
			break;
		}

		ptr += len;
	} while (tag != 0x10);

	if (element_count != 0)
		g_warning ("Number of elements differs from expectations");

	failed = FALSE;

fail_content :
	g_free (inflated_data);
#ifdef OLD_VBA_DUMP
	puts ("</project>");
#endif

fail_compression :
	g_object_unref (dir);
fail_stream :

	g_free (elem_stream);

	if (failed) {
		if (err != NULL)
			*err = g_error_new_literal (gsf_input_error_id (), 0, msg);
		return FALSE;
	}

	return TRUE;
}

#define VBA56_DIRENT_RECORD_COUNT (2 + /* magic */		\
				   4 + /* version */		\
				   2 + /* 0x00 0xff */		\
				  22)  /* unknown */
#define VBA56_DIRENT_HEADER_SIZE (VBA56_DIRENT_RECORD_COUNT +	\
				  2 +  /* type1 record count */	\
				  2)   /* unknown */

#if 0
/**
 * vba_project_read:
 * @vba: #GsfInfileMSVBA
 * @err: (allow-none): place to store a #GError if anything goes wrong
 *
 * Read an VBA dirctory and its project file.
 * along the way.
 *
 * Returns: %FALSE on error setting @err if it is supplied.
 **/
static gboolean
vba_project_read (GsfInfileMSVBA *vba, GError **err)
{
	/* NOTE : This seems constant, find some confirmation */
	static guint8 const signature[]	  = { 0xcc, 0x61 };
	static struct {
		guint8 const signature[4];
		char const * const name;
		int const vba_version;
		gboolean const is_mac;
	} const  versions [] = {
		{ { 0x5e, 0x00, 0x00, 0x01 }, "Office 97",              5, FALSE},
		{ { 0x5f, 0x00, 0x00, 0x01 }, "Office 97 SR1",          5, FALSE },
		{ { 0x65, 0x00, 0x00, 0x01 }, "Office 2000 alpha?",     6, FALSE },
		{ { 0x6b, 0x00, 0x00, 0x01 }, "Office 2000 beta?",      6, FALSE },
		{ { 0x6d, 0x00, 0x00, 0x01 }, "Office 2000",            6, FALSE },
		{ { 0x6f, 0x00, 0x00, 0x01 }, "Office 2000",            6, FALSE },
		{ { 0x70, 0x00, 0x00, 0x01 }, "Office XP beta 1/2",     6, FALSE },
		{ { 0x73, 0x00, 0x00, 0x01 }, "Office XP",              6, FALSE },
		{ { 0x76, 0x00, 0x00, 0x01 }, "Office 2003",            6, FALSE },
		{ { 0x79, 0x00, 0x00, 0x01 }, "Office 2003",            6, FALSE },
		{ { 0x60, 0x00, 0x00, 0x0e }, "MacOffice 98",           5, TRUE },
		{ { 0x62, 0x00, 0x00, 0x0e }, "MacOffice 2001",         5, TRUE },
		{ { 0x63, 0x00, 0x00, 0x0e }, "MacOffice X",		6, TRUE },
		{ { 0x64, 0x00, 0x00, 0x0e }, "MacOffice 2004",         6, TRUE },
	};

	guint8 const *data;
	unsigned i, count, len;
	gunichar2 *uni_name;
	char *name;
	GsfInput *dir;

	dir = gsf_infile_child_by_name (vba->source, "dir");
	if (dir == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error_id (), 0,
					    _("Can't find the VBA directory stream"));
		return FALSE;
	}

	if (gsf_input_seek (dir, 0, G_SEEK_SET) ||
	    NULL == (data = gsf_input_read (dir, VBA56_DIRENT_HEADER_SIZE, NULL)) ||
	    0 != memcmp (data, signature, sizeof (signature))) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error_id (), 0,
					    _("No VBA signature"));
		return FALSE;
	}

	for (i = 0 ; i < G_N_ELEMENTS (versions); i++)
		if (!memcmp (data+2, versions[i].signature, 4))
			break;

	if (i >= G_N_ELEMENTS (versions)) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error_id (), 0,
					    _("Unknown VBA version signature 0x%x%x%x%x"),
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
#endif

static void
gsf_infile_msvba_finalize (GObject *obj)
{
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (obj);

	if (NULL != vba->modules) {
		g_hash_table_destroy (vba->modules);
		vba->modules = NULL;
	}
	if (vba->source != NULL) {
		g_object_unref (vba->source);
		vba->source = NULL;
	}
	parent_class->finalize (obj);
}

static void
gsf_infile_msvba_init (GObject *obj)
{
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (obj);

	vba->source		= NULL;
	vba->modules		= NULL;
	vba->children		= NULL;
}

static void
gsf_infile_msvba_class_init (GObjectClass *gobject_class)
{
	gobject_class->finalize		= gsf_infile_msvba_finalize;
	parent_class = g_type_class_peek_parent (gobject_class);
}

GSF_CLASS (GsfInfileMSVBA, gsf_infile_msvba,
	   gsf_infile_msvba_class_init, gsf_infile_msvba_init,
	   GSF_INFILE_TYPE)

GsfInfile *
gsf_infile_msvba_new (GsfInfile *source, GError **err)
{
	GsfInfileMSVBA *vba;

	g_return_val_if_fail (GSF_IS_INFILE (source), NULL);

	vba = g_object_new (GSF_INFILE_MSVBA_TYPE, NULL);
	vba->source = g_object_ref (source);

	/* vba_project_read (vba, err); */

	/* find the name offset pairs */
	if (vba_dir_read (vba, err))
		return GSF_INFILE (vba);

	if (err != NULL && *err == NULL)
		*err = g_error_new (gsf_input_error_id (), 0,
				    _("Unable to parse VBA header"));

	g_object_unref (vba);
	return NULL;
}

/**
 * gsf_infile_msvba_get_modules:
 * @vba_stream: #GsfInfile
 *
 * a collection of names and source code.
 *
 * Returns: (transfer none) (element-type utf8 gpointer) (nullable):
 * A #GHashTable of names and source code (unknown encoding).
 **/
GHashTable *
gsf_infile_msvba_get_modules (GsfInfileMSVBA const *vba_stream)
{
	g_return_val_if_fail (GSF_IS_INFILE_MSVBA (vba_stream), NULL);
	return vba_stream->modules;
}

/**
 * gsf_infile_msvba_steal_modules:
 * @vba_stream: #GsfInfile
 *
 * A collection of names and source code which the caller is responsible for destroying.
 *
 * Returns: (transfer full) (element-type utf8 gpointer) (nullable): A
 * #GHashTable of names and source code (unknown encoding).
 **/
GHashTable *
gsf_infile_msvba_steal_modules (GsfInfileMSVBA *vba_stream)
{
	GHashTable *res;
	g_return_val_if_fail (GSF_IS_INFILE_MSVBA (vba_stream), NULL);
	res = vba_stream->modules;
	vba_stream->modules = NULL;
	return res;
}

/**
 * gsf_input_find_vba:
 * @input: #GsfInput
 * @err: (nullable): #GError.
 *
 * A utility routine that attempts to find the VBA file withint a stream.
 *
 * Returns: (transfer full) (nullable): a GsfInfile
 **/
GsfInfileMSVBA *
gsf_input_find_vba (GsfInput *input, GError **err)
{
	GsfInput  *vba = NULL;
	GsfInfile *infile;

	if (NULL != (infile = gsf_infile_msole_new (input, NULL))) {
		/* 1) Try XLS */
		vba = gsf_infile_child_by_vname (infile, "_VBA_PROJECT_CUR", "VBA", NULL);
		/* 2) DOC */
		if (NULL == vba)
			vba = gsf_infile_child_by_vname (infile, "Macros", "VBA", NULL);

		/* TODO : PPT is more complex */

		g_object_unref (infile);
	} else if (NULL != (infile = gsf_infile_zip_new (input, NULL))) {
		GsfInput *main_part = gsf_open_pkg_open_rel_by_type (GSF_INPUT (infile),
			"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument",
		        NULL);

		if (NULL != main_part) {
			GsfInput *vba_stream = gsf_open_pkg_open_rel_by_type (main_part,
				"http://schemas.microsoft.com/office/2006/relationships/vbaProject",
			        NULL);
			if (NULL != vba_stream) {
				GsfInfile *ole = gsf_infile_msole_new (vba_stream, err);
				if (NULL != ole) {
					vba = gsf_infile_child_by_vname (ole, "VBA", NULL);
					g_object_unref (ole);
				}
				g_object_unref (vba_stream);
			}
			g_object_unref (main_part);
		}
		g_object_unref (infile);
	}

	if (NULL != vba)
	       return (GsfInfileMSVBA *)
			gsf_infile_msvba_new (GSF_INFILE (vba), err);
	return NULL;
}
