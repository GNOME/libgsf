/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-libxml.h: Utility wrappers for using gsf with libxml
 *
 * Copyright (C) 2002-2005 Jody Goldberg (jody@gnome.org)
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

#ifndef GSF_LIBXML_H
#define GSF_LIBXML_H

#include <gsf/gsf.h>
#include <glib-object.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

/****************************************************************************/
/* GSF wrappers for libxml2 */
xmlParserCtxt *gsf_xml_parser_context (GsfInput   *input);
#if 0
				       /* this is cleaner, tack it on for 2.0 */
					xmlSAXHandlerPtr sax, gpointer user);
#endif
int	       gsf_xmlDocFormatDump   (GsfOutput  *output,
				       xmlDoc	  *cur,
				       char const *encoding,
				       gboolean    format);

/****************************************************************************/
/* Simplified GSF based xml import (based on libxml2 SAX) */
typedef struct _GsfXMLBlob	GsfXMLBlob;

typedef struct _GsfXMLIn		GsfXMLIn;
typedef struct _GsfXMLInDoc		GsfXMLInDoc;
typedef struct _GsfXMLInDocExtension	GsfXMLInDocExtension;
typedef struct _GsfXMLInNode		GsfXMLInNode;
typedef struct _GsfXMLInNS		GsfXMLInNS;

typedef enum {
	GSF_XML_NO_CONTENT,
	GSF_XML_CONTENT,
	GSF_XML_SHARED_CONTENT
} GsfXMLContent;

typedef gboolean (*GsfXMLInUnknownFunc) (GsfXMLIn *state, xmlChar const *elem, xmlChar const **attrs);
struct _GsfXMLIn {
	GsfXMLInDoc  const *doc;	/* init before parsing */

    /* look but do not change */
	GsfXMLInNode const *node;	/* current node */
	GSList	 	   *state_stack;

	GsfXMLInNS   const *default_ns;	/* optionally NULL */
	GSList	 	   *ns_stack;

	GString		*content;
	gint		 unknown_depth;	/* handle recursive unknown tags */
	GHashTable	*ns_prefixes;	/* current ns prefixes */
	GPtrArray	*ns_by_id;		/* indexed by id */

	gpointer	 user_state;
};

struct _GsfXMLInNode {
	char const *id;
	int	    ns_id;
	char const *name;
	char const *parent_id;
	void (*start) (GsfXMLIn *state, xmlChar const **attrs);
	void (*end)   (GsfXMLIn *state, GsfXMLBlob *unknown);

	union {
		int	    v_int;
		gboolean    v_bool;
		gpointer    v_blob;
		char const *v_str;
	} user_data;
	GsfXMLContent has_content;

	unsigned int check_children_for_ns : 1;
	unsigned int share_children_with_parent : 1;

	/* internal state */
	unsigned int parent_initialized : 1;
	GSList *groups;
};

struct _GsfXMLInNS {
	char const *uri;
	unsigned    ns_id;
};

struct _GsfXMLInDocExtension {
};

#define GSF_XML_IN_NS(id, uri) \
{ uri, id}

#define GSF_XML_IN_NODE_FULL(parent_id, id, ns, name, has_content, 	\
			     share_children_with_parent, check_ns, start, end, user)	\
{									\
	#id, ns, name, #parent_id, start, end, { user }, has_content,	\
	check_ns, share_children_with_parent,				\
	FALSE, NULL							\
}

#define GSF_XML_IN_NODE(parent_id, id, ns, name, has_content, start, end) \
	GSF_XML_IN_NODE_FULL(parent_id, id, ns, name, has_content,	  \
			     FALSE, FALSE, start, end, 0)

GsfXMLInDoc *gsf_xml_in_doc_new	   (GsfXMLInNode *root, GsfXMLInNS *ns);
void	     gsf_xml_in_doc_free   (GsfXMLInDoc *doc);
gboolean     gsf_xml_in_doc_parse  (GsfXMLInDoc *doc, GsfInput *input,
				    gpointer user_state);
void	     gsf_xml_in_doc_extend (GsfXMLInDoc *doc, GsfXMLInNode *nodes);
void	     gsf_xml_in_doc_set_unknown_handler (GsfXMLInDoc *doc,
				    GsfXMLInUnknownFunc handler);

char const  *gsf_xml_in_check_ns   (GsfXMLIn const *state, char const *str,
				    unsigned int ns_id);
gboolean     gsf_xml_in_namecmp	   (GsfXMLIn const *state, char const *str,
				    unsigned int ns_id, char const *name);

/****************************************************************************/
/* Simplified GSF based xml export (does not use libxml) */

typedef struct _GsfXMLOut	GsfXMLOut;

#define GSF_XML_OUT_TYPE	(gsf_xml_out_get_type ())
#define GSF_XML_OUT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_XML_OUT_TYPE, GsfXMLOut))
#define GSF_IS_XML_OUT(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_XML_OUT_TYPE))

GType gsf_xml_out_get_type (void);
GsfXMLOut *gsf_xml_out_new (GsfOutput *output);

void	    gsf_xml_out_set_doc_type	(GsfXMLOut *xml, char const *type);
void	    gsf_xml_out_start_element	(GsfXMLOut *xml, char const *id);
char const *gsf_xml_out_end_element	(GsfXMLOut *xml);

void gsf_xml_out_simple_element		(GsfXMLOut *xml, char const *id,
					 char const *content);
void gsf_xml_out_simple_int_element	(GsfXMLOut *xml, char const *id,
					 int val);
void gsf_xml_out_simple_float_element	(GsfXMLOut *xml, char const *id,
					 double val, int precision);

void gsf_xml_out_add_cstr_unchecked	(GsfXMLOut *xml, char const *id,
					 char const *val_utf8);
void gsf_xml_out_add_cstr		(GsfXMLOut *xml, char const *id,
					 char const *val_utf8);
void gsf_xml_out_add_bool		(GsfXMLOut *xml, char const *id,
					 gboolean val);
void gsf_xml_out_add_int		(GsfXMLOut *xml, char const *id,
					 int val);
void gsf_xml_out_add_uint		(GsfXMLOut *xml, char const *id,
					 unsigned int val);
void gsf_xml_out_add_float		(GsfXMLOut *xml, char const *id,
					 double val, int precision);
void gsf_xml_out_add_color		(GsfXMLOut *xml, char const *id,
					 unsigned int r, unsigned int g, unsigned int b);
void gsf_xml_out_add_base64		(GsfXMLOut *xml, char const *id,
					 guint8 const *data, unsigned int len);
void gsf_xml_out_add_enum               (GsfXMLOut *xml, char const *id,
					 GType etype, gint val);

/****************************************************************************/
/* Some general utilities */
gboolean gsf_xml_gvalue_from_str (GValue *res, GType t, char const *str);

G_END_DECLS

#endif /* GSF_LIBXML_H */
