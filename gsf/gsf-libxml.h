/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-libxml.h: Utility wrappers for using gsf with libxml
 *
 * Copyright (C) 2002 Jody Goldberg (jody@gnome.org)
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

#ifndef GSF_LIBXML_H
#define GSF_LIBXML_H

#include <gsf/gsf.h>
#include <glib-object.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

/****************************************************************************/
/* GSF wrappers for libxml2 */
xmlParserCtxt *gsf_xml_parser_context (GsfInput   *input);
int	       gsf_xmlDocFormatDump   (GsfOutput  *output,
				       xmlDoc	  *cur,
				       char const *encoding,
				       gboolean    format);

/****************************************************************************/
/* Simplified GSF based xml import (based on libxml2 SAX) */
typedef struct _GsfInputXML	GsfInputXML;
typedef struct _GsfInputXMLNode	GsfInputXMLNode;

struct _GsfInputXML {
    /* private */
	GsfInputXMLNode	*node;
	GSList	 	*state_stack;
	GString		*content;
	gint		 unknown_depth;	/* handle recursive unknown tags */

    /* public */
	GsfInputXMLNode	*root;
};

struct _GsfInputXMLNode {
	char const *id;
	char const *name;
	char const *parent_id;
	gboolean parent_initialized;
	GSList *first_child;

	gboolean	has_content;

	void (*start) (GsfInputXML *state, xmlChar const **attrs);
	void (*end)   (GsfInputXML *state);

	union {
		int	    v_int;
		gboolean    v_bool;
		gpointer    v_blob;
		char const *v_str;
	} user_data;
};

#define GSF_XML_SAX_NODE(parent_id, id, name, has_content, start, end, user)	\
{ #id, name, #parent_id, FALSE, NULL, has_content, start, end, { user } }

gboolean gsf_input_xml_prep_dtd (GsfInputXMLNode *node);
gboolean gsf_input_xml_parse (GsfInput *input, GsfInputXML *doc);

/****************************************************************************/
/* Simplified GSF based xml export (does not use libxml) */

typedef struct _GsfOutputXML	GsfOutputXML;

#define GSF_OUTPUT_XML_TYPE        (gsf_output_xml_get_type ())
#define GSF_OUTPUT_XML(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_OUTPUT_XML_TYPE, GsfOutputXML))
#define GSF_IS_OUTPUT_XML(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_OUTPUT_XML_TYPE))

GType gsf_output_xml_get_type (void);
GsfOutputXML *gsf_output_xml_new (GsfOutput *output);

void	    gsf_output_xml_set_namespace  (GsfOutputXML *xml, char const *ns);
void	    gsf_output_xml_start_element  (GsfOutputXML *xml, char const *id);
char const *gsf_output_xml_end_element	  (GsfOutputXML *xml);
void	    gsf_output_xml_simple_element (GsfOutputXML *xml, char const *id,
					   char const *content);

void gsf_output_xml_add_attr_cstr	(GsfOutputXML *xml, char const *id,
					 char const *val_utf8);
void gsf_output_xml_add_attr_cstr_safe	(GsfOutputXML *xml, char const *id,
					 char const *val_utf8);
void gsf_output_xml_add_attr_bool	(GsfOutputXML *xml, char const *id,
					 gboolean val);
void gsf_output_xml_add_attr_int	(GsfOutputXML *xml, char const *id,
					 int val);
void gsf_output_xml_add_attr_uint	(GsfOutputXML *xml, char const *id,
					 unsigned val);
void gsf_output_xml_add_attr_float	(GsfOutputXML *xml, char const *id,
					 double val, int precision);
void gsf_output_xml_add_attr_color	(GsfOutputXML *xml, char const *id,
					 unsigned r, unsigned g, unsigned b);
void gsf_output_xml_add_attr_base64	(GsfOutputXML *xml, char const *id,
					 guint8 const *data, unsigned len);

/* TODO : something for enums ? */

G_END_DECLS

#endif /* GSF_LIBXML_H */
