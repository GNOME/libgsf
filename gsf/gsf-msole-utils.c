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
#include <stdio.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>

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

static GValue *
gsf_msole_prop_parse (guint32 type, guint8 const *data)
{
	if (type & 0x6000) /* not valid in a prop set */
		return NULL;

	if (type & 0x1000) /* as array */
		return NULL;
	type &= 0xfff;
	switch (type) {
	case 0 : /* VT_EMPTY */
		puts ("VT_EMPTY");
		break;
	case 1 : /* VT_NULL */
		puts ("VT_NULL");
		break;
	case 2 : /* VT_I2 */
		puts ("VT_I2");
		break;
	case 3 : /* VT_I4 */
		puts ("VT_I4");
		break;
	case 4 : /* VT_R4 */
		puts ("VT_R4");
		break;
	case 5 : /* VT_R8 */
		puts ("VT_R8");
		break;
	case 6 : /* VT_CY */
		puts ("VT_CY");
		break;
	case 7 : /* VT_DATE */
		puts ("VT_DATE");
		break;
	case 8 : /* VT_BSTR */
		puts ("VT_BSTR");
		break;
	case 9 : /* VT_DISPATCH */
		puts ("VT_DISPATCH");
		break;
	/* case 10 : VT_ERROR not valid in prop sets */
	case 11 : /* VT_BOOL */
		puts ("VT_BOOL");
		break;
	case 12 : /* VT_VARIANT */
		puts ("VT_VARIANT");
		break;
	/* case 13 : VT_UNKNOWN not valid in prop sets */
	/* case 14 : VT_DECIMAL not valid in prop sets */
	/* case 16 : VT_I1 not valid in prop sets */
	case 17 : /* VT_UI1 */
		puts ("VT_UI1");
		break;
	case 18 : /* VT_UI2 */
		puts ("VT_UI2");
		break;
	case 19 : /* VT_UI4 */
		puts ("VT_UI4");
		break;
	case 20 : /* VT_I8 */
		puts ("VT_I8");
		break;
	case 21 : /* VT_UI8 */
		puts ("VT_UI8");
		break;
	/* case 22 : VT_INT not valid in prop sets */
	/* case 23 : VT_UINT not valid in prop sets */
	/* case 24 : VT_VOID not valid in prop sets */
	/* case 25 : VT_HRESULT not valid in prop sets */
	/* case 26 : VT_PTR not valid in prop sets */
	/* case 27 : VT_SAFEARRAY not valid in prop sets */
	/* case 28 : VT_CARRAY not valid in prop sets */
	/* case 29 : VT_USERDEFINED not valid in prop sets */
	case 30 : /* VT_LPSTR */
		puts ("VT_LPSTR");
		break;
	case 31 : /* VT_LPWSTR */
		puts ("VT_LPWSTR");
		break;
	case 64 : /* VT_FILETIME */
		puts ("VT_FILETIME");
		break;
	case 65 : /* VT_BLOB */
		puts ("VT_BLOB");
		break;
	case 66 : /* VT_STREAM */
		puts ("VT_STREAM");
		break;
	case 67 : /* VT_STORAGE */
		puts ("VT_STORAGE");
		break;
	case 68 : /* VT_STREAMED_OBJECT */
		puts ("VT_STREAMED_OBJECT");
		break;
	case 69 : /* VT_STORED_OBJECT */
		puts ("VT_STORED_OBJECT");
		break;
	case 70 : /* VT_BLOB_OBJECT */
		puts ("VT_BLOB_OBJECT");
		break;
	case 71 : /* VT_CF */
		puts ("VT_CF");
		break;
	case 72 : /* VT_CLSID */
		puts ("VT_CLSID");
		break;
	default :
		g_warning ("Unknown property type %d (0x%x)", type, type);
		break;
	};

	return NULL;
}

static GValue *
gsf_msole_prop_read (GsfInput *in,
		     GsfMSOleMetaDataSection *section,
		     GsfMSOleMetaDataProp    *props,
		     unsigned i)
{
	guint32 type;
	guint8 const *data;
	unsigned size = ((i+1) >= section->num_props)
		? section->size : props[i+1].offset;

	g_return_val_if_fail (i < section->num_props, NULL);
	g_return_val_if_fail (size >= props[i].offset + 4, NULL);

	size -= props[i].offset; /* includes the type id */
	if (gsf_input_seek (in, section->offset+props[i].offset, G_SEEK_SET) ||
	    NULL == (data = gsf_input_read (in, size, NULL))) {
		g_warning ("failed to read prop #%d", i);
		return NULL;
	}

	type = GSF_LE_GET_GUINT32 (data);

	printf ("%u) type(%d) %x @ %x %x\n", i, type, props[i].id,
		(unsigned)props[i].offset, size);

	return gsf_msole_prop_parse (type, data);
}

static int
msole_metadata_prop_cmp (gconstpointer a, gconstpointer b)
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
		printf ("0x%x\n", (guint32)sections [i].offset);
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
		       msole_metadata_prop_cmp);

		/* find the codepage if it exists */
		for (j = 0; j < sections[i].num_props; j++)
			if (props[j].id == 1) {
			}
		/* find the dictionary if it exists */
		for (j = 0; j < sections[i].num_props; j++)
			if (props[j].id == 0) {
			}

		for (j = 0; j < sections[i].num_props; j++)
			gsf_msole_prop_read (in, sections+i, props, j);

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
	 * which looks the same as 8859-1.  Little endian vs bigendian has to
	 * do with this.  There is only 1 byte, and it would certainly not be
	 * useful to keep the low byte as 0.
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
