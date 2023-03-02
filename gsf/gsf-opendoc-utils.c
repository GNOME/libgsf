/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-opendoc-utils.c:  Handle the application neutral portions of OpenDocument
 *
 * Author:  Luciano Wolf (luciano.wolf@indt.org.br)
 *
 * Copyright (C) 2006 Jody Goldberg (jody@gnome.org)
 * Copyright (C) 2005-2006 INdT - Instituto Nokia de Tecnologia
 * http://www.indt.org.br
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
#include <gsf/gsf-opendoc-utils.h>
#include <gsf/gsf.h>

#include <string.h>

#include <glib/gi18n-lib.h>


#define OFFICE	 "office:"

typedef struct {
	GsfDocMetaData	 *md;
	GsfDocPropVector *keywords;
	GError		 *err;
	char             *name;
	GType            typ;
	GsfXMLInDoc      *doc;
} GsfOOMetaIn;

/**
 * gsf_odf_get_version_string:
 *
 * Gives the ODF version used by libgsf when writing Open Document files.
 *
 * Since: 1.14.24
 *
 * Returns: the ODF version as a string: "1.2".
 **/
char const *
gsf_odf_get_version_string (void)
{
	return "1.2";
}

/**
 * get_gsf_odf_version_string: (skip)
 *
 * Gives the ODF version used by libgsf when writing Open Document files.
 *
 * Deprecated: 1.14.24, use gsf_odf_get_version_string
 *
 * Returns: the ODF version as a string: "1.2".
 **/
char const *
get_gsf_odf_version_string (void)
{
	return "1.2";
}

/**
 * gsf_odf_get_version:
 *
 * Gives the ODF version used by libgsf when writing Open Document files.
 *
 * Since: 1.14.24
 *
 * Returns: the ODF version: 102.
 **/
short
gsf_odf_get_version (void)
{
	return 102;
}

/**
 * get_gsf_odf_version: (skip)
 *
 * Gives the ODF version used by libgsf when writing Open Document files.
 *
 * Deprecated: 1.14.24, use gsf_odf_get_version
 *
 * Returns: the ODF version: 102.
 **/
short
get_gsf_odf_version (void)
{
	return 102;
}

/* Generated based on:
 * http://www.oasis-open.org/committees/download.php/12572/OpenDocument-v1.0-os.pdf */
/* and  OpenDocument-v1.1.pdf */
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
	GSF_XML_IN_NS (OO_NS_PRESENT,	"urn:oasis:names:tc:opendocument:xmlns:presentation:1.0"),

	GSF_XML_IN_NS (OO_NS_DC,	"http://purl.org/dc/elements/1.1/"),
	GSF_XML_IN_NS (OO_NS_OOO,	"http://openoffice.org/2004/office"),
	GSF_XML_IN_NS (OO_NS_OOOW,	"http://openoffice.org/2004/writer"),
	GSF_XML_IN_NS (OO_NS_OOOC,	"http://openoffice.org/2004/calc"),
	GSF_XML_IN_NS (OO_NS_DOM,	"http://www.w3.org/2001/xml-events"),
	GSF_XML_IN_NS (OO_NS_XFORMS,	"http://www.w3.org/2002/xforms"),
	GSF_XML_IN_NS (OO_NS_XSD,	"http://www.w3.org/2001/XMLSchema"),
	GSF_XML_IN_NS (OO_NS_XSI,	"http://www.w3.org/2001/XMLSchema-instance"),

	/* OOo 3.0.x */
	GSF_XML_IN_NS (OO_NS_OF,        "urn:oasis:names:tc:opendocument:xmlns:of:1.2"),
	GSF_XML_IN_NS (OO_NS_FIELD,     "urn:openoffice:names:experimental:ooo-ms-interop:xmlns:field:1.0"),
	GSF_XML_IN_NS (OO_NS_FIELD,     "urn:openoffice:names:experimental:ooxml-odf-interop:xmlns:field:1.0"),
	GSF_XML_IN_NS (OO_NS_FORMX,     "urn:openoffice:names:experimental:ooxml-odf-interop:xmlns:form:1.0"),

	GSF_XML_IN_NS (OO_NS_RPT,       "http://openoffice.org/2005/report"),
	GSF_XML_IN_NS (OO_NS_RDFA,      "http://docs.oasis-open.org/opendocument/meta/rdfa#"),

	/* OOo 3.2.x */
	GSF_XML_IN_NS (OO_NS_GRDDL,     "http://www.w3.org/2003/g/data-view#"),
	GSF_XML_IN_NS (OO_NS_XHTML,     "http://www.w3.org/1999/xhtml"),
	GSF_XML_IN_NS (OO_NS_TABLE_OOO, "http://openoffice.org/2009/table"),

	/* OOo 3.3.x */
	GSF_XML_IN_NS (OO_NS_CHART_OOO, "http://openoffice.org/2010/chart"),

	/* Other OpenDocument v 1.1 */
	GSF_XML_IN_NS (OO_NS_CONFIG,    "urn:oasis:names:tc:opendocument:xmlns:config:1.0"),
	GSF_XML_IN_NS (OO_NS_ANIM,      "urn:oasis:names:tc:opendocument:xmlns:animation:1.0"),
	GSF_XML_IN_NS (OO_NS_DATASTYLE, "urn:oasis:names:tc:opendocument:xmlns:data style:1.0"),
	GSF_XML_IN_NS (OO_NS_MANIFEST,  "urn:oasis:names:tc:opendocument:xmlns:manifest:1.0"),
	GSF_XML_IN_NS (OO_NS_SMIL,      "urn:oasis:names:tc:opendocument:xmlns:smil-compatible:1.0"),


	/* Symphony 1.3 */
	GSF_XML_IN_NS (OO_LOTUS_NS_PRODTOOLS, "http://www.ibm.com/xmlns/prodtools"),

	/* CleverAge ODF Add-in for Microsoft Office 3.0.5224.0 (11.0.8302)*/
	GSF_XML_IN_NS (OO_CLEVERAGE_NS_DC,	"http://purl.org/dc/terms/"),

	/* KOffice 1.6.3 */
	GSF_XML_IN_NS (OO_KDE_NS_KOFFICE, "http://www.koffice.org/2005/"),

	/* Microsoft Excel Formulas in ODF */
	GSF_XML_IN_NS (OO_MS_NS_MSOXL, "http://schemas.microsoft.com/office/excel/formula"),

	/* Gnumeric ODF extensions */
	GSF_XML_IN_NS (OO_GNUM_NS_EXT, "http://www.gnumeric.org/odf-extension/1.0"),

	/* LOCALC */
	GSF_XML_IN_NS (OO_NS_LOCALC_EXT, "urn:org:documentfoundation:names:experimental:office:xmlns:loext:1.0"),
	GSF_XML_IN_NS (OO_NS_LOCALC_EXT2, "urn:org:documentfoundation:names:experimental:calc:xmlns:calcext:1.0"),

	{ NULL, 0 }
};

/**
 * gsf_odf_get_ns:
 *
 * Since: 1.14.24
 *
 * Returns: the used ODF namespace
 **/
GsfXMLInNS const *
gsf_odf_get_ns (void)
{
	return gsf_ooo_ns;
}

/**
 * get_gsf_ooo_ns: (skip)
 *
 * Deprecated: 1.14.24, use gsf_odf_get_ns
 *
 * Returns: the used ODF namespace
 **/
GsfXMLInNS *
get_gsf_ooo_ns (void)
{
	return gsf_ooo_ns;
}


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

#define CXML2C(s) ((char const *)(s))

static void
od_meta_user_defined (GsfXMLIn *xin,  xmlChar const **attrs)
{
	GsfOOMetaIn *mi = (GsfOOMetaIn *)xin->user_state;
	mi->typ = G_TYPE_NONE;
	mi->name = NULL;

	for (; attrs != NULL && attrs[0] && attrs[1] ; attrs += 2) {
		if (!strcmp (CXML2C (attrs[0]), "meta:name"))
			mi->name = g_strdup (CXML2C (attrs[1]));
		else if (!strcmp (CXML2C (attrs[0]), "meta:value-type") ||
			 !strcmp (CXML2C (attrs[0]), "meta:type")) {
				/*
				 * "meta:type" is a typo on the write
				 * side that was
				 * fixed on 20110509.
				 */
			if (!strcmp (CXML2C (attrs[1]), "boolean")) {
				mi->typ = G_TYPE_BOOLEAN;
			} else if (!strcmp (CXML2C (attrs[1]), "float")) {
				mi->typ = G_TYPE_DOUBLE;
			} else if (!strcmp (CXML2C (attrs[1]), "string")) {
				mi->typ = G_TYPE_STRING;
			} else if (!strcmp (CXML2C (attrs[1]), "date") ||
				   !strcmp (CXML2C (attrs[1]), "data")) {
				/*
				 * "data" is a typo on the write side that was
				 * fixed on 20110311.
				 */
				mi->typ = GSF_TIMESTAMP_TYPE;
			} else if (!strcmp (CXML2C (attrs[1]), "time")) {
				mi->typ = G_TYPE_STRING;
				/* We should be able to do better */
			} else {
				/* What? */
			}
		}
	}
	if (mi->name == NULL) /* This should not happen */
		mi->name = g_strdup ("");
}

static void
od_meta_user_defined_end (GsfXMLIn *xin, G_GNUC_UNUSED GsfXMLBlob *blob)
{
	GsfOOMetaIn *mi = (GsfOOMetaIn *)xin->user_state;

	if (mi->name != NULL) {
		GValue *res = g_new0 (GValue, 1);
		GType t = mi->typ;
		if (t == G_TYPE_NONE) t = G_TYPE_STRING;
		if (gsf_xml_gvalue_from_str (res, t, xin->content->str)) {
			if (g_str_has_prefix (mi->name, "GSF_DOCPROP_VECTOR:")) {
				const gchar *true_name = strchr (mi->name + 19, ':');
				if (true_name != NULL && *(++true_name) != 0) {
					GsfDocProp *prop = gsf_doc_meta_data_lookup
						(mi->md, true_name);
					if (prop == NULL) {
						GsfDocPropVector *vector = gsf_docprop_vector_new ();
						GValue *val_vector = g_new0 (GValue, 1);
						g_value_init (val_vector, GSF_DOCPROP_VECTOR_TYPE);
						gsf_docprop_vector_append (vector, res);
						g_value_set_object (val_vector, vector);
						gsf_doc_meta_data_insert (mi->md, g_strdup (true_name),
									  val_vector);
						g_value_unset (res);
						g_free (res);
						g_object_unref (vector);
					} else {
						GValue const *old = gsf_doc_prop_get_val (prop);
						if (G_VALUE_HOLDS (old, GSF_DOCPROP_VECTOR_TYPE)) {
							GValue *new = g_new0 (GValue, 1);
							GValue r = G_VALUE_INIT;
							GArray *vector;
							g_value_init (new, GSF_DOCPROP_VECTOR_TYPE);
							g_value_copy (old, new);
							vector = gsf_value_get_docprop_array (new);
							g_value_init (&r, G_VALUE_TYPE (res));
							g_value_copy (res, &r);
							g_array_append_vals (vector, &r, 1);
							gsf_doc_prop_set_val (prop, new);
						} else
							g_printerr (_("Property \"%s\" used for "
								      "multiple types!"), true_name);
						g_value_unset (res);
						g_free (res);
					}

					g_free (mi->name);
					mi->name = NULL;
					return;
				}
			}
			gsf_doc_meta_data_insert (mi->md, mi->name, res);
		} else {
			g_free (res);
			g_free (mi->name);
		}
		mi->name = NULL;
	}
}

#if 0
/* These need special handling for attributes */
template
auto_reload
hl_behavior
doc_stats
#endif

static GsfXMLInNode const gsf_opendoc_meta_st_dtd[] = {
  GSF_XML_IN_NODE (META, META, OO_NS_OFFICE, "meta", FALSE, NULL, NULL),
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
    GSF_XML_IN_NODE (META, META_USER_DEFINED, OO_NS_META, "user-defined", GSF_XML_CONTENT, &od_meta_user_defined,  &od_meta_user_defined_end),
   GSF_XML_IN_NODE_END
};


static void
gsf_opendoc_metadata_subtree_free (G_GNUC_UNUSED GsfXMLIn *xin, gpointer old_state)
{
	GsfOOMetaIn *state = old_state;

	if (state->keywords) {
		GValue *val = g_new0 (GValue, 1);
		g_value_init (val, GSF_DOCPROP_VECTOR_TYPE);
		g_value_set_object (val, state->keywords);
		gsf_doc_meta_data_insert (state->md,
					  g_strdup (GSF_META_NAME_KEYWORDS), val);
		g_object_unref (state->keywords);
	}

	g_object_unref (state->md);

	if (state->doc)
		gsf_xml_in_doc_free (state->doc);

	g_free (state);
}

/**
 * gsf_doc_meta_data_odf_subtree:
 * @md: #GsfDocMetaData
 * @doc: #GsfXMLInDoc
 *
 * Extend @xin so that it can parse a subtree in OpenDoc metadata format
 *
 * Since: 1.14.24
 **/
void
gsf_doc_meta_data_odf_subtree (GsfDocMetaData *md, GsfXMLIn *doc)
{
	GsfOOMetaIn *state = NULL;

	g_return_if_fail (md != NULL);

	state = g_new0 (GsfOOMetaIn, 1);
	state->md = g_object_ref (md);
	state->typ = G_TYPE_NONE;
	state->doc = gsf_xml_in_doc_new (gsf_opendoc_meta_st_dtd, gsf_ooo_ns);
	gsf_xml_in_push_state (doc, state->doc, state,
			       gsf_opendoc_metadata_subtree_free, NULL);
}

/**
 * gsf_opendoc_metadata_subtree: (skip)
 * @doc: #GsfXMLInDoc
 * @md: #GsfDocMetaData
 *
 * Deprecated: 1.14.24, use gsf_doc_meta_data_odf_subtree
 *
 * Extend @xin so that it can parse a subtree in OpenDoc metadata format
 **/
void
gsf_opendoc_metadata_subtree (GsfXMLIn *xin, GsfDocMetaData *md)
{
	gsf_doc_meta_data_odf_subtree (md, xin);
}

/**
 * gsf_opendoc_metadata_subtree_internal:
 * @doc: #GsfXMLInDoc
 *
 * Extend @xin so that it can parse a subtree in OpenDoc metadata format
 * The current user_state must be a  GsfOOMetaIn!
 **/
static void
gsf_opendoc_metadata_subtree_internal (GsfXMLIn *xin, G_GNUC_UNUSED xmlChar const **attrs)
{
	GsfOOMetaIn *mi = (GsfOOMetaIn *)xin->user_state;

	if (!mi->doc)
		mi->doc = gsf_xml_in_doc_new (gsf_opendoc_meta_st_dtd, gsf_ooo_ns);

	gsf_xml_in_push_state (xin, mi->doc, NULL, NULL, NULL);
}

static GsfXMLInNode const gsf_opendoc_meta_dtd[] = {
  GSF_XML_IN_NODE_FULL (START, START, -1, NULL, FALSE, FALSE, TRUE, NULL, NULL, 0),
  GSF_XML_IN_NODE_FULL (START, META, OO_NS_OFFICE, "meta", FALSE, FALSE, TRUE, &gsf_opendoc_metadata_subtree_internal, NULL, 0),
   GSF_XML_IN_NODE_END
};

/**
 * gsf_doc_meta_data_read_from_odf:
 * @md: #GsfDocMetaData
 * @input: #GsfInput
 *
 * Read an OpenDocument metadata stream from @input and store the properties
 * into @md.  Overwrite any existing properties with the same id.
 *
 * Since: 1.14.24
 *
 * Returns: (transfer full): a #GError if there is a problem.
 **/
GError*
gsf_doc_meta_data_read_from_odf (GsfDocMetaData *md, GsfInput *input)
{
	GsfXMLInDoc	*doc;
	GsfOOMetaIn	 state;

	state.md  = md;
	state.keywords = NULL;
	state.err = NULL;
	state.name = NULL;
	state.doc = NULL;

	doc = gsf_xml_in_doc_new (gsf_opendoc_meta_dtd, gsf_ooo_ns);
	gsf_xml_in_doc_parse (doc, input, &state);
	gsf_xml_in_doc_free (doc);

	if (state.doc)
		gsf_xml_in_doc_free (state.doc);

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

/**
 * gsf_opendoc_metadata_read: (skip)
 * @input: #GsfInput
 * @md: #GsfDocMetaData
 *
 * Read an OpenDocument metadata stream from @input and store the properties
 * into @md.  Overwrite any existing properties with the same id.
 *
 * Deprecated: 1.14.24, use gsf_doc_meta_data_read_from_odf
 *
 * Returns: (transfer full): a #GError if there is a problem.
 **/
GError *
gsf_opendoc_metadata_read (GsfInput *input, GsfDocMetaData *md)
{
	return gsf_doc_meta_data_read_from_odf (md, input);
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

/* ODF does not like "t" and "f" which we use normally */
static void
gsf_xml_out_add_gvalue_for_odf (GsfXMLOut *xout,
				char const *id, GValue const *val)
{
	if (G_VALUE_TYPE (val) == G_TYPE_BOOLEAN)
		gsf_xml_out_add_cstr
			(xout, id,
			 g_value_get_boolean (val) ? "true" : "false");
	else
		gsf_xml_out_add_gvalue (xout, id, val);
}


static void
meta_write_props_user_defined (char const *prop_name, GValue const *val, GsfXMLOut *output)
{
	GType t;
	char const *type_name = NULL;

	gsf_xml_out_start_element (output, "meta:user-defined");
	gsf_xml_out_add_cstr (output, "meta:name", prop_name);

	if (NULL == val) {
			gsf_xml_out_end_element (output);
			return;
	}

	t = G_VALUE_TYPE (val);
	switch (t) {
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
			type_name = "date";
	}
	if (NULL != type_name)
		gsf_xml_out_add_cstr (output, "meta:value-type", type_name);
	if (NULL != val)
		gsf_xml_out_add_gvalue_for_odf (output, NULL, val);
	gsf_xml_out_end_element (output);
}


static void
meta_write_props (char const *prop_name, GsfDocProp *prop, GsfXMLOut *output)
{
	char const *mapped_name;
	GValue const *val = gsf_doc_prop_get_val (prop);

	/* Handle specially */
	if (0 == strcmp (prop_name, GSF_META_NAME_KEYWORDS)) {
		GArray *va;
		unsigned i;
		char *str;

		/* OLE2 stores a single string, with no obvious
		 * standard for seperator */
		if (G_TYPE_STRING == G_VALUE_TYPE (val)) {
			str = g_value_dup_string (val);
			if (str && *str) {
				gsf_xml_out_start_element (output, "meta:keyword");
				gsf_xml_out_add_cstr (output, NULL, str);
				gsf_xml_out_end_element (output);
			}
			g_free (str);
		} else if (NULL != (va = gsf_value_get_docprop_array (val))) {
			for (i = 0 ; i < va->len; i++) {
				str = g_value_dup_string (&g_array_index (va, GValue, i));
				gsf_xml_out_start_element (output, "meta:keyword");
				gsf_xml_out_add_cstr (output, NULL, str);
				gsf_xml_out_end_element (output);
				g_free (str);
			}
		}
		return;
	}

	if (NULL == (mapped_name = od_map_prop_name (prop_name))) {
		if (GSF_DOCPROP_VECTOR_TYPE == G_VALUE_TYPE (val)) {
			GArray		 *vector = gsf_value_get_docprop_array (val);
			guint		  i;
			guint		  num_values = vector->len;

			for (i = 0; i < num_values; i++) {
				GValue	*v;
				char    *new_name = g_strdup_printf
					("GSF_DOCPROP_VECTOR:%.4i:%s", i, prop_name);

				v = &g_array_index (vector, GValue, i);
				meta_write_props_user_defined (new_name, v, output);
				g_free (new_name);
			}
		} else
			meta_write_props_user_defined (prop_name, val, output);
		return;
	}

	/* Standardized  ODF meta items*/
	gsf_xml_out_start_element (output, mapped_name);
	if (NULL != val)
		gsf_xml_out_add_gvalue_for_odf (output, NULL, val);
	gsf_xml_out_end_element (output);
}

/**
 * gsf_doc_meta_data_write_to_odf:
 * @md: #GsfDocMetaData
 * @output: (type GsfXMLOut): a pointer to a #GsfOutput.
 *
 * Since: 1.14.24
 *
 * Returns: %TRUE if no error occured.
 **/
gboolean
gsf_doc_meta_data_write_to_odf (GsfDocMetaData const *md, gpointer output)
{
	char *ver_str;
	GsfXMLOut *xout;
	GsfODFOut *oout;

	if (output == NULL)
		return FALSE;

	/* For compatibility we take a GsfXMLOut argument.  It really
	   ought to be a GsfODFOut.  */
	xout = GSF_XML_OUT (output);
	oout = GSF_IS_ODF_OUT (xout) ? GSF_ODF_OUT (xout) : NULL;

	ver_str = oout
		? gsf_odf_out_get_version_string (oout)
		: g_strdup (gsf_odf_get_version_string ());

	gsf_xml_out_start_element (xout, OFFICE "document-meta");
	gsf_xml_out_add_cstr_unchecked (xout, "xmlns:office",
		"urn:oasis:names:tc:opendocument:xmlns:office:1.0");
	gsf_xml_out_add_cstr_unchecked (xout, "xmlns:xlink",
		"http://www.w3.org/1999/xlink");
	gsf_xml_out_add_cstr_unchecked (xout, "xmlns:dc",
		"http://purl.org/dc/elements/1.1/");
	gsf_xml_out_add_cstr_unchecked (xout, "xmlns:meta",
		"urn:oasis:names:tc:opendocument:xmlns:meta:1.0");
	gsf_xml_out_add_cstr_unchecked (xout, "xmlns:ooo",
		"http://openoffice.org/2004/office");
	gsf_xml_out_add_cstr_unchecked (xout, "office:version", ver_str);
	gsf_xml_out_start_element (xout, OFFICE "meta");
	gsf_doc_meta_data_foreach (md, (GHFunc) meta_write_props, xout);
	gsf_xml_out_end_element (xout); /* </office:meta> */

	gsf_xml_out_end_element (xout); /* </office:document-meta> */

	g_free (ver_str);

	return TRUE;
}

/**
 * gsf_opendoc_metadata_write: (skip)
 * @output: a pointer to a #GsfOutput.
 * @md: #GsfDocMetaData
 *
 * Deprecated: 1.14.24, use gsf_doc_meta_data_write_to_odf
 *
 * Returns: %TRUE if no error occured.
 **/
gboolean
gsf_opendoc_metadata_write (gpointer output, GsfDocMetaData const *md)
{
	return gsf_doc_meta_data_write_to_odf (md, output);
}

/****************************************************************************/

typedef struct _GsfODFOutPrivate {
	int odf_version;
} GsfODFOutPrivate;

enum {
	PROP_0,
	PROP_ODF_VERSION
};

G_DEFINE_TYPE_WITH_CODE (GsfODFOut, gsf_odf_out, GSF_XML_OUT_TYPE,
			 G_ADD_PRIVATE (GsfODFOut))

static GObjectClass *parent_class;

static void
gsf_odf_out_set_property (GObject      *object,
			  guint         property_id,
			  GValue const *value,
			  GParamSpec   *pspec)
{
	GsfODFOut *xout = (GsfODFOut *)object;
	GsfODFOutPrivate *priv = xout->priv;

	switch (property_id) {
	case PROP_ODF_VERSION:
		priv->odf_version = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gsf_odf_out_get_property (GObject     *object,
			  guint        property_id,
			  GValue      *value,
			  GParamSpec  *pspec)
{
	GsfODFOut const *xout = (GsfODFOut const *)object;
	GsfODFOutPrivate const *priv = xout->priv;

	switch (property_id) {
	case PROP_ODF_VERSION:
		g_value_set_int (value, priv->odf_version);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gsf_odf_out_init (GsfODFOut *xout)
{
	GsfODFOutPrivate *priv = gsf_odf_out_get_instance_private (xout);

	xout->priv = priv;
	priv->odf_version = 100;
}

static void
gsf_odf_out_class_init (GsfODFOutClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (gobject_class);

	gobject_class->get_property = gsf_odf_out_get_property;
	gobject_class->set_property = gsf_odf_out_set_property;

	g_object_class_install_property
		(gobject_class, PROP_ODF_VERSION,
		 g_param_spec_int ("odf-version",
				   _("ODF version"),
				   _("The ODF version this object is targeting as an integer like 100"),
				   0,
				   G_MAXINT,
				   100,
				   G_PARAM_STATIC_STRINGS |
				   G_PARAM_READWRITE |
				   G_PARAM_CONSTRUCT_ONLY));
}

int
gsf_odf_out_get_version (GsfODFOut *oout)
{
	g_return_val_if_fail (GSF_IS_ODF_OUT (oout), 100);
	return oout->priv->odf_version;
}

char *
gsf_odf_out_get_version_string (GsfODFOut *oout)
{
	int ver = gsf_odf_out_get_version (oout);
	return g_strdup_printf ("%d.%d", ver / 100, ver % 100);
}
