/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-msole-metadata.c: 
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
#include <gsf/gsf-msole-metadata.h>
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
	GsfMSOleMetaDataType type;
	gsf_off_t	     offset;
} GsfMSOleMetaDataSection;

typedef struct {
	guint32		prop_id;
	gsf_off_t	offset;
} GsfMSOleMetaDataProp;

#if 0
{
	SummaryItem *sit;
	gboolean     ok;
	unsigned     i;

	for (i = 0 ; i < EXCEL_TO_GNUM_MAPPING_COUNT; i++) {
		if (excel_to_gnum_mapping[i].ps_id == psid) {
			MsOleSummaryPID  p = excel_to_gnum_mapping[i].excel;
			const gchar *name;

			sit = NULL;
			name = summary_item_name[excel_to_gnum_mapping[i].gnumeric];
			switch (MS_OLE_SUMMARY_TYPE (p)) {

			case MS_OLE_SUMMARY_TYPE_STRING: {
				gchar *val = ms_ole_summary_get_string (si, p, &ok);
				if (ok) {
					char* ans, *ptr = val;
					guint32 lp;
					size_t length = strlen (val);
					size_t inbytes = length,
					outbytes = (length + 2) * 8,
					retlength;
					char* inbuf = g_new (char, length), *outbufptr;
					char const * inbufptr = inbuf;

					ans = g_new (char, outbytes + 1);
					outbufptr = ans;
					for (lp = 0; lp < length; lp++) {
						inbuf[lp] = GSF_LE_GET_GUINT8 (ptr);
						ptr++;
					}

					excel_iconv (current_summary_iconv,
					     &inbufptr, &inbytes,
					     &outbufptr, &outbytes);

					retlength = outbufptr-ans;
					ans[retlength] = 0;
					g_free (inbuf);
					g_free (val);

					sit = summary_item_new_string (name, ans, FALSE);
				}
				break;
			}

			case MS_OLE_SUMMARY_TYPE_BOOLEAN:
			{
				gboolean val = ms_ole_summary_get_boolean (si, p, &ok);
				if (ok)
					sit = summary_item_new_boolean (name, val);
				break;
			}

			case MS_OLE_SUMMARY_TYPE_SHORT:
			{
				guint16 val = ms_ole_summary_get_short (si, p, &ok);
				if (ok)
					sit = summary_item_new_short (name, val);
				break;
			}

			case MS_OLE_SUMMARY_TYPE_LONG:
			{
				guint32 val = ms_ole_summary_get_long (si, p, &ok);
				if (ok)
					sit = summary_item_new_int (name, val);
				break;
			}

			case MS_OLE_SUMMARY_TYPE_TIME:
			{
				GTimeVal val = ms_ole_summary_get_time (si, p, &ok);
				if (ok)
					sit = summary_item_new_time (name, val);
				break;
			}

			default:
				g_warning ("Unsupported summary type:%#x", p);
				break;
			}

			if (sit)
				summary_info_add (sin, sit);
		}
	}
}
#endif

gboolean
gsf_msole_metadata_read (GsfInput *in, GError **err)
{
	guint8 const *data = gsf_input_read (in, 28, NULL);
	guint16 version;
	guint32 os, num_sections, section_size, num_props;
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

		section_size = GSF_LE_GET_GUINT32 (data); /* includes header */
		num_props    = GSF_LE_GET_GUINT32 (data + 4);
		if (num_props <= 0)
			continue;
		props = g_new (GsfMSOleMetaDataProp, num_props);
		for (j = 0; j < num_props; j++) {
			if (NULL == (data = gsf_input_read (in, 8, NULL))) {
				g_free (props);
				if (err != NULL)
					*err = g_error_new (gsf_input_error (), 0,
							    "Invalid MS property section");
				return FALSE;
			}

			props [j].prop_id = GSF_LE_GET_GUINT32 (data);
			props [j].offset  = GSF_LE_GET_GUINT32 (data + 4);
		}

		g_free (props);
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
