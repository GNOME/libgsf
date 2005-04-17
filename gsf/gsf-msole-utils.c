/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-msole-utils.c: 
 *
 * Copyright (C) 2002-2004 Jody Goldberg (jody@gnome.org)
 * Copyright (C) 2002-2003 Dom Lachowicz (cinamod@hotmail.com)
 * excel_iconv* family of functions (C) 2001 by Vlad Harchev <hvv@hippo.ru>
 * write support (C) 2005 by Fabalabs Software GmbH
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

#include <gsf-config.h>
#include <gsf/gsf-docprop-vector.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-input.h>
#include <gsf/gsf-output.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-timestamp.h>
#include <gsf/gsf-meta-names.h>
#include <stdio.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* #define NO_DEBUG_OLE_PROPS */
#undef NO_DEBUG_OLE_PROPS
#ifndef NO_DEBUG_OLE_PROPS
#define d(code)	do { code } while (0)
#else
#define d(code)
#endif

/*
 * The Format Identifier for Summary Information
 * F29F85E0-4FF9-1068-AB91-08002B27B3D9
 */
static guint8 const component_guid [] = {
	0xe0, 0x85, 0x9f, 0xf2, 0xf9, 0x4f, 0x68, 0x10,
	0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9
};

/*
 * The Format Identifier for Document Summary Information
 * D5CDD502-2E9C-101B-9397-08002B2CF9AE
 */
static guint8 const document_guid [] = {
	0x02, 0xd5, 0xcd, 0xd5, 0x9c, 0x2e, 0x1b, 0x10,
	0x93, 0x97, 0x08, 0x00, 0x2b, 0x2c, 0xf9, 0xae
};

/*
 * The Format Identifier for User-Defined Properties
 * D5CDD505-2E9C-101B-9397-08002B2CF9AE
 */
static guint8 const user_guid [] = {
	0x05, 0xd5, 0xcd, 0xd5, 0x9c, 0x2e, 0x1b, 0x10,
	0x93, 0x97, 0x08, 0x00, 0x2b, 0x2c, 0xf9, 0xae
};

typedef enum {
	GSF_MSOLE_META_DATA_COMPONENT,
	GSF_MSOLE_META_DATA_DOCUMENT,
	GSF_MSOLE_META_DATA_USER
} GsfMSOleMetaDataType;

typedef enum {
	VT_EMPTY	   = 0,
	VT_NULL		   = 1,
	VT_I2		   = 2,
	VT_I4		   = 3,
	VT_R4		   = 4,
	VT_R8		   = 5,
	VT_CY		   = 6,
	VT_DATE		   = 7,
	VT_BSTR		   = 8,
	VT_DISPATCH	   = 9,
	VT_ERROR	   = 10,
	VT_BOOL		   = 11,
	VT_VARIANT	   = 12,
	VT_UNKNOWN	   = 13,
	VT_DECIMAL	   = 14,
	VT_I1		   = 16,
	VT_UI1		   = 17,
	VT_UI2		   = 18,
	VT_UI4		   = 19,
	VT_I8		   = 20,
	VT_UI8		   = 21,
	VT_INT		   = 22,
	VT_UINT		   = 23,
	VT_VOID		   = 24,
	VT_HRESULT	   = 25,
	VT_PTR		   = 26,
	VT_SAFEARRAY	   = 27,
	VT_CARRAY	   = 28,
	VT_USERDEFINED	   = 29,
	VT_LPSTR	   = 30,
	VT_LPWSTR	   = 31,
	VT_FILETIME	   = 64,
	VT_BLOB		   = 65,
	VT_STREAM	   = 66,
	VT_STORAGE	   = 67,
	VT_STREAMED_OBJECT = 68,
	VT_STORED_OBJECT   = 69,
	VT_BLOB_OBJECT	   = 70,
	VT_CF		   = 71,
	VT_CLSID	   = 72,
	VT_VECTOR	   = 0x1000
} GsfMSOleVariantType;

typedef struct {
	char const *ms_name;
	char const *gsf_name;
	guint32	    id;
	GsfMSOleVariantType prefered_type;
} GsfMSOleMetaDataPropMap;

typedef struct {
	guint32		id;
	gsf_off_t	offset;
} GsfMSOleMetaDataProp;

typedef struct {
	GValue	        *value;
	char const	*dict_name;
	guint32		id;
	gsf_off_t	offset;
	GsfMSOleVariantType type;
} GsfMSOleMetaDataProp_real;

typedef struct {
	GsfMSOleMetaDataType type;
	gsf_off_t   offset;
	guint32	    size, num_props;
	GIConv	    iconv_handle;
	unsigned    char_size;
	GHashTable *dict;
} GsfMSOleMetaDataSection;

typedef struct {
	GsfMSOleMetaDataProp_real *props;
	guint32		udef_props;
	guint32		it;
	guint32		count;
	guint32		dict_count;
	gsf_off_t	offset;
	gsf_off_t	dict_offset;
} AddPropsStruct;

/*
 * DocumentSummaryInformation properties
 */
static GsfMSOleMetaDataPropMap const document_props[] = {
	{ "Category",		GSF_META_NAME_CATEGORY,            2,	VT_LPSTR },
	{ "PresentationFormat",	GSF_META_NAME_PRESENTATION_FORMAT, 3,	VT_LPSTR },
	{ "NumBytes",		GSF_META_NAME_BYTE_COUNT,          4,	VT_I4 },
	{ "NumLines",		GSF_META_NAME_LINE_COUNT,          5,	VT_I4 },
	{ "NumParagraphs",	GSF_META_NAME_PARAGRAPH_COUNT,     6,	VT_I4 },
	{ "NumSlides",		GSF_META_NAME_SLIDE_COUNT,         7,	VT_I4 },
	{ "NumNotes",		GSF_META_NAME_NOTE_COUNT,          8,	VT_I4 },
	{ "NumHiddenSlides",	GSF_META_NAME_HIDDEN_SLIDE_COUNT,  9,	VT_I4 },
	{ "NumMMClips",		GSF_META_NAME_MM_CLIP_COUNT,       10,	VT_I4 },
	{ "Scale",		GSF_META_NAME_SCALE,               11,	VT_BOOL },
	{ "HeadingPairs",	GSF_META_NAME_HEADING_PAIRS,       12,	VT_VECTOR | VT_VARIANT },
	{ "DocumentParts",	GSF_META_NAME_DOCUMENT_PARTS,      13,	VT_VECTOR | VT_LPSTR },
	{ "Manager",		GSF_META_NAME_MANAGER,             14,	VT_LPSTR },
	{ "Company",		GSF_META_NAME_COMPANY,             15,	VT_LPSTR },
	{ "LinksDirty",		GSF_META_NAME_LINKS_DIRTY,         16,	VT_BOOL },
	{ "DocSumInfo_17",      GSF_META_NAME_USER_DEFINED_1,      17,	VT_UNKNOWN },
	{ "DocSumInfo_18",      GSF_META_NAME_USER_DEFINED_2,      18,	VT_UNKNOWN },
	{ "DocSumInfo_19",      GSF_META_NAME_USER_DEFINED_3,      19,	VT_BOOL },
	{ "DocSumInfo_20",      GSF_META_NAME_USER_DEFINED_4,      20,	VT_UNKNOWN },
	{ "DocSumInfo_21",      GSF_META_NAME_USER_DEFINED_5,      21,	VT_UNKNOWN },
	{ "DocSumInfo_22",      GSF_META_NAME_USER_DEFINED_6,      22,	VT_BOOL },
	{ "DocSumInfo_23",      GSF_META_NAME_USER_DEFINED_7,      23,	VT_I4 }
};

/*
 * SummaryInformation properties
 */
static GsfMSOleMetaDataPropMap const component_props[] = {
	{ "Title",		GSF_META_NAME_TITLE,		2,	VT_LPSTR },
	{ "Subject",		GSF_META_NAME_SUBJECT,		3,	VT_LPSTR },
	{ "Author",		GSF_META_NAME_CREATOR,		4,	VT_LPSTR },
	{ "Keywords",		GSF_META_NAME_KEYWORDS,		5,	VT_LPSTR },
	{ "Comments",		GSF_META_NAME_DESCRIPTION,	6,	VT_LPSTR },
	{ "Template",		GSF_META_NAME_TEMPLATE,		7,	VT_LPSTR },
	{ "LastSavedBy",	GSF_META_NAME_LAST_SAVED_BY,	8,	VT_LPSTR },
	{ "RevisionNumber",	GSF_META_NAME_REVISION_COUNT,	9,	VT_LPSTR },
	{ "TotalEditingTime",	GSF_META_NAME_EDITING_DURATION,	10,	VT_FILETIME },
	{ "LastPrinted",	GSF_META_NAME_LAST_PRINTED,	11,	VT_FILETIME },
	{ "CreateTime",		GSF_META_NAME_DATE_CREATED,	12,	VT_FILETIME },
	{ "LastSavedTime",	GSF_META_NAME_DATE_MODIFIED,	13,	VT_FILETIME },
	{ "NumPages",		GSF_META_NAME_PAGE_COUNT,	14,	VT_I4 },
	{ "NumWords",		GSF_META_NAME_WORD_COUNT,	15,	VT_I4 },
	{ "NumCharacters",	GSF_META_NAME_CHARACTER_COUNT,	16,	VT_I4 },
	{ "Thumbnail",		GSF_META_NAME_THUMBNAIL,	17,	VT_CF },
	{ "AppName",		GSF_META_NAME_GENERATOR,	18,	VT_LPSTR },
	{ "Security",		GSF_META_NAME_SECURITY,		19,	VT_I4 }
};

static GsfMSOleMetaDataPropMap const common_props[] = {
	{ "Dictionary",		  GSF_META_NAME_DICTIONARY,            0,	   0, /* magic */},
	{ "CodePage",		  GSF_META_NAME_LANGUAGE,              1,	   VT_I2 },
	{ "LOCALE_SYSTEM_DEFAULT",GSF_META_NAME_LOCALE_SYSTEM_DEFAULT, 0x80000000, VT_UI4},
	{ "CASE_SENSITIVE",	  GSF_META_NAME_CASE_SENSITIVE,        0x80000003, VT_UI4},
};

static GHashTable *name_to_prop_hash = NULL;

static char const *
msole_prop_id_to_gsf (GsfMSOleMetaDataSection *section, guint32 id, gboolean *linked)
{
	char const *res = NULL;
	GsfMSOleMetaDataPropMap const *map = NULL;
	unsigned i = 0;

	*linked = FALSE;
	if (section->dict != NULL) {
		if (id & 0x1000000) {
			*linked = TRUE;
			id &= ~0x1000000;
			d (printf ("LINKED "););
		}

		res = g_hash_table_lookup (section->dict, GINT_TO_POINTER (id));

		if (res != NULL) {
			d (printf (res););
			return res;
		}
	}

	if (section->type == GSF_MSOLE_META_DATA_COMPONENT) {
		map = component_props;
		i = G_N_ELEMENTS (component_props);
	} else if (section->type == GSF_MSOLE_META_DATA_DOCUMENT) {
		map = document_props;
		i = G_N_ELEMENTS (document_props);
	}
	while (i-- > 0)
		if (map[i].id == id) {
			d (printf (map[i].gsf_name););
			return map[i].gsf_name;
		}

	map = common_props;
	i = G_N_ELEMENTS (common_props);
	while (i-- > 0)
		if (map[i].id == id) {
			d (printf (map[i].gsf_name););
			return map[i].gsf_name;
		}

	d (printf ("_UNKNOWN_(0x%x %d)", id, id););

	return NULL;
}

static GsfMSOleMetaDataPropMap const *
msole_gsf_name_to_prop (char const *name)
{
	if (NULL == name_to_prop_hash) {
		int i;
		GsfMSOleMetaDataPropMap const *map;

		name_to_prop_hash = g_hash_table_new (g_str_hash, g_str_equal);

		for (i = G_N_ELEMENTS (map = component_props); i-- > 0; )
			g_hash_table_replace (name_to_prop_hash,
				(gpointer) map[i].gsf_name, (gpointer) (map+i));
		for (i = G_N_ELEMENTS (map = document_props); i-- > 0; )
			g_hash_table_replace (name_to_prop_hash,
				(gpointer) map[i].gsf_name, (gpointer) (map+i));
		for (i = G_N_ELEMENTS (map = common_props); i-- > 0; )
			g_hash_table_replace (name_to_prop_hash,
				(gpointer) map[i].gsf_name, (gpointer) (map+i));
	}

	return g_hash_table_lookup (name_to_prop_hash, (gpointer)name);
}

static GValue *
msole_prop_parse (GsfMSOleMetaDataSection *section,
		  guint32 type, guint8 const **data, guint8 const *data_end)
{
	GValue *res;
	char *str;
	guint32 len;
	gsize gslen;
	gboolean const is_vector = type & VT_VECTOR;

	g_return_val_if_fail (!(type & (unsigned)(~0x1fff)), NULL); /* not valid in a prop set */

	type &= 0xfff;

	if (is_vector) {
		/*
		 *  A vector is basically an array.  If the type associated with
		 *  it is a variant, then each element can have a different
		 *  variant type.  Otherwise, each element has the same variant
		 *  type associated with the vector.
		 */
		unsigned i, n;
		GsfDocPropVector *vector;

		g_return_val_if_fail (*data + 4 <= data_end, NULL);

		n = GSF_LE_GET_GUINT32 (*data);
		*data += 4;

		d (printf (" array with %d elem\n", n);
		   gsf_mem_dump (*data, (unsigned)(data_end - *data)););
		
		vector = gsf_docprop_vector_new ();

		for (i = 0 ; i < n ; i++) {
			GValue *v;
			d (printf ("\t[%d] ", i););
			v = msole_prop_parse (section, type, data, data_end);
			if (v) {
				if (G_IS_VALUE (v)) {
					gsf_docprop_vector_append (vector, v);
					g_value_unset (v);
				}
				g_free (v);
			}
		}

		res = g_new0 (GValue, 1);
		g_value_init (res, GSF_DOCPROP_VECTOR_TYPE);
		gsf_value_set_docprop_vector (res, vector);
		return res;
	}

	res = g_new0 (GValue, 1);
	switch (type) {
	case VT_EMPTY :		 d (puts ("VT_EMPTY"););
		/*
		 * A property with a type indicator of VT_EMPTY has no data
		 * associated with it; that is, the size of the value is zero.
		 */
		/* value::unset == empty */
		break;

	case VT_NULL :		 d (puts ("VT_NULL"););
		/* This is like a pointer to NULL */
		/* value::unset == null too :-) do we need to distinguish ? */
		break;

	case VT_I2 :		 d (puts ("VT_I2"););
		/* 2-byte signed integer */
		g_return_val_if_fail (*data + 2 <= data_end, NULL);
		g_value_init (res, G_TYPE_INT);
		g_value_set_int	(res, GSF_LE_GET_GINT16 (*data));
		*data += 2;
		break;

	case VT_I4 :		 d (puts ("VT_I4"););
		/* 4-byte signed integer */
		g_return_val_if_fail (*data + 4 <= data_end, NULL);
		g_value_init (res, G_TYPE_INT);
		g_value_set_int	(res, GSF_LE_GET_GINT32 (*data));
		*data += 4;
		break;

	case VT_R4 :		 d (puts ("VT_R4"););
		/* 32-bit IEEE floating-point value */
		g_return_val_if_fail (*data + 4 <= data_end, NULL);
		g_value_init (res, G_TYPE_FLOAT);
		g_value_set_float (res, GSF_LE_GET_FLOAT (*data));
		*data += 4;
		break;

	case VT_R8 :		 d (puts ("VT_R8"););
		/* 64-bit IEEE floating-point value */
		g_return_val_if_fail (*data + 8 <= data_end, NULL);
		g_value_init (res, G_TYPE_DOUBLE);
		g_value_set_double (res, GSF_LE_GET_DOUBLE (*data));
		*data += 8;
		break;

	case VT_CY :		 d (puts ("VT_CY"););
		/* 8-byte two's complement integer (scaled by 10,000) */
#warning TODO
		break;

	case VT_DATE :		 d (puts ("VT_DATE"););
		/* 
		 * 64-bit floating-point number representing the number of days
		 * (not seconds) since December 31, 1899.
		 */
#warning TODO
		break;

	case VT_BSTR :		 d (puts ("VT_BSTR"););
		/*
		 * Pointer to null-terminated Unicode string; the string is pre-
		 * ceeded by a DWORD representing the byte count of the number
		 * of bytes in the string (including the  terminating null).
		 */
#warning TODO
		break;

	case VT_DISPATCH :	 d (puts ("VT_DISPATCH"););
#warning TODO
		break;

	case VT_BOOL :		 d (puts ("VT_BOOL"););
		/* A boolean (WORD) value containg 0 (false) or -1 (true). */
		g_return_val_if_fail (*data + 1 <= data_end, NULL);
		g_value_init (res, G_TYPE_BOOLEAN);
		g_value_set_boolean (res, **data ? TRUE : FALSE);
		*data += 1;
		break;

	case VT_VARIANT :	 d (printf ("VT_VARIANT containing a "););
		/*
		 * A type indicator (a DWORD) followed by the corresponding
		 *  value.  VT_VARIANT is only used in conjunction with
		 *  VT_VECTOR.
		 */
		g_free (res);
		type = GSF_LE_GET_GUINT32 (*data);
		*data += 4;
		return msole_prop_parse (section, type, data, data_end);

	case VT_UI1 :		 d (puts ("VT_UI1"););
		/* 1-byte unsigned integer */
		g_return_val_if_fail (*data + 1 <= data_end, NULL);
		g_value_init (res, G_TYPE_UCHAR);
		g_value_set_uchar (res, (guchar)(**data));
		*data += 1;
		break;

	case VT_UI2 :		 d (puts ("VT_UI2"););
		/* 2-byte unsigned integer */
		g_return_val_if_fail (*data + 2 <= data_end, NULL);
		g_value_init (res, G_TYPE_UINT);
		g_value_set_uint (res, GSF_LE_GET_GUINT16 (*data));
		*data += 2;
		break;

	case VT_UI4 :		 d (puts ("VT_UI4"););
		/* 4-type unsigned integer */
		g_return_val_if_fail (*data + 4 <= data_end, NULL);
		g_value_init (res, G_TYPE_UINT);
		*data += 4;
		d (printf ("%u\n", GSF_LE_GET_GUINT32 (*data)););
		break;

	case VT_I8 :		 d (puts ("VT_I8"););
		/* 8-byte signed integer */
		g_return_val_if_fail (*data + 8 <= data_end, NULL);
		g_value_init (res, G_TYPE_INT64);
		*data += 8;
		break;

	case VT_UI8 :		 d (puts ("VT_UI8"););
		/* 8-byte unsigned integer */
		g_return_val_if_fail (*data + 8 <= data_end, NULL);
		g_value_init (res, G_TYPE_UINT64);
		*data += 8;
		break;

	case VT_LPSTR :		 d (puts ("VT_LPSTR"););
		/* 
		 * This is the representation of many strings.  It is stored in
		 * the same representation as VT_BSTR.  Note that the serialized
		 * representation of VP_LPSTR has a preceding byte count, wheras
		 * the in-memory representation does not.
		 */
		/* be anal and safe */
		g_return_val_if_fail (*data + 4 <= data_end, NULL);

		len = GSF_LE_GET_GUINT32 (*data);

		g_return_val_if_fail (len < 0x10000, NULL);
		g_return_val_if_fail (*data + 4 + len*section->char_size <= data_end, NULL);

		gslen = 0;
		str = g_convert_with_iconv (*data + 4,
			len * section->char_size,
			section->iconv_handle, &gslen, NULL, NULL);
		len = (guint32)gslen;

		g_value_init (res, G_TYPE_STRING);
		g_value_set_string (res, str);
		g_free (str);
		*data += 4 + len;
		break;

	case VT_LPWSTR : d (puts ("VT_LPWSTR"););
		/*
		 * A counted and null-terminated Unicode string; a DWORD character
		 * count (where the count includes the terminating null) followed
		 * by that many Unicode (16-bit) characters.  Note that the count
		 * is character count, not byte count.
		 */
		/* be anal and safe */
		g_return_val_if_fail (*data + 4 <= data_end, NULL);

		len = GSF_LE_GET_GUINT32 (*data);

		g_return_val_if_fail (len < 0x10000, NULL);
		g_return_val_if_fail (*data + 4 + len <= data_end, NULL);

		str = g_convert (*data + 4, len*2,
				 "UTF-8", "UTF-16LE", &gslen, NULL, NULL);
		len = (guint32)gslen;

		g_value_init (res, G_TYPE_STRING);
		g_value_set_string (res, str);
		g_free (str);
		*data += 4 + len;
		break;

	case VT_FILETIME :	 d (puts ("VT_FILETIME"););
		/* 64-bit FILETIME structure, as defined by Win32. */
		g_return_val_if_fail (*data + 8 <= data_end, NULL);
		g_value_init (res, GSF_TIMESTAMP_TYPE);
	{
		/* ft * 100ns since Jan 1 1601 */
		guint64 ft = GSF_LE_GET_GUINT64 (*data);
		GsfTimestamp ts;

		ft /= 10000000; /* convert to seconds */
#ifdef _MSC_VER
		ft -= 11644473600i64; /* move to Jan 1 1970 */
#else
		ft -= 11644473600ULL; /* move to Jan 1 1970 */
#endif
		ts.timet = (time_t)ft;
		gsf_value_set_timestamp (res, &ts);
		*data += 8;
		break;
	}
	case VT_BLOB :		 d (puts ("VT_BLOB"););
		/*
		 * A DWORD count of bytes, followed by that many bytes of data.
		 * The byte count does not include the four bytes for the length
		 * of the count itself:  An empty blob would have a count of
		 * zero, followed by zero bytes.  Thus the serialized represen-
		 * tation of a VT_BLOB is similar to that of a VT_BSTR but does
		 * not guarantee a null byte at the end of the data.
		 */
#warning TODO
		g_free (res);
		res = NULL;
		break;

	case VT_STREAM :	 d (puts ("VT_STREAM"););
		/*
		 * Indicates the value is stored in a stream that is sibling to
		 * the CONTENTS stream.  Following this type indicator is data
		 * in the format of a serialized VT_LPSTR, which names the stream
		 * containing the data.
		 */
#warning TODO
		g_free (res);
		res = NULL;
		break;

	case VT_STORAGE :	 d (puts ("VT_STORAGE"););
		/*
		 * Indicates the value is stored in an IStorage that is sibling
		 * to the CONTENTS stream.  Following this type indicator is data
		 * in the format of a serialized VT_LPSTR, which names the
		 * IStorage containing the data.
		 */
#warning TODO
		g_free (res);
		res = NULL;
		break;

	case VT_STREAMED_OBJECT: d (puts ("VT_STREAMED_OBJECT"););
		/*
		 * Same as VT_STREAM, but indicates that the stream contains a
		 * serialized object, which is a class ID followed by initiali-
		 * zation data for the class.
		 */
#warning TODO
		g_free (res);
		res = NULL;
		break;

	case VT_STORED_OBJECT :	 d (puts ("VT_STORED_OBJECT"););
		/*
		 * Same as VT_STORAGE, but indicates that the designated IStorage
		 * contains a loadable object.
		 */
#warning TODO
		g_free (res);
		res = NULL;
		break;

	case VT_BLOB_OBJECT :	 d (puts ("VT_BLOB_OBJECT"););
		/*
		 * Contains a serialized object in the same representation as
		 * would appear in a VT_STREAMED_OBJECT.  That is, following the
		 * VT_BLOB_OBJECT tag is a DWORD byte count of the remaining data
		 * (where the byte count does not include the size of itself)
		 * which is in the format of a class ID followed by initialization
		 * data for that class
		 */
#warning TODO
		g_free (res);
		res = NULL;
		break;

	case VT_CF :		 d (puts ("VT_CF"););
		/*
		 * a BLOB containing a clipboard format identifier followed by
		 * the data in that format.  That is, following the VT_CF tag is
		 * data in the format of a VT_BLOB: a CWORD count of bytes,
		 * followed by that many bytes of data in the format of a packed
		 * VTCFREP, followed immediately by an array of bytes as appropriate
		 * for data in the clipboard format.
		 */
#warning TODO
		g_free (res);
		res = NULL;
		break;

	case VT_CLSID :		 d (puts ("VT_CLSID"););
		/* A class ID (or other GUID) */
		*data += 16;
		g_free (res);
		res = NULL;
		break;

	case VT_ERROR :
		/* A DWORD containing a status code. */
	case VT_UNKNOWN :
	case VT_DECIMAL :
	case VT_I1 :
		/* 1-byte signed integer */
	case VT_INT :
	case VT_UINT :
	case VT_VOID :
	case VT_HRESULT :
	case VT_PTR :
	case VT_SAFEARRAY :
	case VT_CARRAY :
	case VT_USERDEFINED :
		g_warning ("type %d (0x%x) is not permitted in property sets",
			   type, type);
		g_free (res);
		res = NULL;
		break;

	default :
		g_warning ("Unknown property type %d (0x%x)", type, type);
		g_free (res);
		res = NULL;
	}

	if (res != NULL && G_IS_VALUE (res)) {
		d ( {
			char *val = g_strdup_value_contents (res);
			printf ("%s\n", val);
			g_free (val);
		});
	} else {
#if 0 /* enable after release */
		puts ("<unparsed>\n");
#endif
	}
	return res;
}

static gboolean
msole_prop_read (GsfInput *in,
		 GsfMSOleMetaDataSection *section,
		 GsfMSOleMetaDataProp    *props,
		 unsigned		  i,
		 GsfDocMetaData		 *accum)
{
	guint32 type;
	guint8 const *data;
	/* FIXME : why size-4 ? I must be missing something */
	gsf_off_t size = ((i+1) >= section->num_props)
		? section->size-4 : props[i+1].offset;
	char   *name;
	GValue *val;

	g_return_val_if_fail (i < section->num_props, FALSE);
	g_return_val_if_fail (size >= props[i].offset + 4, FALSE);

	size -= props[i].offset; /* includes the type id */
	if (gsf_input_seek (in, section->offset+props[i].offset, G_SEEK_SET) ||
	    NULL == (data = gsf_input_read (in, size, NULL))) {
		g_warning ("failed to read prop #%d", i);
		return FALSE;
	}

	type = GSF_LE_GET_GUINT32 (data);
	data += 4;

	/* dictionary is magic */
	if (props[i].id == 0) {
		guint32 len, id, i, n;
		gsize gslen;
		char *name;
		guint8 const *start = data;

		g_return_val_if_fail (section->dict == NULL, FALSE);

		section->dict = g_hash_table_new_full (
			g_direct_hash, g_direct_equal,
			NULL, g_free);

		d ({ g_print ("Dictionary = \n"); gsf_mem_dump (data-4, size); });
		n = type;
		for (i = 0 ; i < n ; i++) {
			id = GSF_LE_GET_GUINT32 (data);
			len = GSF_LE_GET_GUINT32 (data + 4);

			g_return_val_if_fail (len < 0x10000, FALSE);

			gslen = 0;
			name = g_convert_with_iconv (data + 8,
				len * section->char_size,
				section->iconv_handle, &gslen, NULL, NULL);
			len = (guint32)gslen;
			data += 8 + len;

			d (printf ("\t%u == %s\n", id, name););
			g_hash_table_replace (section->dict,
				GINT_TO_POINTER (id), name);

			/* MS documentation blows goats !
			 * The docs claim there are padding bytes in the dictionary.
			 * Their examples show padding bytes.
			 * In reality non-unicode strings do not see to have padding.
			 */
			if (section->char_size != 1 && (data - start) % 4)
				data += 4 - ((data - start) % 4);
		}
	} else {
		gboolean linked;
		d (printf ("%u) ", i););
		name = g_strdup (msole_prop_id_to_gsf (section, props[i].id, &linked));
		d (printf (" @ %x %x = ", (unsigned)props[i].offset, (unsigned)size););
		val = msole_prop_parse (section, type, &data, data + size);

		if (NULL != name && NULL != val) {
			if (linked) {
				GsfDocProp *prop = gsf_doc_meta_data_lookup (accum, name);
				if (NULL == prop) {
					g_warning ("linking property '%s' before it\'s value is specified",
						   (name ? name : "<null>"));
				} else if (!G_VALUE_HOLDS_STRING (val)) {
					g_warning ("linking property '%s' before it\'s value is specified",
						   (name ? name : "<null>"));
				} else
					gsf_doc_prop_set_link (prop,
						g_value_dup_string (val));
			} else {
				gsf_doc_meta_data_insert (accum, name, val);
				val = NULL;
				name = NULL;
			}
		}

		if (NULL != val) {
			if (G_IS_VALUE (val))
				g_value_unset (val);
			g_free (val);
		}
		g_free (name);
	}

	return TRUE;
}

static int
msole_prop_cmp (gconstpointer a, gconstpointer b)
{
	GsfMSOleMetaDataProp const *prop_a = a ;
	GsfMSOleMetaDataProp const *prop_b = b ;
	return prop_a->offset - prop_b->offset;
}

/**
 * gsf_msole_metadata_read :
 * @in    : #GsfInput
 * @accum : #GsfDocMetaData
 *
 * Read a stream formated as a set of MS OLE properties from @in and store the
 * results in @accum.
 *
 * Returns GError which the caller must free on error.
 **/
GError *
gsf_msole_metadata_read	(GsfInput *in, GsfDocMetaData *accum)
{
	guint8 const *data = gsf_input_read (in, 28, NULL);
	guint16 version;
	guint32 os, num_sections;
	unsigned i, j;
	GsfMSOleMetaDataSection *sections;
	GsfMSOleMetaDataProp	*props;
	GsfDocProp		*prop;
	
	if (NULL == data)
		return g_error_new (gsf_input_error_id (), 0,
			"Unable to read MS property stream header");

	/*
	 * Validate the Property Set Header.
	 * Format (bytes) :
	 *   00 - 01	Byte order		0xfffe
	 *   02 - 03	Format			0
	 *   04 - 05	OS Version		high word is the OS
	 *   06 - 07				low  word is the OS version
	 *					  0 = win16
	 *					  1 = mac
	 *					  2 = win32
	 *   08 - 23	Class Identifier	Usually Format ID
	 *   24 - 27	Section count		Should be at least 1
	 */
	os	     = GSF_LE_GET_GUINT16 (data + 6);
	version	     = GSF_LE_GET_GUINT16 (data + 2);
	num_sections = GSF_LE_GET_GUINT32 (data + 24);
	if (GSF_LE_GET_GUINT16 (data + 0) != 0xfffe
	    || (version != 0 && version != 1)
	    || os > 2
	    || num_sections > 100) /* arbitrary sanity check */
		return g_error_new (gsf_input_error_id (), 0,
			"Invalid MS property stream header");

	/* extract the section info */
	/*
	 * The Format ID/Offset list follows.
	 * Format:
	 *   00 - 16	Section Name		Format ID
	 *   16 - 19	Section Offset		The offset is the number of
	 *					bytes from the start of the
	 *					whole stream to where the
	 *					section begins.
	 */
	sections = (GsfMSOleMetaDataSection *)g_alloca (sizeof (GsfMSOleMetaDataSection)* num_sections);
	for (i = 0 ; i < num_sections ; i++) {
		data = gsf_input_read (in, 20, NULL);
		if (NULL == data)
			return g_error_new (gsf_input_error_id (), 0,
				"Unable to read MS property stream header");
		if (!memcmp (data, component_guid, sizeof (component_guid)))
			sections [i].type = GSF_MSOLE_META_DATA_COMPONENT;
		else if (!memcmp (data, document_guid, sizeof (document_guid)))
			sections [i].type = GSF_MSOLE_META_DATA_DOCUMENT;
		else if (!memcmp (data, user_guid, sizeof (user_guid)))
			sections [i].type = GSF_MSOLE_META_DATA_USER;
		else {
			sections [i].type = GSF_MSOLE_META_DATA_USER;
			g_warning ("Unknown property section type, treating it as USER");
			gsf_mem_dump (data, 16);
		}

		sections [i].offset = GSF_LE_GET_GUINT32 (data + 16);
		d (printf ("0x%x\n", (guint32)sections [i].offset););
	}

	/*
	 * A section is the third part of the property set stream.
	 * Format (bytes) :
	 *   00 - 03	Section size	A byte count for the section (which is inclusive
	 *				of the byte count itself and should always be a
	 *				multiple of 4);
	 *   04 - 07	Property count	A count of the number of properties
	 *   08 - xx   			An array of 32-bit Property ID/Offset pairs
	 *   yy - zz			An array of Property Type indicators/Value pairs
	 */
	for (i = 0 ; i < num_sections ; i++) {
		if (gsf_input_seek (in, sections[i].offset, G_SEEK_SET) ||
		    NULL == (data = gsf_input_read (in, 8, NULL)))
			return g_error_new (gsf_input_error_id (), 0,
				"Invalid MS property section");

		sections[i].iconv_handle = (GIConv)-1;
		sections[i].char_size    = 1;
		sections[i].dict      = NULL;
		sections[i].size      = GSF_LE_GET_GUINT32 (data); /* includes header */
		sections[i].num_props = GSF_LE_GET_GUINT32 (data + 4);
		if (sections[i].num_props <= 0)
			continue;

		/*
		 * Get and save all the Property ID/Offset pairs.
		 * Format (bytes) :
		 *   00 - 03	id	Property ID
		 *   04 - 07	offset	The distance from the start of the section to the
		 *			start of the Property Type/Value pair.
		 */
		props = g_new (GsfMSOleMetaDataProp, sections[i].num_props);
		for (j = 0; j < sections[i].num_props; j++) {
			if (NULL == (data = gsf_input_read (in, 8, NULL))) {
				g_free (props);
				return g_error_new (gsf_input_error_id (), 0,
					"Invalid MS property section");
			}

			props [j].id = GSF_LE_GET_GUINT32 (data);
			props [j].offset  = GSF_LE_GET_GUINT32 (data + 4);
		}

		/* order prop info by offset to facilitate bounds checking */
		qsort (props, sections[i].num_props,
		       sizeof (GsfMSOleMetaDataProp),
		       msole_prop_cmp);

		/*
		 * Find and process the code page.
		 * Property ID 1 is reserved as an indicator of the code page.
		 */
		sections[i].iconv_handle = (GIConv)-1;
		sections[i].char_size = 1;
		for (j = 0; j < sections[i].num_props; j++) /* first codepage */
			if (props[j].id == 1) {
				msole_prop_read (in, sections+i, props, j, accum);
				if (NULL != (prop = gsf_doc_meta_data_lookup (accum, GSF_META_NAME_LANGUAGE))) {
					GValue const *val = gsf_doc_prop_get_val (prop);
					if (NULL != val && G_VALUE_HOLDS_INT (val)) {
						int codepage = g_value_get_int (val);
						sections[i].iconv_handle =
							gsf_msole_iconv_open_for_import (codepage);
						if (codepage == 1200 || codepage == 1201)
							sections[i].char_size = 2;
					}
				}
			}

		if (sections[i].iconv_handle == (GIConv)-1)
			sections[i].iconv_handle = gsf_msole_iconv_open_for_import (1252);

		/*
		 * Find and process the Property Set Dictionary
		 * Property ID 0 is reserved as an indicator of the dictionary.
		 * For User Defined Sections, Property ID 0 is NOT a dictionary.
		 */
		for (j = 0; j < sections[i].num_props; j++) /* then dictionary */
			if (props[j].id == 0)
				msole_prop_read (in, sections+i, props, j, accum);

		/* Process all the properties */
		for (j = 0; j < sections[i].num_props; j++) /* the rest */
			if (props[j].id > 1)
				msole_prop_read (in, sections+i, props, j, accum);

		gsf_iconv_close (sections[i].iconv_handle);
		g_free (props);
		if (sections[i].dict != NULL)
			g_hash_table_destroy (sections[i].dict);
	}
	return NULL;
}

static void
add_props (char const *name, GValue *value, AddPropsStruct *user_data)
{
	gboolean  error = FALSE;
	GValue	 *tmp = NULL;
	guint	  i, vector_num_values = 1;
	gsf_off_t offset;
	guint32			       it = user_data->it;
	GsfMSOleMetaDataProp_real     *props = user_data->props + it;
	GsfMSOleMetaDataPropMap const *map = msole_gsf_name_to_prop (name);
	GValueArray		      *vector = NULL;

	/* Do not write codepage or dictionary */
	if (map != NULL && (map->id == 0 || map->id == 1))
		return;

	/* allocate predefined ids or add it to the dictionary */
	if (NULL != map) {
		props->dict_name = NULL;
		props->id = map->id;
		user_data->count++;
		offset = user_data->offset;
	} else {
		props->dict_name = name;
		props->id = user_data->dict_count + user_data->udef_props;
		user_data->dict_count++;
		offset = user_data->dict_offset;
		d (g_warning("name not found (%s) - generate an id (=%d)", (char *)name, props->id););
	}
	props->value = value;
	it++;

	/* calculate offset */
	props->offset = offset;

	/* we have an vector */
	if (IS_GSF_DOCPROP_VECTOR (value)) {
		vector = gsf_value_get_docprop_varray (value);
		vector_num_values = vector->n_values;
	}

	for (i=0; i < vector_num_values; i++) {
		if (vector != NULL)
			value = g_value_array_get_nth (vector, i);

		switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (value))) {
		case G_TYPE_BOOLEAN:
			props->type = VT_BOOL;
			offset += 1;
			offset += 3; /* 3x \0 */
			break;

		case G_TYPE_UCHAR:
			props->type = VT_UI1;
			offset += 1;
			break;

		case G_TYPE_INT:
			switch (map->prefered_type) {
			case VT_I2:
				props->type = VT_I2;
				offset += 2;
				break;

			case VT_I4:
				props->type = VT_I4;
				offset += 4;
				break;

			default:
				props->type = VT_I4;
				offset += 4;
				break;
			}
			break;

		case G_TYPE_UINT:
			switch (map->prefered_type) {
			case VT_UI2:
				props->type = VT_UI2;
				offset += 2;
				break;

			case VT_UI4:
				props->type = VT_UI4;
				offset += 4;
				break;

			default:
				props->type = VT_UI4;
				offset += 4;
				break;
			}
			break;

		case G_TYPE_FLOAT:
			props->type = VT_R4;
			offset += 4;
			break;

		case G_TYPE_DOUBLE:
			props->type = VT_R8;
			offset += 8;
			break;

		case G_TYPE_STRING:
			props->type = VT_LPSTR;
			offset += 4; /* length of string */
			offset += strlen ((char *)g_value_get_string (value)) + 1; /* \0 */
			break;

		case G_TYPE_BOXED:
			props->type = VT_FILETIME;
			offset += 8;
			break;

		case G_TYPE_POINTER:
			tmp = (GValue *)g_value_get_pointer(value);
			props->type = map->prefered_type;
			offset += g_value_get_uint (&tmp[0]) - 1 /* \0 */ - 4 /* type definition */;
			break;

		default:
			g_warning ("Unknown property type for property: %s", (char *)name);
			error = TRUE;
			break;
		}
	}

	if (error) {
		/* if error occured, count backwards */
		g_warning("leaving this property out");
		if (props->id >= user_data->udef_props)
			user_data->dict_count--;
		else
			user_data->count--;
		it--;
	}
	else {
		offset += sizeof(guint32); /* sizeof type definition */

		if (vector != NULL) {
			props->type = VT_VECTOR;
			offset += sizeof(guint32); /* sizeof count */

			if (!memcmp(name, GSF_META_NAME_DOCUMENT_PARTS, sizeof(GSF_META_NAME_DOCUMENT_PARTS)))
			      props->type = VT_VECTOR | VT_LPSTR;
			else if (!memcmp(name, GSF_META_NAME_HEADING_PAIRS, sizeof(GSF_META_NAME_HEADING_PAIRS))) {
			      offset += sizeof(guint32) * vector_num_values; /* type definition of variant */
			      props->type = VT_VECTOR | VT_VARIANT;
			}
		}

		/* save offset in struct */
		if (props->id >= user_data->udef_props)
			user_data->dict_offset = offset;
		else
			user_data->offset = offset;
	}

	/* set iterator in struct */
	user_data->it = it;
}

static gboolean
gsf_msole_metadata_write_prop (GsfOutput *out, GsfMSOleMetaDataProp_real *prop)
{
	gboolean success = TRUE;
	GsfTimestamp const *timestamp = NULL;
	GValueArray	   *vector = NULL;
	guint32 vector_num;
	GsfMSOleMetaDataProp_real *vector_prop = NULL;
	guint64 timet_value = 0;
	guint32 length = 0;
	guint8 buf[8];
	GValue *tmp;
	guint32 i;


	GSF_LE_SET_GUINT32 (buf+0, prop->type);
	if (!gsf_output_write (out, 4, buf))
		success = FALSE;

	if (success) {
		switch (prop->type) {
		case VT_BOOL:
			/* 0=false, -1=true */
			if (g_value_get_boolean(prop->value) == TRUE)
				GSF_LE_SET_GINT8(buf+0, -1);
			else
				GSF_LE_SET_GINT8(buf+0, 0);
			buf[1] = 0;
			buf[2] = 0;
			buf[3] = 0;
			if (!gsf_output_write (out, 4, buf))
				success = FALSE;
			break;

		case VT_UI1:
			GSF_LE_SET_GUINT8(buf+0, g_value_get_uchar (prop->value));
			if (!gsf_output_write (out, 1, buf))
				success = FALSE;
			break;

		case VT_I2:
			GSF_LE_SET_GINT16(buf+0, g_value_get_int (prop->value));
			if (!gsf_output_write (out, 2, buf))
				success = FALSE;
			break;

		case VT_I4:
			GSF_LE_SET_GINT32(buf+0, g_value_get_int (prop->value));
			if (!gsf_output_write (out, 4, buf))
				success = FALSE;
			break;

		case VT_UI2:
			GSF_LE_SET_GUINT16(buf+0, g_value_get_uint (prop->value));
			if (!gsf_output_write (out, 2, buf))
				success = FALSE;
			break;

		case VT_UI4:
			GSF_LE_SET_GUINT16(buf+0, g_value_get_uint (prop->value));
			if (!gsf_output_write (out, 4, buf))
				success = FALSE;
			break;

		case VT_R4:
			GSF_LE_SET_FLOAT(buf+0, g_value_get_float(prop->value));
			if (!gsf_output_write (out, 4, buf))
				success = FALSE;
			break;

		case VT_R8:
			GSF_LE_SET_DOUBLE(buf+0, g_value_get_double(prop->value));
			if (!gsf_output_write (out, 8, buf))
				success = FALSE;
			break;

		case VT_LPSTR:
			/* write the length of the string */
			length = strlen((char *)g_value_get_string (prop->value));
			GSF_LE_SET_GINT32(buf+0, length + 1 /* \0 */);
			if (!gsf_output_write (out, 4, buf))
				success = FALSE;

			/* write the string */
			if (!gsf_output_write (out, length, (char *)g_value_get_string(prop->value)))
				success = FALSE;

			/* append an \0 */
			buf[0] = 0;
			if (!gsf_output_write (out, 1, buf))
				success = FALSE;
			break;

		case VT_FILETIME:
			timestamp = (GsfTimestamp const *)g_value_get_boxed(prop->value);
			timet_value = (time_t)timestamp->timet;

#ifdef _MSC_VER
			timet_value += 11644473600i64;
#else
			timet_value += 11644473600ULL;
#endif
			timet_value *= 10000000;

			GSF_LE_SET_GUINT64(buf+0, timet_value);
			if (!gsf_output_write (out, 8, buf))
				success = FALSE;
			break;

		case VT_VECTOR | VT_VARIANT:
			vector = gsf_value_get_docprop_varray (prop->value);
			vector_num = vector->n_values;

			GSF_LE_SET_GUINT32(buf+0, vector_num);
			if (!gsf_output_write (out, 4, buf))
			      success = FALSE;

			for (i = 0; i < vector_num; i++) {
				tmp = g_value_array_get_nth (vector, i);
				vector_prop = g_new (GsfMSOleMetaDataProp_real, 1);
				vector_prop->value = tmp;

				switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (tmp))) {
				case G_TYPE_BOOLEAN:
					vector_prop->type = VT_BOOL;
					break;

				case G_TYPE_UCHAR:
					vector_prop->type = VT_UI1;
					break;

				case G_TYPE_INT:
					vector_prop->type = prop->type;
					if (vector_prop->type == VT_UNKNOWN)
					      vector_prop->type = VT_I4;
					break;

				case G_TYPE_UINT:
					vector_prop->type = prop->type;
					if (vector_prop->type == VT_UNKNOWN)
					      vector_prop->type = VT_UI4;
					break;

				case G_TYPE_FLOAT:
					vector_prop->type = VT_R4;
					break;

				case G_TYPE_DOUBLE:
					vector_prop->type = VT_R8;
					break;

				case G_TYPE_STRING:
					vector_prop->type = VT_LPSTR;
					break;

				case G_TYPE_BOXED:
					vector_prop->type = VT_FILETIME;
					break;

				default:
					vector_prop->type = VT_NULL;
				}

				gsf_msole_metadata_write_prop(out, vector_prop);
				if (vector_prop != NULL)
				      g_free (vector_prop);
				}

				break;

			case VT_VECTOR | VT_LPSTR:
				/* NOTE: don't write the type here */
				vector = gsf_value_get_docprop_varray (prop->value);
				vector_num = vector->n_values;

				GSF_LE_SET_GUINT32(buf+0, vector_num);
				if (!gsf_output_write (out, 4, buf))
				      success = FALSE;

				for(i=0; i<vector_num; i++) {
					tmp = g_value_array_get_nth (vector, i);

					/* write the length of the string */
					length = strlen((char *)g_value_get_string(tmp));
					GSF_LE_SET_GINT32(buf+0, length + 1 /* \0 */);
					if (!gsf_output_write (out, 4, buf))
						success = FALSE;

					/* write the string */
					if (!gsf_output_write (out, length, (char *)g_value_get_string(tmp)))
						success = FALSE;

					/* append an \0 */
					buf[0] = 0;
					if (!gsf_output_write (out, 1, buf))
						success = FALSE;
				}
				break;

			default:
				g_warning("Can't write datatype. unknown");
				break;
		}
	}

	return success;
}

/**
 * gsf_msole_metadata_write :
 * @out : #GsfOutput
 * @meta_data : #GsfDocMetaData
 * @doc_not_component : a kludge to differentiate DocumentSummary from Summary
 *
 * Returns GError which the caller must free on error.
 **/
GError *
gsf_msole_metadata_write (GsfOutput *out,
			  GsfDocMetaData const *meta_data,
			  gboolean doc_not_component)
{
	static guint8 const header[] = {
		0xfe, 0xff, /* byte order */
		0, 0,   /* no one seems to use version 1 */
		0x04, 0x0a, 0x02, 0x00, /* win32 version (xp = a04, nt/2k = 04 ?) */
		0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, /* clasid = 0 */
	};

	gboolean success = FALSE;
	gsf_off_t offset = 0;

	/* prepare */
	guint32 section_count = 0;	/* count of sections */
	guint32 byte_count = 0;		/* count of bytes in a section */
	guint32 props_count = 0;	/* count of properties global */
	guint32 section_props_count = 0; /* count of properties in a section */
	guint32 length = 0;
	guint32 dict_size = 0;
	gsf_off_t section_offset = 8;
	gsf_off_t index_offset = 0;
	guint32 udef_props = 30;   /* id start of user defined ids
	                              must be higher than the id of the last defined property */
	guint8 buf[8];
	guint8 const *guid;
	guint32 i;
	int	codepage = 1200;
	GsfDocProp		  *prop;
	GsfMSOleMetaDataProp_real *props_codepage = NULL;
	GsfMSOleMetaDataProp_real *props = NULL;
	AddPropsStruct		  *props_struct = NULL;

	props_count = gsf_doc_meta_data_size (meta_data);

	/* check and save codepage into props_codepage first to get the ncoding
	 * or future strings */
	if (NULL != (prop = gsf_doc_meta_data_lookup (meta_data, GSF_META_NAME_LANGUAGE))) {
		GValue const *val = gsf_doc_prop_get_val (prop);
		if (NULL == val || !G_VALUE_HOLDS_INT (val)) {
			g_warning ("value of property with name=%s is not an valid codepage",
				   GSF_META_NAME_LANGUAGE);
			goto err;
		}
		codepage = g_value_get_int (val);
		props_codepage = g_new (GsfMSOleMetaDataProp_real, 1);
		props_codepage->id = 1; /* see common_props for GSF_META_NAME_LANGUAGE */
		props_codepage->offset = 0;
		props_count--; /* do not count the codepage */
	}

	props = g_new (GsfMSOleMetaDataProp_real, props_count);

	/* insert ids and offsets into props[] (only for id !=0, 1, >0xFFFFFFFF) */
	props_struct = g_new (AddPropsStruct, 1);
	props_struct->props = props;
	props_struct->udef_props = udef_props;
	props_struct->it = 0;
	props_struct->count = 0;
	props_struct->dict_count = 0;
	props_struct->offset = section_offset;
	props_struct->dict_offset = section_offset;
	gsf_doc_meta_data_foreach (meta_data,
		(GHFunc) add_props, props_struct);

	/* write static header */
	if (!gsf_output_write (out, sizeof (header), header))
		goto err;
	offset += sizeof (header);

	/* write section count */
	section_count = 1;
	if (props_struct->dict_count > 0)
		section_count = 2;

	buf[0] = section_count;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	if (!gsf_output_write (out, 4, buf))
		goto err;
	offset += 4;

	/* write unqiue format id  (see msdn) */
	guid = doc_not_component ? document_guid : component_guid;
	if (!gsf_output_write (out, 16, guid))
		goto err;
	offset += 16;

	/* write offset */
	/* we need to write the user_guid if we have a dictionary,
	 * so we add the size to the offset */
	if (props_struct->dict_count > 0) {
		offset += sizeof (user_guid);
		offset += 4;
	}

	GSF_LE_SET_GUINT32 (buf, offset + 4 /* size of offset itself */);
	if (!gsf_output_write (out, 4, buf))
		goto err;
	offset += 4;

	/* calc section header */
	byte_count = props_struct->offset;

	section_props_count = props_struct->count;
	if (props_codepage != NULL)
		section_props_count++;

	index_offset = 8; /* section header itself */
	index_offset += (section_props_count * (sizeof(guint32) /* property id */ + sizeof(guint32) /* property offset */));

	/* write user_guid if dictionary exist */
	if (props_struct->dict_count > 0) {
		if (!gsf_output_write (out, sizeof (user_guid), user_guid))
			goto err;

		GSF_LE_SET_GUINT32 (buf+0, byte_count + index_offset + offset);
		if (!gsf_output_write (out, 4, buf))
			goto err;
	}

	/*  write secion header */
	GSF_LE_SET_GUINT32 (buf+0, byte_count + index_offset); /* byte count */
	GSF_LE_SET_GUINT32 (buf+4, section_props_count);       /* properties count */
	if (!gsf_output_write (out, 8, buf))
		goto err;

	/* write codepage index */
	if (props_codepage != NULL) {
		props_codepage->offset = index_offset;
		GSF_LE_SET_GUINT32 (buf+0, props_codepage->id);
		GSF_LE_SET_GUINT32 (buf+4, props_codepage->offset);
		if (!gsf_output_write (out, 8, buf))
			goto err;
	}

	/* write property index */
	for (i = 0; i < props_count; i++)
		if ((props_struct->props)[i].dict_name == NULL) {
			GSF_LE_SET_GUINT32 (buf+0, (props_struct->props)[i].id);
			GSF_LE_SET_GUINT32 (buf+4, (props_struct->props)[i].offset + index_offset);
			if (!gsf_output_write (out, 8, buf))
				goto err;
		}

	/* write codepage
	 * TODO: write codepage support!
	 */
	if (props_codepage != NULL) {
		GSF_LE_SET_GUINT32 (buf+0, VT_I2);
		GSF_LE_SET_GUINT32 (buf+4, codepage);
		if (!gsf_output_write (out, 8, buf))
			goto err;
	}

	/* write properties */
	for (i=0; i < props_count; i++)
		if ((props_struct->props)[i].dict_name == NULL &&
		    !gsf_msole_metadata_write_prop(out, &(props_struct->props)[i]))
			goto err;


	/*
	 * Write 2. Section
	 */
	if (section_count >= 2) {
		/* calculate sizeof dictionary */
		dict_size = 4;
		for (i=0; i < props_count; i++) {
			if ((props_struct->props)[i].dict_name != NULL) {
				dict_size += 4; /* property id */
				dict_size += 4; /* lengt of string */
				dict_size += strlen((props_struct->props)[i].dict_name) + 1; /* \0 */
			}
		}

		/* write section header */
		byte_count = props_struct->dict_offset;
		byte_count += dict_size;
		section_props_count = props_struct->dict_count;
		section_props_count++; /* dictionary */
		index_offset = 8; /* section header itself */
		if (props_codepage != NULL)
			section_props_count++;
		index_offset += (section_props_count * (sizeof(guint32) /* property id */ + sizeof(guint32) /* property offset */));
		GSF_LE_SET_GUINT32 (buf+0, byte_count + index_offset); /* byte count */
		GSF_LE_SET_GUINT32 (buf+4, section_props_count);  /* properties count */
		if (!gsf_output_write (out, 8, buf))
			goto err;

		/* write dictionary index */
		GSF_LE_SET_GUINT32 (buf+0, 0);
		GSF_LE_SET_GUINT32 (buf+4, index_offset);
		if (!gsf_output_write (out, 8, buf))
			goto err;

		index_offset += dict_size; /* sizeof dictionary */

		/* write codepage index */
		if (props_codepage != NULL) {
			props_codepage->offset = index_offset;
			GSF_LE_SET_GUINT32 (buf+0, props_codepage->id);
			GSF_LE_SET_GUINT32 (buf+4, props_codepage->offset);
			if (!gsf_output_write (out, 8, buf))
				goto err;
		}

		/* write property index */
		for (i=0; i < props_count; i++)
			if ((props_struct->props)[i].dict_name != NULL) {
				GSF_LE_SET_GUINT32 (buf+0, (props_struct->props)[i].id - udef_props + 2);
				GSF_LE_SET_GUINT32 (buf+4, (props_struct->props)[i].offset + index_offset);
				if (!gsf_output_write (out, 8, buf))
					goto err;
			}

		/* write dictionary */
		GSF_LE_SET_GUINT32 (buf+0, props_struct->dict_count);
		if (!gsf_output_write (out, 4, buf))
			goto err;

		for (i=0; i < props_count; i++) {
			if ((props_struct->props)[i].dict_name != NULL) {
				length = strlen((props_struct->props)[i].dict_name);
				GSF_LE_SET_GUINT32 (buf+0, (props_struct->props)[i].id - udef_props + 2);
				GSF_LE_SET_GUINT32 (buf+4, length + 1 /* \0 */);
				if (!gsf_output_write (out, 8, buf))
					goto err;

				/* write the string */
				if (!gsf_output_write (out, length, (props_struct->props)[i].dict_name) )
					goto err;

				/* append an \0 */
				buf[0] = 0;
				if (!gsf_output_write (out, 1, buf))
					goto err;
			}
		}

		/* write codepage
		 * TODO: write codepage support!
		 */
		if (props_codepage != NULL) {
			GSF_LE_SET_GUINT32 (buf+0, VT_I2);
			GSF_LE_SET_GUINT32 (buf+4, codepage);
			if (!gsf_output_write (out, 8, buf))
				goto err;
		}

		/* write properties */
		for (i = 0; i < props_count; i++)
			if ((props_struct->props)[i].dict_name != NULL &&
			    !gsf_msole_metadata_write_prop(out, &(props_struct->props)[i]))
				goto err;
	}

	success = TRUE;
err:
	/* free some stuff */
	if (props_codepage != NULL)
		g_free (props_codepage);

	if (props != NULL)
		g_free (props);

	if (props_struct != NULL)
		g_free (props_struct);

	return success ? NULL : g_error_copy (gsf_output_error (out));
}

typedef struct {
	char const *tag;
	guint	lid;
} GsfLanguageMapping;

static GsfLanguageMapping const gsf_msole_language_ids[] = {
	{ "-none-", 0x0000 }, /* none (language neutral) */
	{ "-none-", 0x0400 }, /* none */
	{ "af_ZA",  0x0436 }, /* Afrikaans */
	{ "am",     0x045e }, /* Amharic */
	{ "sq_AL",  0x041c }, /* Albanian */
	{ "ar_SA",  0x0401 }, /* Arabic (Saudi) */
	{ "ar_IQ",  0x0801 }, /* Arabic (Iraq) */
	{ "ar_EG",  0x0c01 }, /* Arabic (Egypt) */		
	{ "ar_LY",  0x1001 }, /* Arabic (Libya) */
	{ "ar_DZ",  0x1401 }, /* Arabic (Algeria) */
	{ "ar_MA",  0x1801 }, /* Arabic (Morocco) */
	{ "ar_TN",  0x1c01 }, /* Arabic (Tunisia) */
	{ "ar_OM",  0x2001 }, /* Arabic (Oman) */
	{ "ar_YE",  0x2401 }, /* Arabic (Yemen) */		
	{ "ar_SY",  0x2801 }, /* Arabic (Syria) */
	{ "ar_JO",  0x2c01 }, /* Arabic (Jordan) */
	{ "ar_LB",  0x3001 }, /* Arabic (Lebanon) */
	{ "ar_KW",  0x3401 }, /* Arabic (Kuwait) */
	{ "ar_AE",  0x3801 }, /* Arabic (United Arab Emirates) */
	{ "ar_BH",  0x3c01 }, /* Arabic (Bahrain) */		
	{ "ar_QA",  0x4001 }, /* Arabic (Qatar) */
	{ "as",     0x044d }, /* Assamese */
	{ "az",     0x042c }, /* Azerbaijani */
	{ "hy_AM",  0x042b }, /* Armenian */
	{ "az",     0x044c }, /* Azeri (Latin) az_ */
	{ "az",     0x082c }, /* Azeri (Cyrillic) az_ */
	{ "eu_ES",  0x042d }, /* Basque */
	{ "be_BY",  0x0423 }, /* Belarussian */		
	{ "bn",     0x0445 }, /* Bengali bn_ */
	{ "bg_BG",  0x0402 }, /* Bulgarian */
	{ "ca_ES",  0x0403 }, /* Catalan */
	{ "zh_TW",  0x0404 }, /* Chinese (Taiwan) */
	{ "zh_CN",  0x0804 }, /* Chinese (PRC) */
	{ "zh_HK",  0x0c04 }, /* Chinese (Hong Kong) */		
	{ "zh_SG",  0x1004 }, /* Chinese (Singapore) */
	{ "ch_MO",  0x1404 }, /* Chinese (Macau SAR) */
	{ "hr_HR",  0x041a }, /* Croatian */
	{ "cs_CZ",  0x0405 }, /* Czech */
	{ "da_DK",  0x0406 }, /* Danish */
	{ "div",    0x465 }, /* Divehi div_*/
	{ "nl_NL",  0x0413 }, /* Dutch (Netherlands) */		
	{ "nl_BE",  0x0813 }, /* Dutch (Belgium) */
	{ "en_US",  0x0409 }, /* English (USA) */
	{ "en_GB",  0x0809 }, /* English (UK) */
	{ "en_AU",  0x0c09 }, /* English (Australia) */
	{ "en_CA",  0x1009 }, /* English (Canada) */
	{ "en_NZ",  0x1409 }, /* English (New Zealand) */
	{ "en_IE",  0x1809 }, /* English (Ireland) */
	{ "en_ZA",  0x1c09 }, /* English (South Africa) */
	{ "en_JM",  0x2009 }, /* English (Jamaica) */
	{ "en",     0x2409 }, /* English (Caribbean) */
	{ "en_BZ",  0x2809 }, /* English (Belize) */
	{ "en_TT",  0x2c09 }, /* English (Trinidad) */		
	{ "en_ZW",  0x3009 }, /* English (Zimbabwe) */
	{ "en_PH",  0x3409 }, /* English (Phillipines) */
	{ "et_EE",  0x0425 }, /* Estonian */
	{ "fo",     0x0438 }, /* Faeroese fo_ */
	{ "fa_IR",  0x0429 }, /* Farsi */
	{ "fi_FI",  0x040b }, /* Finnish */		
	{ "fr_FR",  0x040c }, /* French (France) */
	{ "fr_BE",  0x080c }, /* French (Belgium) */
	{ "fr_CA",  0x0c0c }, /* French (Canada) */
	{ "fr_CH",  0x100c }, /* French (Switzerland) */
	{ "fr_LU",  0x140c }, /* French (Luxembourg) */
	{ "fr_MC",  0x180c }, /* French (Monaco) */		
	{ "gl",     0x0456 }, /* Galician gl_ */
	{ "ga_IE",  0x083c }, /* Irish Gaelic */
	{ "gd_GB",  0x100c }, /* Scottish Gaelic */
	{ "ka_GE",  0x0437 }, /* Georgian */
	{ "de_DE",  0x0407 }, /* German (Germany) */
	{ "de_CH",  0x0807 }, /* German (Switzerland) */
	{ "de_AT",  0x0c07 }, /* German (Austria) */
	{ "de_LU",  0x1007 }, /* German (Luxembourg) */
	{ "de_LI",  0x1407 }, /* German (Liechtenstein) */
	{ "el_GR",  0x0408 }, /* Greek */
	{ "gu",     0x0447 }, /* Gujarati gu_ */
	{ "ha",     0x0468 }, /* Hausa */
	{ "he_IL",  0x040d }, /* Hebrew */
	{ "hi_IN",  0x0439 }, /* Hindi */
	{ "hu_HU",  0x040e }, /* Hungarian */
	{ "is_IS",  0x040f }, /* Icelandic */		
	{ "id_ID",  0x0421 }, /* Indonesian */
	{ "iu",     0x045d }, /* Inkutitut */
	{ "it_IT",  0x0410 }, /* Italian (Italy) */
	{ "it_CH",  0x0810 }, /* Italian (Switzerland) */
	{ "ja_JP",  0x0411}, /* Japanese */
	{ "kn",     0x044b }, /* Kannada kn_ */
	{ "ks",     0x0860 }, /* Kashmiri (India) ks_ */
	{ "kk",     0x043f }, /* Kazakh kk_ */
	{ "kok",    0x0457 }, /* Konkani kok_ */
	{ "ko_KR",  0x0412 }, /* Korean */
	{ "ko",     0x0812 }, /* Korean (Johab) ko_ */
	{ "kir",    0x0440 }, /* Kyrgyz */
	{ "la",     0x0476 }, /* Latin */
	{ "lo",     0x0454 }, /* Laothian */
	{ "lv_LV",  0x0426 }, /* Latvian */
	{ "lt_LT",  0x0427 }, /* Lithuanian */		
	{ "lt_LT",  0x0827 }, /* Lithuanian (Classic) */
	{ "mk",     0x042f }, /* FYRO Macedonian */
	{ "my_MY",  0x043e }, /* Malaysian */
	{ "my_BN",  0x083e }, /* Malay Brunei Darussalam */
	{ "ml",     0x044c }, /* Malayalam ml_ */
	{ "mr",     0x044e }, /* Marathi mr_ */
	{ "mt",     0x043a }, /* Maltese */
	{ "mo",     0x0450 }, /* Mongolian */
	{ "ne_NP",  0x0461 }, /* Napali (Nepal) */
	{ "ne_IN",  0x0861 }, /* Nepali (India) */
	{ "nb_NO",  0x0414 }, /* Norwegian (Bokmaal) */
	{ "nn_NO",  0x0814 }, /* Norwegian (Nynorsk) */
	{ "or",     0x0448 }, /* Oriya or_ */
	{ "om",     0x0472 }, /* Oromo (Afan, Galla) */
	{ "pl_PL",  0x0415 }, /* Polish */		
	{ "pt_BR",  0x0416 }, /* Portuguese (Brazil) */
	{ "pt_PT",  0x0816 }, /* Portuguese (Portugal) */
	{ "pa",     0x0446 }, /* Punjabi pa_ */
	{ "ps",     0x0463 }, /* Pashto (Pushto) */
	{ "rm",     0x0417 }, /* Rhaeto_Romanic rm_ */
	{ "ro_RO",  0x0418 }, /* Romanian */
	{ "ro_MD",  0x0818 }, /* Romanian (Moldova) */		
	{ "ru_RU",  0x0419 }, /* Russian */
	{ "ru_MD",  0x0819 }, /* Russian (Moldova) */
	{ "se",     0x043b }, /* Sami (Lappish) se_ */
	{ "sa",     0x044f }, /* Sanskrit sa_ */
	{ "sr",     0x0c1a }, /* Serbian (Cyrillic) sr_ */
	{ "sr",     0x081a }, /* Serbian (Latin) sr_ */		
	{ "sd",     0x0459 }, /* Sindhi sd_ */
	{ "sk_SK",  0x041b }, /* Slovak */
	{ "sl_SI",  0x0424 }, /* Slovenian */
	{ "wen",    0x042e }, /* Sorbian wen_ */
	{ "so",     0x0477 }, /* Somali */
	{ "es_ES",  0x040a }, /* Spanish (Spain, Traditional) */
	{ "es_MX",  0x080a }, /* Spanish (Mexico) */		
	{ "es_ES",  0x0c0a }, /* Spanish (Modern) */
	{ "es_GT",  0x100a }, /* Spanish (Guatemala) */
	{ "es_CR",  0x140a }, /* Spanish (Costa Rica) */
	{ "es_PA",  0x180a }, /* Spanish (Panama) */
	{ "es_DO",  0x1c0a }, /* Spanish (Dominican Republic) */
	{ "es_VE",  0x200a }, /* Spanish (Venezuela) */		
	{ "es_CO",  0x240a }, /* Spanish (Colombia) */
	{ "es_PE",  0x280a }, /* Spanish (Peru) */
	{ "es_AR",  0x2c0a }, /* Spanish (Argentina) */
	{ "es_EC",  0x300a }, /* Spanish (Ecuador) */
	{ "es_CL",  0x340a }, /* Spanish (Chile) */
	{ "es_UY",  0x380a }, /* Spanish (Uruguay) */		
	{ "es_PY",  0x3c0a }, /* Spanish (Paraguay) */
	{ "es_BO",  0x400a }, /* Spanish (Bolivia) */
	{ "es_SV",  0x440a }, /* Spanish (El Salvador) */
	{ "es_HN",  0x480a }, /* Spanish (Honduras) */
	{ "es_NI",  0x4c0a }, /* Spanish (Nicaragua) */
	{ "es_PR",  0x500a }, /* Spanish (Puerto Rico) */
	{ "sx",     0x0430 }, /* Sutu */
	{ "sw",     0x0441 }, /* Swahili (Kiswahili/Kenya) */
	{ "sv_SE",  0x041d }, /* Swedish */
	{ "sv_FI",  0x081d }, /* Swedish (Finland) */
	{ "ta",     0x0449 }, /* Tamil ta_ */
	{ "tt",     0x0444 }, /* Tatar (Tatarstan) tt_ */
	{ "te",     0x044a }, /* Telugu te_ */
	{ "th_TH",  0x041e }, /* Thai */
	{ "ts",     0x0431 }, /* Tsonga ts_ */
	{ "tn",     0x0432 }, /* Tswana tn_ */
	{ "tr_TR",  0x041f }, /* Turkish */
	{ "tl",     0x0464 }, /* Tagalog */
	{ "tg",     0x0428 }, /* Tajik */
	{ "bo",     0x0451 }, /* Tibetan */
	{ "ti",     0x0473 }, /* Tigrinya */
	{ "uk_UA",  0x0422 }, /* Ukrainian */		
	{ "ur_PK",  0x0420 }, /* Urdu (Pakistan) */
	{ "ur_IN",  0x0820 }, /* Urdu (India) */
	{ "uz",     0x0443 }, /* Uzbek (Latin) uz_ */
	{ "uz",     0x0843 }, /* Uzbek (Cyrillic) uz_ */
	{ "ven",    0x0433 }, /* Venda ven_ */
	{ "vi_VN",  0x042a }, /* Vietnamese */
	{ "cy_GB",  0x0452 }, /* Welsh */
	{ "xh",     0x0434 }, /* Xhosa xh */
	{ "yi",     0x043d }, /* Yiddish yi_ */
	{ "yo",     0x046a }, /* Yoruba */
	{ "zu",     0x0435 }, /* Zulu zu_ */
	{ "en_US",  0x0800 } /* Default */
};

/**
 * gsf_msole_lid_for_language
 * @lang :
 *
 * Returns the LID (Language Identifier) for the input language.
 * If lang is %null, return 0x0400 ("-none-"), and not 0x0000 ("no proofing")
 **/
guint
gsf_msole_lid_for_language (char const *lang)
{
	guint i = 0 ;
	size_t len;

	if (lang == NULL)
		return 0x0400;   /* return -none- */

	/* Allow lang to match as a prefix (eg fr == fr_FR@euro) */
	len = strlen (lang);
	for (i = 0 ; i < G_N_ELEMENTS(gsf_msole_language_ids); i++)
		if (!strncmp (lang, gsf_msole_language_ids[i].tag, len))
			return gsf_msole_language_ids[i].lid;
	
	return 0x0400 ;   /* return -none- */
}

/**
 * gsf_msole_language_for_lid :
 * @lid :
 *
 * Returns the xx_YY style string (can be just xx or xxx) for the given LID.
 * Return value must not be freed. If the LID is not found, is set to 0x0400,
 * or is set to 0x0000, will return "-none-"
 **/
char const *
gsf_msole_language_for_lid (guint lid)
{
	guint i = 0 ;
	
	for (i = 0 ; i < G_N_ELEMENTS(gsf_msole_language_ids); i++)
		if (gsf_msole_language_ids[i].lid == lid)
			return gsf_msole_language_ids[i].tag;
	
	return "-none-"; /* default */
}

/**
 * gsf_msole_locale_to_lid :
 *
 * Covert the the codepage into an applicable LID
 **/
guint
gsf_msole_codepage_to_lid (int codepage)
{
	switch (codepage) {
	case 77:		/* MAC_CHARSET */
		return 0xFFF;	/* This number is a hack */
	case 128:		/* SHIFTJIS_CHARSET */
		return 0x411;	/* Japanese */
	case 129:		/* HANGEUL_CHARSET */
		return 0x412;	/* Korean */
	case 130:		/* JOHAB_CHARSET */
		return 0x812;	/* Korean (Johab) */
	case 134:		/* GB2312_CHARSET - Chinese Simplified */
		return 0x804;	/* China PRC - And others!! */
	case 136:		/* CHINESEBIG5_CHARSET - Chinese Traditional */
		return 0x404;	/* Taiwan - And others!! */
	case 161:		/* GREEK_CHARSET */
		return 0x408;	/* Greek */
	case 162:		/* TURKISH_CHARSET */
		return 0x41f;	/* Turkish */
	case 163:		/* VIETNAMESE_CHARSET */
		return 0x42a;	/* Vietnamese */
	case 177:		/* HEBREW_CHARSET */
		return 0x40d;	/* Hebrew */
	case 178:		/* ARABIC_CHARSET */
		return 0x01;	/* Arabic */
	case 186:		/* BALTIC_CHARSET */
		return 0x425;	/* Estonian - And others!! */
	case 204:		/* RUSSIAN_CHARSET */
		return 0x419;	/* Russian - And others!! */
	case 222:		/* THAI_CHARSET */
		return 0x41e;	/* Thai */
	case 238:		/* EASTEUROPE_CHARSET */
		return 0x405;	/* Czech - And many others!! */
	}

	/* default */
	return 0x0;
}

/**
 * gsf_msole_lid_to_codepage
 * @lid :
 *
 * Returns our best guess at the codepage for the given language id
 **/
guint
gsf_msole_lid_to_codepage (guint lid)
{
	if (lid == 0x0FFF) /* Macintosh Hack */
		return 0x0FFF;

	switch (lid & 0xff) {
	case 0x01:		/* Arabic */
		return 1256;
	case 0x02:		/* Bulgarian */
		return 1251;
	case 0x03:		/* Catalan */
		return 1252;
	case 0x04:		/* Chinese */
		switch (lid) {
		case 0x1004:		/* Chinese (Singapore) */
		case 0x0404:		/* Chinese (Taiwan) */
		case 0x1404:		/* Chinese (Macau SAR) */
		case 0x0c04:		/* Chinese (Hong Kong SAR, PRC) */
			return 950;
			
		case 0x0804:		/* Chinese (PRC) */
			return 936;
		default :
			break;
		}
		break;
	case 0x05:		/* Czech */
		return 1250;
	case 0x06:		/* Danish */
		return 1252;
	case 0x07:		/* German */
		return 1252;
	case 0x08:		/* Greek */
		return 1253;
	case 0x09:		/* English */
		return 1252;
	case 0x0a:		/* Spanish */
		return 1252;
	case 0x0b:		/* Finnish */
		return 1252;
	case 0x0c:		/* French */
		return 1252;
	case 0x0d:		/* Hebrew */
		return 1255;
	case 0x0e:		/* Hungarian */
		return 1250;
	case 0x0f:		/* Icelandic */
		return 1252;
	case 0x10:		/* Italian */
		return 1252;
	case 0x11:		/* Japanese */
		return 932;
	case 0x12:		/* Korean */
		switch (lid) {
		case 0x0812:		/* Korean (Johab) */
			return 1361;
		case 0x0412:		/* Korean */
			return 949;
		default :
			break;
		}
		break;
	case 0x13:		/* Dutch */
		return 1252;
	case 0x14:		/* Norwegian */
		return 1252;
	case 0x15:		/* Polish */
		return 1250;
	case 0x16:		/* Portuguese */
		return 1252;
	case 0x17:		/* Rhaeto-Romanic */
		return 1252;
	case 0x18:		/* Romanian */
		return 1250;
	case 0x19:		/* Russian */
		return 1251;
	case 0x1a:		/* Serbian, Croatian, (Bosnian?) */
		switch (lid) {
		case 0x041a:		/* Croatian */
			return 1252;
		case 0x0c1a:		/* Serbian (Cyrillic) */
			return 1251;
		case 0x081a:		/* Serbian (Latin) */
			return 1252;
		default :
			break;
		}
		break;
	case 0x1b:		/* Slovak */
		return 1250;
	case 0x1c:		/* Albanian */
		return 1251;
	case 0x1d:		/* Swedish */
		return 1252;
	case 0x1e:		/* Thai */
		return 874;
	case 0x1f:		/* Turkish */
		return 1254;
	case 0x20:		/* Urdu. This is Unicode only. */
		return 0;
	case 0x21:		/* Bahasa Indonesian */
		return 1252;
	case 0x22:		/* Ukrainian */
		return 1251;
	case 0x23:		/* Byelorussian / Belarusian */
		return 1251;
	case 0x24:		/* Slovenian */
		return 1250;
	case 0x25:		/* Estonian */
		return 1257;
	case 0x26:		/* Latvian */
		return 1257;
	case 0x27:		/* Lithuanian */
		return 1257;
	case 0x29:		/* Farsi / Persian. This is Unicode only. */
		return 0;
	case 0x2a:		/* Vietnamese */
		return 1258;
	case 0x2b:		/* Windows 2000: Armenian. This is Unicode only. */
		return 0;
	case 0x2c:		/* Azeri */
		switch (lid) {
		case 0x082c:		/* Azeri (Cyrillic) */
			return 1251;
		default :
			break;
		}
		break;
	case 0x2d:		/* Basque */
		return 1252;
	case 0x2f:		/* Macedonian */
		return 1251;
	case 0x36:		/* Afrikaans */
		return 1252;
	case 0x37:		/* Windows 2000: Georgian. This is Unicode only. */
		return 0;
	case 0x38:		/* Faeroese */
		return 1252;
	case 0x39:		/* Windows 2000: Hindi. This is Unicode only. */
		return 0;
	case 0x3E:		/* Malaysian / Malay */
		return 1252;
	case 0x41:		/* Swahili */
		return 1252;
	case 0x43:		/* Uzbek */
		switch (lid) {
		case 0x0843:		/* Uzbek (Cyrillic) */
			return 1251;
		default :
			break;
		}
		break;
	case 0x45:		/* Windows 2000: Bengali. This is Unicode only. */
	case 0x46:		/* Windows 2000: Punjabi. This is Unicode only. */
	case 0x47:		/* Windows 2000: Gujarati. This is Unicode only. */
	case 0x48:		/* Windows 2000: Oriya. This is Unicode only. */
	case 0x49:		/* Windows 2000: Tamil. This is Unicode only. */
	case 0x4a:		/* Windows 2000: Telugu. This is Unicode only. */
	case 0x4b:		/* Windows 2000: Kannada. This is Unicode only. */
	case 0x4c:		/* Windows 2000: Malayalam. This is Unicode only. */
	case 0x4d:		/* Windows 2000: Assamese. This is Unicode only. */
	case 0x4e:		/* Windows 2000: Marathi. This is Unicode only. */
	case 0x4f:		/* Windows 2000: Sanskrit. This is Unicode only. */
	case 0x55:		/* Myanmar / Burmese. This is Unicode only. */
	case 0x57:		/* Windows 2000: Konkani. This is Unicode only. */
	case 0x61:		/* Windows 2000: Nepali (India). This is Unicode only. */
		return 0;

#if 0
		/****************************************************************** 
		 * Below this line is untested, unproven, and are just guesses.   *
		 * Insert above and use at your own risk                          *
		 ******************************************************************/

	case 0x042c:		/* Azeri (Latin) */
	case 0x0443:		/* Uzbek (Latin) */
	case 0x30:		/* Sutu */
		return 1252; /* UNKNOWN, believed to be CP1252 */

	case 0x3f:		/* Kazakh */
		return 1251; /* JUST UNKNOWN, probably CP1251 */

	case 0x44:		/* Tatar */
	case 0x58:		/* Manipuri */
	case 0x59:		/* Sindhi */
	case 0x60:		/* Kashmiri (India) */
		return 0; /* UNKNOWN, believed to be Unicode only */
#endif
	};
	
	/* This is just a guess, but it will be a frequent guess */
	return 1252;
}

/**
 * gsf_msole_lid_to_codepage_str
 * @lid :
 * 
 * Returns the Iconv codepage string for the given LID.
 * Return value must be g_free ()'d
 **/
gchar *
gsf_msole_lid_to_codepage_str (guint lid)
{
	guint cp = 0;

	if (lid == 0x0FFF)	/* Macintosh Hack */
		return g_strdup ("MACINTOSH");

	cp = gsf_msole_lid_to_codepage (lid);
	return g_strdup_printf ("CP%d", cp);
}

/**
 * gsf_msole_iconv_win_codepage :
 *
 * Returns our best guess at the applicable windows code page based on an
 * 	environment variable or the current locale.
 **/
guint
gsf_msole_iconv_win_codepage (void)
{
	char *lang;

	if ((lang = getenv("WINDOWS_LANGUAGE")) == NULL) {
		char const *locale = setlocale (LC_CTYPE, NULL);
		if (locale != NULL) {
			char const *lang_sep = strchr (locale, '.');
			if (lang_sep)
				lang = g_strndup (locale, (unsigned)(lang_sep - locale));
			else
				lang = g_strdup (locale); /* simplifies exit */
		}
	}

	if (lang != NULL) {
		guint lid = gsf_msole_lid_for_language (lang);
		g_free (lang);
		return gsf_msole_lid_to_codepage (lid);
	}
	return 1252; /* default ansi */
}

static GSList *
gsf_msole_iconv_get_codepage_string_list (guint codepage)
{
	GSList *cp_list = NULL;

	switch (codepage)
	{
		case 1200:
			cp_list = g_slist_prepend (cp_list, g_strdup ("UTF-16LE"));
			break;
		case 1201:
			cp_list = g_slist_prepend (cp_list, g_strdup ("UTF-16BE"));
			break;
		case 10000:
			cp_list = g_slist_prepend (cp_list, g_strdup ("MACROMAN"));
			cp_list = g_slist_prepend (cp_list, g_strdup ("MACINTOSH"));
			break;
		case 65001:
			cp_list = g_slist_prepend (cp_list, g_strdup ("UTF-8"));
			break;
		default:
			cp_list = g_slist_prepend (cp_list, g_strdup_printf ("CP%u", codepage));
	}
	
	return cp_list;
}

/**
 * gsf_msole_iconv_open_codepage_for_import :
 * @to:
 * @codepage :
 *
 * Returns an iconv converter for @codepage -> utf8.
 **/
GIConv
gsf_msole_iconv_open_codepage_for_import (char const *to, int codepage)
{
	GIConv iconv_handle = (GIConv)(-1);
	gchar *codepage_str;
	GSList *codepage_list, *cp;
	g_return_val_if_fail (to != NULL, (GIConv)(-1));

	cp = codepage_list = gsf_msole_iconv_get_codepage_string_list (codepage);
	while (cp) {
		codepage_str = cp->data;
		if (iconv_handle == (GIConv)(-1))
			iconv_handle = g_iconv_open (to, codepage_str);
		g_free (codepage_str);
		cp = cp->next;
	}
	g_slist_free (codepage_list);

	if (iconv_handle == (GIConv)(-1))
		g_warning ("Unable to open an iconv handle from codepage %u -> %s",
			   codepage, to);
	return iconv_handle;
}

/**
 * gsf_msole_iconv_open_for_import :
 * @codepage :
 *
 * Returns an iconv converter for single byte encodings @codepage -> utf8.
 * 	Attempt to handle the semantics of a specification for multibyte encodings
 * 	since this is only supposed to be used for single bytes.
 **/
GIConv
gsf_msole_iconv_open_for_import (int codepage)
{
	return gsf_msole_iconv_open_codepage_for_import ("UTF-8", codepage);
}

/**
 * gsf_msole_iconv_open_codepages_for_export :
 * @codepage_to :
 * @from :
 *
 * Returns an iconv converter to go from utf8 -> to our best guess at a useful
 * 	windows codepage.
 **/
GIConv
gsf_msole_iconv_open_codepages_for_export (guint codepage_to, char const *from)
{
	GIConv iconv_handle = (GIConv)(-1);
	gchar *codepage_str;
	GSList *codepage_list, *cp;
	g_return_val_if_fail (from != NULL, (GIConv)(-1));

	cp = codepage_list = gsf_msole_iconv_get_codepage_string_list (codepage_to);
	while (cp) {
		codepage_str = cp->data;
		if (iconv_handle == (GIConv)(-1))
			iconv_handle = g_iconv_open (codepage_str, from);
		g_free (codepage_str);
		cp = cp->next;
	}
	g_slist_free (codepage_list);

	if (iconv_handle == (GIConv)(-1))
		g_warning ("Unable to open an iconv handle from %s -> codepage %u",
			   from, codepage_to);
	return iconv_handle;
}

/**
 * gsf_msole_iconv_open_codepage_for_export :
 * @codepage_to:
 *
 * Returns an iconv converter to go from utf8 -> to our best guess at a useful
 * 	windows codepage.
 **/
GIConv
gsf_msole_iconv_open_codepage_for_export (guint codepage_to)
{
	return gsf_msole_iconv_open_codepages_for_export (codepage_to, "UTF-8");
}

/**
 * gsf_msole_iconv_open_for_export :
 *
 * Returns an iconv convert to go from utf8 -> to our best guess at a useful
 * 	windows codepage.
 **/
GIConv
gsf_msole_iconv_open_for_export (void)
{
	return gsf_msole_iconv_open_codepage_for_export (gsf_msole_iconv_win_codepage ());
}

#define VBA_COMPRESSION_WINDOW 4096

/**
 * gsf_msole_inflate:
 * @input: stream to read from
 * @offset: offset into it for start byte of compresse stream
 * 
 * Decompresses an LZ compressed stream.
 * 
 * Return value: A GByteArray that the caller is responsible for freeing
 **/
GByteArray *
gsf_msole_inflate (GsfInput *input, gsf_off_t offset)
{
	GByteArray *res;
	unsigned	i, win_pos, pos = 0;
	unsigned	mask, shift, distance;
	guint8		flag, buffer [VBA_COMPRESSION_WINDOW];
	guint8 const   *tmp;
	guint16		token, len;
	gboolean	clean = TRUE;

	if (gsf_input_seek (input, offset, G_SEEK_SET))
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
/*				fprintf (stderr, "Shift %d, token len %d, distance %d bytes %.2x %.2x\n",
				shift, len, distance, (token & 0xff), (token >> 8)); */

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
	return res;
}
