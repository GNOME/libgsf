/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-msole-utils.c: 
 *
 * Copyright (C) 2002 Jody Goldberg (jody@gnome.org)
 * Copyright (C) 2002 Dom Lachowicz (cinamod@hotmail.com)
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
	GIConv	    iconv_handle;
	unsigned    char_size;
	GHashTable *dict;
} GsfMSOleMetaDataSection;

static GsfMSOleMetaDataPropMap const document_props[] = {
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

static GsfMSOleMetaDataPropMap const common_props[] = {
	{ "Dictionary",		0,	0, /* magic */},
	{ "CodePage",		1,	VT_UI2 },
	{ "LOCALE_SYSTEM_DEFAULT",	0x80000000,	VT_UI4},
	{ "CASE_SENSITIVE",		0x80000003,	VT_UI4},
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

	map = common_props;
	i = G_N_ELEMENTS (common_props);
	while (i-- > 0)
		if (map[i].id == id) {
			d (printf (map[i].name););
			return map[i].name;
		}

	d (printf ("_UNKNOWN_(0x%x %d)", id, id););

	return NULL;
}

static GValue *
msole_prop_parse (GsfMSOleMetaDataSection *section,
		  guint32 type, guint8 const **data, guint8 const *data_end)
{
	GValue *res;
	char *str;
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
			GValue *v;
			d (printf ("\t[%d] ", i););
			v = msole_prop_parse (section, type, data, data_end);
			if (!v) {
				/* FIXME: do something with it.  */
				g_value_unset (v);
				g_free (v);
			}
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
		return msole_prop_parse (section, type, data, data_end);

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

		str = g_convert_with_iconv (*data + 4,
			len * section->char_size,
			section->iconv_handle, &len, NULL, NULL);

		g_value_init (res, G_TYPE_STRING);
		g_value_set_string (res, str);
		g_free (str);
		*data += 4 + len;
		break;

	case VT_LPWSTR : d (puts ("VT_LPWSTR"););
		/* be anal and safe */
		g_return_val_if_fail (*data + 4 <= data_end, NULL);

		len = GSF_LE_GET_GUINT32 (*data);

		g_return_val_if_fail (*data + 4 + len <= data_end, NULL);

		str = g_convert (*data + 4, len*2,
				 "UTF-8", "UTF-16LE", &len, NULL, NULL);

		g_value_init (res, G_TYPE_STRING);
		g_value_set_string (res, str);
		g_free (str);
		*data += 4 + len;
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
		guint8 const *start = data;

		g_return_val_if_fail (section->dict == NULL, NULL);

		section->dict = g_hash_table_new_full (
			g_direct_hash, g_direct_equal,
			NULL, g_free);

		d (gsf_mem_dump (data-4, size););
		n = type;
		for (i = 0 ; i < n ; i++) {
			id = GSF_LE_GET_GUINT32 (data);
			len = GSF_LE_GET_GUINT32 (data + 4);

			g_return_val_if_fail (len < 0x10000, NULL);

			name = g_convert_with_iconv (data + 8,
				len * section->char_size,
				section->iconv_handle, &len, NULL, NULL);
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

		return NULL;
	}

	d (printf ("%u) ", i););
	prop_name = msole_prop_id_to_gsf (section, props[i].id);

	d (printf (" @ %x %x = ", (unsigned)props[i].offset, size););
	return msole_prop_parse (section, type, &data, data + size);
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

		sections[i].iconv_handle = (GIConv)-1;
		sections[i].char_size    = 1;
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

		sections[i].iconv_handle = (GIConv)-1;
		sections[i].char_size = 1;
		for (j = 0; j < sections[i].num_props; j++) /* first codepage */
			if (props[j].id == 1) {
				GValue *v = msole_prop_read (in, sections+i, props, j);
				if (v != NULL) {
					int codepage = g_value_get_int (v);
					sections[i].iconv_handle = gsf_msole_iconv_open_for_import (codepage);
					if (codepage == 1200 || codepage == 1201)
						sections[i].char_size = 2;
					g_value_unset (v);
					g_free (v) ;
				}
			}
		if (sections[i].iconv_handle == (GIConv)-1)
			sections[i].iconv_handle = gsf_msole_iconv_open_for_import (1252);

		for (j = 0; j < sections[i].num_props; j++) /* then dictionary */
			if (props[j].id == 0) {
				GValue *v = msole_prop_read (in, sections+i, props, j);
				if (v) {
					/* FIXME: do something with it.  */
					g_value_unset (v);
					g_free (v);
				}
			}
		for (j = 0; j < sections[i].num_props; j++) /* the rest */
			if (props[j].id > 1) {
				GValue *v = msole_prop_read (in, sections+i, props, j);
				if (v) {
					/* FIXME: do something with it.  */
					g_value_unset (v);
					g_free (v);
				}
			}

		gsf_iconv_close (sections[i].iconv_handle);
		g_free (props);
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
	if (!gsf_output_write (out, sizeof (header), header))
		goto err;
	guid = doc_not_component ? document_guid : component_guid;
	if (!gsf_output_write (out, 16, guid))
		goto err;

	GSF_LE_SET_GUINT32 (buf, sizeof (header) + 16 /* guid */ + 4 /* this size */);
	if (!gsf_output_write (out, 4, buf))
		goto err;

	GSF_LE_SET_GUINT32 (buf+0, 8);	/* 8 bytes in header, 0 bytes in props */
	GSF_LE_SET_GUINT32 (buf+4, 0);	/* 0 props */
	if (!gsf_output_write (out, 8, buf))
		goto err;

	return TRUE;

err:

	if (err != NULL)
		*err = g_error_copy (gsf_output_error (out));
	return FALSE;
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
	{ "al_AL",  0x041c }, /* Albanian */
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
	{ "nb_NO",  0x0414 }, /* Norwegian (Bokmai) */
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
 *
 * Return the LID (Language Identifier) for the input language.
 * If lang is %null, return 0x0400 ("-none-"), and not 0x0000 ("no proofing")
 */
guint
gsf_msole_lid_for_language (const char * lang)
{
	guint i = 0 ;

	if (lang == NULL)
		return 0x0400;   /* return -none- */
	
	for (i = 0 ; i < G_N_ELEMENTS(gsf_msole_language_ids); i++)
		if (!strcmp (lang, gsf_msole_language_ids[i].tag))
			return gsf_msole_language_ids[i].lid;
	
	return 0x0400 ;   /* return -none- */
}

/**
 * gsf_msole_language_for_lid
 *
 * Returns the xx_YY style string (can be just xx or xxx) for the given LID.
 * Return value must not be freed. If the LID is not found, is set to 0x0400,
 * or is set to 0x0000, will return "-none-"
 */
G_CONST_RETURN char *
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
 */
guint
gsf_msole_codepage_to_lid (guint codepage)
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
 *
 * Our best guess at the codepage for the given language id
 */
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
 * 
 * Returns the Iconv codepage string for the given LID.
 * Return value must be g_free()'d
 */
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
		guint lid = gsf_msole_lid_for_language (lang);
		g_free (lang);
		return gsf_msole_lid_to_codepage (lid);
	}
	return 1252; /* default ansi */
}

/**
 * gsf_msole_iconv_open_codepage_for_import :
 * @to
 * @codepage :
 *
 * Open an iconv converter for @codepage -> utf8.
 **/
GIConv
gsf_msole_iconv_open_codepage_for_import (const char * to, guint codepage)
{
	GIConv iconv_handle;

	g_return_val_if_fail (to != NULL, (GIConv)(-1));

	if (codepage != 1200 && codepage != 1201) {
		char* src_charset = g_strdup_printf ("CP%d", codepage);
		iconv_handle = g_iconv_open (to, src_charset);
		g_free (src_charset);
		if (iconv_handle != (GIConv)(-1))
			return iconv_handle;
		g_warning ("Unknown codepage %d", codepage);
		return (GIConv)(-1);
	}

	iconv_handle = g_iconv_open (to,
		(codepage == 1200) ? "UTF-16LE" : "UTF-16BE");
	if (iconv_handle != (GIConv)(-1))
		return iconv_handle;
	g_warning ("Unable to open an iconv handle from %s -> %s",
		   (codepage == 1200) ? "UTF-16LE" : "UTF-16BE", to);
	return (GIConv)(-1);
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
	return gsf_msole_iconv_open_codepage_for_import ("UTF-8", codepage);
}

/**
 * gsf_msole_iconv_open_for_export :
 *
 * Open an iconv convert to go from utf8 -> to our best guess at a useful
 * windows codepage.
 **/
GIConv
gsf_msole_iconv_open_codepages_for_export (guint codepage_to, char const *from)
{
	char *dest_charset = g_strdup_printf ("CP%d", codepage_to);
	GIConv iconv_handle;

	if (from == NULL) {
		g_warning ("No codepage supplied. Assuming UTF-8\n");
		from = "UTF-8";
	}

	iconv_handle = g_iconv_open (dest_charset, from);
	g_free (dest_charset);
	return iconv_handle;
}

/**
 * gsf_msole_iconv_open_for_export :
 * @to
 *
 * Open an iconv convert to go from utf8 -> to our best guess at a useful
 * windows codepage.
 **/
GIConv
gsf_msole_iconv_open_codepage_for_export (guint codepage_to)
{
	return gsf_msole_iconv_open_codepages_for_export (codepage_to, "UTF-8");
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
	return gsf_msole_iconv_open_codepage_for_export (gsf_msole_iconv_win_codepage ());
}
