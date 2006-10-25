/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-opendoc-utils.c:  Handle the application neutral portions of OpenDocument
 *
 * Author:  Luciano Wolf (luciano.wolf@indt.org.br)
 *
 * Copyright (C) 2005 INdT - Instituto Nokia de Tecnologia
 * http://www.indt.org.br
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf-opendoc-utils.h>
#include <gsf/gsf-meta-names.h>
#include <gsf/gsf-doc-meta-data.h>
#include <gsf/gsf-timestamp.h>
#include <gsf/gsf-docprop-vector.h>
#include <string.h>

#define OFFICE	 "office:"

typedef struct {
	GsfDocMetaData	 *md;
	GsfDocPropVector *keywords;
	GError		 *err;
} GsfOOMetaIn;

/* Generated based on:
 * http://www.oasis-open.org/committees/download.php/12572/OpenDocument-v1.0-os.pdf */
GsfXMLInNS gsf_ooo_ns[] = {
	/* OOo 1.0.x & 1.1.x */
	GSF_XML_IN_NS (OO_NS_OFFICE,	"http://openoffice.org/2000/office"),
	GSF_XML_IN_NS (OO_NS_STYLE,	"http://openoffice.org/2000/style"),
	GSF_XML_IN_NS (OO_NS_TEXT,	"http://openoffice.org/2000/text"),
	GSF_XML_IN_NS (OO_NS_TABLE,	"http://openoffice.org/2000/table"),
	GSF_XML_IN_NS (OO_NS_DRAW,	"http://openoffice.org/2000/drawing"),
	GSF_XML_IN_NS (OO_NS_NUMBER,	"http://openoffice.org/2000/datastyle"),
	GSF_XML_IN_NS (OO_NS_CHART,	"http://openoffice.org/2000/chart"),
	GSF_XML_IN_NS (OO_NS_DR3D,	"http://openoffice.org/2000/dr3d"),
	GSF_XML_IN_NS (OO_NS_FORM,	"http://openoffice.org/2000/form"),
	GSF_XML_IN_NS (OO_NS_SCRIPT,	"http://openoffice.org/2000/script"),
	GSF_XML_IN_NS (OO_NS_CONFIG,	"http://openoffice.org/2001/config"),
	GSF_XML_IN_NS (OO_NS_MATH,	"http://www.w3.org/1998/Math/MathML"),	/* also in 2.0 */
	GSF_XML_IN_NS (OO_NS_FO,	"http://www.w3.org/1999/XSL/Format"),
	GSF_XML_IN_NS (OO_NS_XLINK,	"http://www.w3.org/1999/xlink"),	/* also in 2.0 */
	GSF_XML_IN_NS (OO_NS_SVG,	"http://www.w3.org/2000/svg"),

	/* OOo 1.9.x & 2.0.x */
	GSF_XML_IN_NS (OO_NS_OFFICE,	"urn:oasis:names:tc:opendocument:xmlns:office:1.0"),
	GSF_XML_IN_NS (OO_NS_STYLE,	"urn:oasis:names:tc:opendocument:xmlns:style:1.0"),
	GSF_XML_IN_NS (OO_NS_TEXT,	"urn:oasis:names:tc:opendocument:xmlns:text:1.0"),
	GSF_XML_IN_NS (OO_NS_TABLE,	"urn:oasis:names:tc:opendocument:xmlns:table:1.0"),
	GSF_XML_IN_NS (OO_NS_DRAW,	"urn:oasis:names:tc:opendocument:xmlns:drawing:1.0"),
	GSF_XML_IN_NS (OO_NS_FO,	"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"),
	GSF_XML_IN_NS (OO_NS_META,	"urn:oasis:names:tc:opendocument:xmlns:meta:1.0"),
	GSF_XML_IN_NS (OO_NS_NUMBER,	"urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0"),
	GSF_XML_IN_NS (OO_NS_SVG,	"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0"),
	GSF_XML_IN_NS (OO_NS_CHART,	"urn:oasis:names:tc:opendocument:xmlns:chart:1.0"),
	GSF_XML_IN_NS (OO_NS_DR3D,	"urn:oasis:names:tc:opendocument:xmlns:dr3d:1.0"),
	GSF_XML_IN_NS (OO_NS_FORM,	"urn:oasis:names:tc:opendocument:xmlns:form:1.0"),
	GSF_XML_IN_NS (OO_NS_SCRIPT,	"urn:oasis:names:tc:opendocument:xmlns:script:1.0"),
	GSF_XML_IN_NS (OO_NS_DC,	"http://purl.org/dc/elements/1.1/"),
	GSF_XML_IN_NS (OO_NS_OOO,	"http://openoffice.org/2004/office"),
	GSF_XML_IN_NS (OO_NS_OOOW,	"http://openoffice.org/2004/writer"),
	GSF_XML_IN_NS (OO_NS_OOOC,	"http://openoffice.org/2004/calc"),
	GSF_XML_IN_NS (OO_NS_DOM,	"http://www.w3.org/2001/xml-events"),
	GSF_XML_IN_NS (OO_NS_XFORMS,	"http://www.w3.org/2002/xforms"),
	GSF_XML_IN_NS (OO_NS_XSD,	"http://www.w3.org/2001/XMLSchema"),
	GSF_XML_IN_NS (OO_NS_XSI,	"http://www.w3.org/2001/XMLSchema-instance"),
	{ NULL, 0 }
};

static void
od_get_meta_prop (GsfXMLIn *xin, char const *prop_name, GType g_type)
{
	GValue *res = g_new0 (GValue, 1);
	if (gsf_xml_gvalue_from_str (res, g_type, xin->content->str))
		gsf_doc_meta_data_insert (((GsfOOMetaIn *)xin->user_state)->md,
			g_strdup (prop_name), res);
	else
		g_free (res);
}

/* Avoid duplication */
#define OO_PROP(tag, name, type)					\
static void								\
od_meta_ ## tag (GsfXMLIn *xin, G_GNUC_UNUSED GsfXMLBlob *blob)	\
{									\
	od_get_meta_prop (xin, name, type);				\
}
OO_PROP(generator,		GSF_META_NAME_GENERATOR,	G_TYPE_STRING)
OO_PROP(title,			GSF_META_NAME_TITLE,		G_TYPE_STRING)
OO_PROP(description,		GSF_META_NAME_DESCRIPTION,	G_TYPE_STRING)
OO_PROP(subject,		GSF_META_NAME_SUBJECT,		G_TYPE_STRING)
OO_PROP(initial_creator,	GSF_META_NAME_INITIAL_CREATOR,	G_TYPE_STRING)
/* OD considers this the last person to modify the doc, rather than
 * the DC convention of the person primarilly responsible for its creation */
OO_PROP(creator,		GSF_META_NAME_CREATOR,		G_TYPE_STRING)
/* last to print */
OO_PROP(printed_by,		GSF_META_NAME_PRINTED_BY,	G_TYPE_STRING)

OO_PROP(date_created,		GSF_META_NAME_DATE_CREATED,	GSF_TIMESTAMP_TYPE)
OO_PROP(date_modified,		GSF_META_NAME_DATE_MODIFIED,	GSF_TIMESTAMP_TYPE)
OO_PROP(print_date,		GSF_META_NAME_LAST_PRINTED,	GSF_TIMESTAMP_TYPE)

OO_PROP(language,		GSF_META_NAME_LANGUAGE,		G_TYPE_STRING)
OO_PROP(editing_cycles,		GSF_META_NAME_REVISION_COUNT,	G_TYPE_UINT)
/* FIXME FIXME FIXME should be durations using format 'PnYnMnDTnHnMnS' */
OO_PROP(editing_duration,	GSF_META_NAME_EDITING_DURATION, G_TYPE_STRING)

/* OD allows multiple keywords, accumulate things and make it an array */
static void
od_meta_keyword (GsfXMLIn *xin, G_GNUC_UNUSED GsfXMLBlob *blob)
{
	GsfOOMetaIn *mi = (GsfOOMetaIn *)xin->user_state;
	GValue *v = g_new0 (GValue, 1);

	if (NULL == mi->keywords)
		mi->keywords = gsf_docprop_vector_new ();

	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, xin->content->str);
	gsf_docprop_vector_append (mi->keywords, v);
	g_value_unset (v);
	g_free (v);
}

static void
od_meta_user_defined (G_GNUC_UNUSED GsfXMLIn *xin, G_GNUC_UNUSED GsfXMLBlob *blob)
{
}

#if 0
/* These need special handling for attributes */
template
auto_reload
hl_behavior
doc_stats
#endif

static GsfXMLInNode const gsf_opendoc_meta_dtd[] = {
  GSF_XML_IN_NODE_FULL (START, START, -1, NULL, FALSE, FALSE, TRUE, NULL, NULL, 0),
  GSF_XML_IN_NODE (START, META, OO_NS_OFFICE, "meta", FALSE, NULL, NULL),
    /* OpenDocument TAGS */
    GSF_XML_IN_NODE (META, META_GENERATOR,	OO_NS_META, "generator", TRUE, NULL, &od_meta_generator),
    GSF_XML_IN_NODE (META, META_TITLE,		OO_NS_DC, "title", TRUE, NULL, &od_meta_title),
    GSF_XML_IN_NODE (META, META_DESCRIPTION,	OO_NS_DC, "description", TRUE, NULL, &od_meta_description),
    GSF_XML_IN_NODE (META, META_SUBJECT,	OO_NS_DC, "subject", TRUE, NULL, &od_meta_subject),
    GSF_XML_IN_NODE (META, META_KEYWORD,	OO_NS_META, "keyword", TRUE, NULL, &od_meta_keyword),
    GSF_XML_IN_NODE (META, META_INITIAL_CREATOR, OO_NS_META, "initial-creator", TRUE, NULL, &od_meta_initial_creator),
    GSF_XML_IN_NODE (META, META_CREATOR,	OO_NS_DC, "creator", TRUE, NULL, &od_meta_creator),
    GSF_XML_IN_NODE (META, META_PRINTED_BY,	OO_NS_META, "printed-by", TRUE, NULL, &od_meta_printed_by),
    GSF_XML_IN_NODE (META, META_CREATION_DATE,	OO_NS_META, "creation-date", TRUE, NULL, &od_meta_date_created),
    GSF_XML_IN_NODE (META, META_DATE_MOD,	OO_NS_DC, "date", TRUE, NULL, &od_meta_date_modified),
    GSF_XML_IN_NODE (META, META_PRINT_DATE,	OO_NS_META, "print-date", TRUE, NULL, &od_meta_print_date),

    GSF_XML_IN_NODE (META, META_TEMPLATE,	OO_NS_META, "template", FALSE, NULL, NULL),
    GSF_XML_IN_NODE (META, META_AUTO_RELOAD,	OO_NS_META, "auto-reload", FALSE, NULL, NULL),
    GSF_XML_IN_NODE (META, META_HL_BEHAVIOUR,	OO_NS_META, "hyperlink-behaviour", FALSE, NULL, NULL),
    GSF_XML_IN_NODE (META, META_DOCUMENT_STATS,	OO_NS_META, "document-statistic", FALSE, NULL, NULL),
    GSF_XML_IN_NODE (META, META_LANGUAGE,	OO_NS_DC, "language", TRUE, NULL, &od_meta_language),

    GSF_XML_IN_NODE (META, META_EDITING_CYCLES,	OO_NS_META, "editing-cycles", TRUE, NULL, &od_meta_editing_cycles),
    GSF_XML_IN_NODE (META, META_EDITING_DURATION, OO_NS_META, "editing-duration", TRUE, NULL, &od_meta_editing_duration),

    GSF_XML_IN_NODE (META, META_USER_DEFINED,	OO_NS_META, "user-defined", TRUE, NULL, &od_meta_user_defined),
  GSF_XML_IN_NODE_END
};

/**
 * gsf_opendoc_metadata_read :
 * @input : #GsfInput
 * @md    : #GsfDocMetaData
 *
 * Read an OpenDocument metadata stream from @input and store the properties
 * into @md.  Overwrite any existing properties with the same id.
 * Returns a GError if there is a problem.
 **/
GError *
gsf_opendoc_metadata_read (GsfInput *input, GsfDocMetaData *md)
{
	GsfXMLInDoc	*doc;
	GsfOOMetaIn	 state;

	state.md  = md;
	state.keywords = NULL;
	state.err = NULL;

	doc = gsf_xml_in_doc_new (gsf_opendoc_meta_dtd, gsf_ooo_ns);
	gsf_xml_in_doc_parse (doc, input, &state);
	gsf_xml_in_doc_free (doc);

	if (state.keywords) {
		GValue *val = g_new0 (GValue, 1);
		g_value_init (val, GSF_DOCPROP_VECTOR_TYPE);
		g_value_set_object (val, state.keywords);
		gsf_doc_meta_data_insert (md,
			g_strdup (GSF_META_NAME_KEYWORDS), val);
		g_object_unref (state.keywords);
	}

	return state.err;
}

static void
gsf_opendoc_metadata_subtree_free (GsfXMLIn *xin, G_GNUC_UNUSED gpointer old_state)
{
	gsf_xml_in_doc_free (xin->user_state);
}

/**
 * gsf_opendoc_metadata_subtree :
 * @doc : #GsfXMLInDoc
 * @md  : #GsfDocMetaData
 *
 * Extend @xin so that it can parse a subtree in OpenDoc metadata format
 **/
void
gsf_opendoc_metadata_subtree (GsfXMLIn *xin, GsfDocMetaData *md)
{
	GsfXMLInDoc *doc = gsf_xml_in_doc_new (gsf_opendoc_meta_dtd+1, gsf_ooo_ns);
	gsf_xml_in_push_state (xin, doc, md, &gsf_opendoc_metadata_subtree_free,
		NULL);
}

static char const *
od_map_prop_name (char const *name)
{
	/* shared by all instances and never freed */
	static GHashTable *od_prop_name_map = NULL;

	if (NULL == od_prop_name_map) 
	{
		static struct {
			char const *gsf_key;
			char const *od_key;
		} const map [] = {
			{ GSF_META_NAME_GENERATOR,	"meta:generator" },
			{ GSF_META_NAME_TITLE,		"dc:title" },
			{ GSF_META_NAME_DESCRIPTION,	"dc:description" },
			{ GSF_META_NAME_SUBJECT,	"dc:subject" },
			{ GSF_META_NAME_INITIAL_CREATOR,"meta:initial-creator" },
			{ GSF_META_NAME_CREATOR,	"dc:creator" },
			{ GSF_META_NAME_PRINTED_BY,	"meta:printed-by" },
			{ GSF_META_NAME_DATE_CREATED,	"meta:creation-date" },
			{ GSF_META_NAME_DATE_MODIFIED,	"dc:date" },
			{ GSF_META_NAME_LAST_PRINTED,	"meta:print-date" },
			{ GSF_META_NAME_LANGUAGE,	"dc:language" },
			{ GSF_META_NAME_REVISION_COUNT,	"meta:editing-cycles" },
			{ GSF_META_NAME_EDITING_DURATION, "meta:editing-duration" }
		};
		int i = G_N_ELEMENTS (map);

		od_prop_name_map = g_hash_table_new (g_str_hash, g_str_equal);
		while (i-- > 0)
			g_hash_table_insert (od_prop_name_map,
				(gpointer)map[i].gsf_key,
				(gpointer)map[i].od_key);
	}

	return g_hash_table_lookup (od_prop_name_map, name);
}

#if 0
meta:page-count		GSF_META_NAME_PAGE_COUNT
meta:table-count	GSF_META_NAME_TABLE_COUNT:
meta:draw-count
meta:image-count	GSF_META_NAME_IMAGE_COUNT:
meta:ole-object-count	GSF_META_NAME_OBJECT_COUNT:
meta:paragraph-count	GSF_META_NAME_PARAGRAPH_COUNT:
meta:word-count
meta:character-count	GSF_META_NAME_CHARACTER_COUNT
meta:row-count		GSF_META_NAME_LINE_COUNT:
meta:frame-count
meta:sentence-count
meta:syllable-count
meta:non-whitespace-character-count

meta:page-count
	GSF_META_NAME_SPREADSHEET_COUNT
meta:table-count
	GSF_META_NAME_TABLE_COUNT:
meta:image-count
	* GSF_META_NAME_IMAGE_COUNT:
meta:cell-count
	GSF_META_NAME_CELL_COUNT
meta:object-count
	GSF_META_NAME_OBJECT_COUNT:

meta:page-count
	 GSF_META_NAME_SLIDE_COUNT:
meta:image-count
	GSF_META_NAME_IMAGE_COUNT:
meta:object-count
	GSF_META_NAME_OBJECT_COUNT:
#endif

static void
meta_write_props (char const *prop_name, GsfDocProp *prop, GsfXMLOut *output)
{
	char const *mapped_name;
	GValue const *val = gsf_doc_prop_get_val (prop);

	/* Handle specially */
	if (0 == strcmp (prop_name, GSF_META_NAME_KEYWORDS)) {
		GValueArray *va;
		unsigned i;
		char *str;

		/* OLE2 stores a single string, with no obvious
		 * standard for seperator */
		if (G_TYPE_STRING == G_VALUE_TYPE (val)) {
			str = g_value_dup_string (val);
			if (str && *str) {
				gsf_xml_out_start_element (output, "meta:keyword");
				gsf_xml_out_add_cstr (output, NULL, str);
			}
			g_free (str);
		} else if (NULL != (va = gsf_value_get_docprop_varray (val))) {
			for (i = 0 ; i < va->n_values; i++) {
				str = g_value_dup_string (g_value_array_get_nth	(va, i));
				gsf_xml_out_start_element (output, "meta:keyword");
				gsf_xml_out_add_cstr (output, NULL, str);
				g_free (str);
			}
		}
		return;
	}

	if (NULL == (mapped_name = od_map_prop_name (prop_name))) {
		GType t;
		char const *type_name = NULL;

		gsf_xml_out_start_element (output, "meta:user-defined");
		gsf_xml_out_add_cstr (output, "meta:name", prop_name);

		if (NULL == val)
			gsf_xml_out_end_element (output);

		switch ((t = G_VALUE_TYPE (val))) {
		case G_TYPE_CHAR:
		case G_TYPE_UCHAR:
		case G_TYPE_STRING:
		case G_TYPE_ENUM:
		case G_TYPE_FLAGS:
			type_name = "string";
			break;
		case G_TYPE_BOOLEAN:
			type_name = "boolean";
			break;
		case G_TYPE_INT:
		case G_TYPE_UINT:
		case G_TYPE_LONG:
		case G_TYPE_ULONG:
		case G_TYPE_FLOAT:
		case G_TYPE_DOUBLE:
			type_name = "float";
			break; 

		default:
			if (GSF_TIMESTAMP_TYPE == t)
				type_name = "data";
		}
		gsf_xml_out_add_cstr (output, "meta:type", type_name);
	} else
		gsf_xml_out_start_element (output, mapped_name);
	gsf_xml_out_add_gvalue (output, NULL, val);
	gsf_xml_out_end_element (output);
}

gboolean
gsf_opendoc_metadata_write (GsfXMLOut *output, GsfDocMetaData const *md)
{
	if (output == NULL)
		return FALSE;

	gsf_xml_out_start_element (output, OFFICE "document-meta");
	gsf_xml_out_add_cstr_unchecked (output, "xmlns:office",
		"urn:oasis:names:tc:opendocument:xmlns:office:1.0");
	gsf_xml_out_add_cstr_unchecked (output, "xmlns:xlink",
		"http://www.w3.org/1999/xlink");
	gsf_xml_out_add_cstr_unchecked (output, "xmlns:dc",
		"http://purl.org/dc/elements/1.1/");
	gsf_xml_out_add_cstr_unchecked (output, "xmlns:meta",
		"urn:oasis:names:tc:opendocument:xmlns:meta:1.0");
	gsf_xml_out_add_cstr_unchecked (output, "xmlns:ooo",
		"http://openoffice.org/2004/office");
	gsf_xml_out_add_cstr_unchecked (output, "office:version", "1.0");

	gsf_xml_out_start_element (output, OFFICE "meta");
	gsf_doc_meta_data_foreach (md, (GHFunc) meta_write_props, output);
	gsf_xml_out_end_element (output); /* </office:meta> */

	gsf_xml_out_end_element (output); /* </office:document-meta> */

	return TRUE;
}
