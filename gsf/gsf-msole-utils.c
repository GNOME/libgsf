/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-msole-utils.c: 
 *
 * Copyright (C) 2002 Jody Goldberg (jody@gnome.org)
 * excel_iconv* family of functions (C) 2001 by Vlad Harchev <hvv@hippo.ru>
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
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-input.h>
#include <gsf/gsf-output.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-timestamp.h>
#include <stdio.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NO_DEBUG_OLE_PROPS
#ifndef NO_DEBUG_OLE_PROPS
#define d(code)	do { code } while (0)
#else
#define d(code)
#endif

static guint8 const component_guid [] = {
	0xe0, 0x85, 0x9f, 0xf2, 0xf9, 0x4f, 0x68, 0x10,
	0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9
};

static guint8 const document_guid [] = {
	0x02, 0xd5, 0xcd, 0xd5, 0x9c, 0x2e, 0x1b, 0x10,
	0x93, 0x97, 0x08, 0x00, 0x2b, 0x2c, 0xf9, 0xae
};

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
	char const *name;
	guint32	    id;
	GsfMSOleVariantType prefered_type;
} GsfMSOleMetaDataPropMap;

typedef struct {
	guint32		id;
	gsf_off_t	offset;
} GsfMSOleMetaDataProp;

typedef struct {
	GsfMSOleMetaDataType type;
	gsf_off_t   offset;
	guint32	    size, num_props;
	GIConv	    iconv;
	GHashTable *dict;
} GsfMSOleMetaDataSection;

static GsfMSOleMetaDataPropMap const document_props[] = {
	{ "CodePage",		1,	VT_UI2 },
	{ "Category",		2,	VT_LPSTR },
	{ "PresentationFormat",	3,	VT_LPSTR },
	{ "NumBytes",		4,	VT_I4 },
	{ "NumLines",		5,	VT_I4 },
	{ "NumParagraphs",	6,	VT_I4 },
	{ "NumSlides",		7,	VT_I4 },
	{ "NumNotes",		8,	VT_I4 },
	{ "NumHiddenSlides",	9,	VT_I4 },
	{ "NumMMClips",		10,	VT_I4 },
	{ "Scale",		11,	VT_BOOL },
	{ "HeadingPairs",	12,	VT_VECTOR | VT_VARIANT },
	{ "DocumentParts",	13,	VT_VECTOR | VT_LPSTR },
	{ "Manager",		14,	VT_LPSTR },
	{ "Company",		15,	VT_LPSTR },
	{ "LinksDirty",		16,	VT_BOOL }
};

static GsfMSOleMetaDataPropMap const component_props[] = {
	{ "CodePage",		1,	VT_UI2 },
	{ "Title",		2,	VT_LPSTR },
	{ "Subject",		3,	VT_LPSTR },
	{ "Author",		4,	VT_LPSTR },
	{ "Keywords",		5,	VT_LPSTR },
	{ "Comments",		6,	VT_LPSTR },
	{ "Template",		7,	VT_LPSTR },
	{ "LastSavedBy",	8,	VT_LPSTR },
	{ "RevisionNumber",	9,	VT_LPSTR },
	{ "TotalEditingTime",	10,	VT_FILETIME },
	{ "LastPrinted",	11,	VT_FILETIME },
	{ "CreateTime",		12,	VT_FILETIME },
	{ "LastSavedTime",	13,	VT_FILETIME },
	{ "NumPages",		14,	VT_I4 },
	{ "NumWords",		15,	VT_I4 },
	{ "NumCharacters",	16,	VT_I4 },
	{ "Thumbnail",		17,	VT_CF },
	{ "AppName",		18,	VT_LPSTR },
	{ "Security",		19,	VT_I4 }
};

static char const *
msole_prop_id_to_gsf (GsfMSOleMetaDataSection *section, guint32 id)
{
	char const * res = NULL;
	GsfMSOleMetaDataPropMap const *map = NULL;
	unsigned i = 0;

	if (section->dict != NULL) {
		if (id & 0x1000000) {
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
			d (printf (map[i].name););
			return map[i].name;
		}
	d (printf ("_UNKNOWN_(0x%x %d)", id, id););

	return NULL;
}

static GValue *
msole_prop_parse (guint32 type, guint8 const **data, guint8 const *data_end)
{
	GValue *res;
	guint32 len;
	gboolean const is_vector = type & VT_VECTOR;

	g_return_val_if_fail (!(type & (unsigned)(~0x1fff)), NULL); /* not valid in a prop set */

	type &= 0xfff;

	if (is_vector) {
		unsigned i, n;

		g_return_val_if_fail (*data + 4 <= data_end, NULL);

		n = GSF_LE_GET_GUINT32 (*data);
		*data += 4;

		d (printf (" array with %d elem\n", n);
		   gsf_mem_dump (*data, (unsigned)(data_end - *data)););
		for (i = 0 ; i < n ; i++) {
			d (printf ("\t[%d] ", i););
			msole_prop_parse (type, data, data_end);
		}
		return NULL;
	}

	res = g_new0 (GValue, 1);
	switch (type) {
	case VT_EMPTY :		 d (puts ("VT_EMPTY"););
		/* value::unset == empty */
		break;

	case VT_NULL :		 d (puts ("VT_NULL"););
		/* value::unset == null too :-) do we need to distinguish ? */
		break;

	case VT_I2 :		 d (puts ("VT_I2"););
		g_return_val_if_fail (*data + 2 <= data_end, NULL);
		g_value_init (res, G_TYPE_INT);
		g_value_set_int	(res, GSF_LE_GET_GINT16 (*data));
		*data += 2;
		break;

	case VT_I4 :		 d (puts ("VT_I4"););
		g_return_val_if_fail (*data + 4 <= data_end, NULL);
		g_value_init (res, G_TYPE_INT);
		g_value_set_int	(res, GSF_LE_GET_GINT32 (*data));
		*data += 4;
		break;

	case VT_R4 :		 d (puts ("VT_R4"););
		g_return_val_if_fail (*data + 4 <= data_end, NULL);
		g_value_init (res, G_TYPE_FLOAT);
		g_value_set_float (res, GSF_LE_GET_FLOAT (*data));
		*data += 4;
		break;

	case VT_R8 :		 d (puts ("VT_R8"););
		g_return_val_if_fail (*data + 8 <= data_end, NULL);
		g_value_init (res, G_TYPE_DOUBLE);
		g_value_set_double (res, GSF_LE_GET_DOUBLE (*data));
		*data += 8;
		break;

	case VT_CY :		 d (puts ("VT_CY"););
		break;

	case VT_DATE :		 d (puts ("VT_DATE"););
		break;

	case VT_BSTR :		 d (puts ("VT_BSTR"););
		break;

	case VT_DISPATCH :	 d (puts ("VT_DISPATCH"););
		break;

	case VT_BOOL :		 d (puts ("VT_BOOL"););
		g_return_val_if_fail (*data + 1 <= data_end, NULL);
		g_value_init (res, G_TYPE_BOOLEAN);
		g_value_set_boolean (res, **data ? TRUE : FALSE);
		*data += 1;
		break;

	case VT_VARIANT :	 d (printf ("VT_VARIANT containing a "););
		g_free (res);
		type = GSF_LE_GET_GUINT32 (*data);
		*data += 4;
		return msole_prop_parse (type, data, data_end);

	case VT_UI1 :		 d (puts ("VT_UI1"););
		g_return_val_if_fail (*data + 1 <= data_end, NULL);
		g_value_init (res, G_TYPE_UCHAR);
		g_value_set_uchar (res, (guchar)(**data));
		*data += 1;
		break;

	case VT_UI2 :		 d (puts ("VT_UI2"););
		g_return_val_if_fail (*data + 2 <= data_end, NULL);
		g_value_init (res, G_TYPE_UINT);
		g_value_set_uint (res, GSF_LE_GET_GUINT16 (*data));
		*data += 2;
		break;

	case VT_UI4 :		 d (puts ("VT_UI4"););
		g_return_val_if_fail (*data + 4 <= data_end, NULL);
		g_value_init (res, G_TYPE_UINT);
		*data += 4;
		d (printf ("%u\n", GSF_LE_GET_GUINT32 (*data)););
		break;

	case VT_I8 :		 d (puts ("VT_I8"););
		g_return_val_if_fail (*data + 8 <= data_end, NULL);
		g_value_init (res, G_TYPE_INT64);
		*data += 8;
		break;

	case VT_UI8 :		 d (puts ("VT_UI8"););
		g_return_val_if_fail (*data + 8 <= data_end, NULL);
		g_value_init (res, G_TYPE_UINT64);
		*data += 8;
		break;

	case VT_LPSTR :		 d (puts ("VT_LPSTR"););
		/* be anal and safe */
		g_return_val_if_fail (*data + 4 <= data_end, NULL);
		len = GSF_LE_GET_GUINT32 (*data);
		g_return_val_if_fail (*data + 4 + len <= data_end, NULL);
		g_value_init (res, G_TYPE_STRING);
		g_value_set_string (res, *data + 4);
		*data += 4 + len;
#warning TODO : use the codepage
		break;

	case VT_LPWSTR :	 d (puts ("VT_LPWSTR"););
		g_value_init (res, G_TYPE_STRING);
#warning TODO : map to utf8
		break;

	case VT_FILETIME :	 d (puts ("VT_FILETIME"););
		g_return_val_if_fail (*data + 8 <= data_end, NULL);
		g_value_init (res, GSF_TIMESTAMP_TYPE);
	{
		/* ft * 100ns since Jan 1 1601 */
		guint64 ft = GSF_LE_GET_GUINT64 (*data);
		GsfTimestamp ts;

		ft /= 10000000; /* convert to seconds */
		ft -= 11644473600; /* move to Jan 1 1970 */
		ts.timet = (time_t)ft;
		g_value_set_timestamp (res, &ts);
		*data += 8;
		break;
	}
	case VT_BLOB :		 d (puts ("VT_BLOB"););
		break;
	case VT_STREAM :	 d (puts ("VT_STREAM"););
		break;
	case VT_STORAGE :	 d (puts ("VT_STORAGE"););
		break;
	case VT_STREAMED_OBJECT: d (puts ("VT_STREAMED_OBJECT"););
		break;
	case VT_STORED_OBJECT :	 d (puts ("VT_STORED_OBJECT"););
		break;
	case VT_BLOB_OBJECT :	 d (puts ("VT_BLOB_OBJECT"););
		break;
	case VT_CF :		 d (puts ("VT_CF"););
		break;
	case VT_CLSID :		 d (puts ("VT_CLSID"););
		*data += 16;
		break;

	case VT_ERROR :
	case VT_UNKNOWN :
	case VT_DECIMAL :
	case VT_I1 :
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
	};

	d ( if (res != NULL) {
		char *val = g_strdup_value_contents (res);
		printf ("%s\n", val);
		g_free (val);
	} else
		puts ("<unparsed>\n");
	);
	return res;
}

static GValue *
msole_prop_read (GsfInput *in,
		 GsfMSOleMetaDataSection *section,
		 GsfMSOleMetaDataProp    *props,
		 unsigned i)
{
	guint32 type;
	guint8 const *data;
	/* TODO : why size-4 ? I must be missing something */
	unsigned size = ((i+1) >= section->num_props)
		? section->size-4 : props[i+1].offset;
	char const *prop_name;

	g_return_val_if_fail (i < section->num_props, NULL);
	g_return_val_if_fail (size >= props[i].offset + 4, NULL);

	size -= props[i].offset; /* includes the type id */
	if (gsf_input_seek (in, section->offset+props[i].offset, G_SEEK_SET) ||
	    NULL == (data = gsf_input_read (in, size, NULL))) {
		g_warning ("failed to read prop #%d", i);
		return NULL;
	}

	type = GSF_LE_GET_GUINT32 (data);
	data += 4;

	/* dictionary is magic */
	if (props[i].id == 0) {
		guint32 len, id, i, n;
		char *name;

		g_return_val_if_fail (section->dict == NULL, NULL);

		section->dict = g_hash_table_new_full (
			g_direct_hash, g_direct_equal,
			NULL, g_free);

		n = type;
		for (i = 0 ; i < n ; i++) {
			id = GSF_LE_GET_GUINT32 (data);
			len = GSF_LE_GET_GUINT32 (data + 4);

			g_return_val_if_fail (len < 0x10000, NULL);

#warning TODO : map to utf8
			name = g_strndup (data + 8, len);
			data += 8 + len;

			d (printf ("\t%u == %s\n", id, name););
			g_hash_table_replace (section->dict,
				GINT_TO_POINTER (id), name);
		}

		return NULL;
	}

	d (printf ("%u) ", i););
	prop_name = msole_prop_id_to_gsf (section, props[i].id);

	d (printf (" @ %x %x = ", (unsigned)props[i].offset, size););
	return msole_prop_parse (type, &data, data + size);
}

static int
msole_prop_cmp (gconstpointer a, gconstpointer b)
{
	GsfMSOleMetaDataProp const *prop_a = a ;
	GsfMSOleMetaDataProp const *prop_b = b ;
	return prop_a->offset - prop_b->offset;
}

gboolean
gsf_msole_metadata_read (GsfInput *in, GError **err)
{
	guint8 const *data = gsf_input_read (in, 28, NULL);
	guint16 version;
	guint32 os, num_sections;
	unsigned i, j;
	GsfMSOleMetaDataSection *sections;
	GsfMSOleMetaDataProp *props;

	if (NULL == data) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
					    "Unable to read MS property stream header");
		return FALSE;
	}

	/* NOTE : high word is the os, low word is the os version
	 * 0 = win16
	 * 1 = mac
	 * 2 = win32
	 */
	os = GSF_LE_GET_GUINT16 (data + 6);
	version = GSF_LE_GET_GUINT16 (data + 2);

	num_sections = GSF_LE_GET_GUINT32 (data + 24);
	if (GSF_LE_GET_GUINT16 (data + 0) != 0xfffe
	    || (version != 0 && version != 1)
	    || os > 2
	    || num_sections > 100) { /* arbitrary sanity check */
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
					    "Invalid MS property stream header");
		return FALSE;
	}

	/* extract the section info */
	sections = g_alloca (sizeof (GsfMSOleMetaDataSection)* num_sections);
	for (i = 0 ; i < num_sections ; i++) {
		data = gsf_input_read (in, 20, NULL);
		if (NULL == data) {
			if (err != NULL)
				*err = g_error_new (gsf_input_error (), 0,
						    "Unable to read MS property stream header");
			return FALSE;
		}
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
#ifndef NO_DEBUG_OLE_PROPS
		printf ("0x%x\n", (guint32)sections [i].offset);
#endif
	}
	for (i = 0 ; i < num_sections ; i++) {
		if (gsf_input_seek (in, sections[i].offset, G_SEEK_SET) ||
		    NULL == (data = gsf_input_read (in, 8, NULL))) {
			if (err != NULL)
				*err = g_error_new (gsf_input_error (), 0,
						    "Invalid MS property section");
			return FALSE;
		}

		sections[i].iconv     = (GIConv)-1;
		sections[i].dict      = NULL;
		sections[i].size      = GSF_LE_GET_GUINT32 (data); /* includes header */
		sections[i].num_props = GSF_LE_GET_GUINT32 (data + 4);
		if (sections[i].num_props <= 0)
			continue;
		props = g_new (GsfMSOleMetaDataProp, sections[i].num_props);
		for (j = 0; j < sections[i].num_props; j++) {
			if (NULL == (data = gsf_input_read (in, 8, NULL))) {
				g_free (props);
				if (err != NULL)
					*err = g_error_new (gsf_input_error (), 0,
							    "Invalid MS property section");
				return FALSE;
			}

			props [j].id = GSF_LE_GET_GUINT32 (data);
			props [j].offset  = GSF_LE_GET_GUINT32 (data + 4);
		}

		/* order prop info by offset to facilitate bounds checking */
		qsort (props, sections[i].num_props,
		       sizeof (GsfMSOleMetaDataProp),
		       msole_prop_cmp);

		for (j = 0; j < sections[i].num_props; j++) /* first codepage */
			if (props[j].id == 1) {
			}
		for (j = 0; j < sections[i].num_props; j++) /* then dictionary */
			if (props[j].id == 0)
				msole_prop_read (in, sections+i, props, j);
		for (j = 0; j < sections[i].num_props; j++) /* the rest */
			if (props[j].id > 1)
				msole_prop_read (in, sections+i, props, j);

		g_free (props);
		gsf_iconv_close (sections[i].iconv);
		if (sections[i].dict != NULL)
			g_hash_table_destroy (sections[i].dict);
	}
	return TRUE;
}

gboolean
gsf_msole_metadata_write (GsfOutput *out, gboolean doc_not_component,
			  GError **err)
{
	guint8 buf[8];
	guint8 const *guid;
	gboolean has_user_defined = FALSE;
	guint8 header[] = {
		0xfe, 0xff,	/* byte order */
		0, 0,		/* no one seems to use version 1 */
		0x04, 0x0a, 0x02, 0x00,	/* win32 version (xp = a04, nt/2k = 04 ?) */
		0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,	/* clasid = 0 */
		1, 0, 0, 0,				/* 1 section FIXME */
	};

	/* TODO : add a test to set this */
	if (has_user_defined)
		GSF_LE_SET_GUINT32 (header + sizeof (header) - 4, 2);
	gsf_output_write (out, sizeof (header), header);
	guid = doc_not_component ? document_guid : component_guid;
	gsf_output_write (out, 16, guid);

	GSF_LE_SET_GUINT32 (buf, sizeof (header) + 16 /* guid */ + 4 /* this size */);
	gsf_output_write (out, 4, buf);

	GSF_LE_SET_GUINT32 (buf+0, 8);	/* 8 bytes in header, 0 bytes in props */
	GSF_LE_SET_GUINT32 (buf+4, 0);	/* 0 props */
	gsf_output_write (out, 8, buf);

	return TRUE;
}

typedef struct
{
	char const * const * keys;/*NULL-terminated list*/
	int value;
} s_hash_entry;

/* here is a list of languages for which cp1251 is used on Windows*/
static char const * const cyr_locales[] =
{
	"be", "be_BY", "bulgarian", "bg", "bg_BG", "mk", "mk_MK",
	"russian", "ru", "ru_RU", "ru_UA", "sp", "sp_YU", "sr", "sr_YU",
	"ukrainian", "uk", "uk_UA", NULL
};


/* here is a list of languages for which cp for cjk is used on Windows*/
static char const * const jp_locales[] =
{
	"japan", "japanese", "ja", "ja_JP", NULL
};

static char const * const zhs_locales[] =
{
	"chinese-s", "zh", "zh_CN", NULL
};

static char const * const kr_locales[] =
{
	"korean", "ko", "ko_KR", NULL
};

static char const * const zht_locales[] =
{
	"chinese-t", "zh_HK", "zh_TW", NULL
 };
 
static s_hash_entry const win_codepages[]=
{
	{ cyr_locales , 1251 },
	{ jp_locales  , 932 },
	{ zhs_locales , 936 },
	{ kr_locales  , 949 },
	{ zht_locales , 950 },
	{ NULL, 0 } /*terminator*/
};

/**
 * gsf_msole_iconv_win_codepage :
 *
 * Our best guess at the applicable windows code page based on an environment
 * variable or the current locale.
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
		s_hash_entry const *entry;
		char const * const *key;

		for (entry = win_codepages; entry->keys; ++entry) {
			for (key = entry->keys; *key; ++key)
				if (!g_strcasecmp (*key, lang)) {
					g_free (lang);
					return entry->value;
				}
		}
		g_free (lang);
	}
	return 1252; /* default ansi */
}

/**
 * gsf_msole_iconv_open_for_import :
 * @codepage :
 *
 * Open an iconv converter for single byte encodings @codepage -> utf8.
 * Attempt to handle the semantics of a specification for multibyte encodings
 * since this is only supposed to be used for single bytes.
 **/
GIConv
gsf_msole_iconv_open_for_import (guint codepage)
{
	GIConv iconv_handle;

	if (codepage != 1200 && codepage != 1201) {
		char* src_charset = g_strdup_printf ("CP%d", codepage);
		iconv_handle = g_iconv_open ("UTF-8", src_charset);
		g_free (src_charset);
		if (iconv_handle != (GIConv)(-1))
			return iconv_handle;
		g_warning ("Unknown codepage %d", codepage);
		return (GIConv)(-1);
	}

	/* this is 'compressed' unicode.  unicode characters 0000->00FF
	 * which looks the same as 8859-1.  What does Little endian vs
	 * bigendian have to do with this.  There is only 1 byte, and it would
	 * certainly not be useful to keep the low byte as 0.
	 */
	return g_iconv_open ("UTF-8", "ISO-8859-1");
}

/**
 * gsf_msole_iconv_open_for_export :
 *
 * Open an iconv convert to go from utf8 -> to our best guess at a useful
 * windows codepage.
 **/
GIConv
gsf_msole_iconv_open_for_export (void)
{
	char *dest_charset = g_strdup_printf ("CP%d", gsf_msole_iconv_win_codepage());
	GIConv iconv_handle = g_iconv_open (dest_charset, "UTF-8");
	g_free (dest_charset);
	return iconv_handle;
}
