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
#include <libxml/tree.h>

G_BEGIN_DECLS

xmlParserCtxtPtr	gsf_xml_parser_context	(GsfInput  *input);
int			gsf_xmlDocFormatDump	(GsfOutput *output,
						 xmlDocPtr cur, gboolean format);

/* SAX Utils */
typedef struct _GsfXmlSAXState	GsfXmlSAXState;
typedef struct _GsfXmlSAXNode	GsfXmlSAXNode;

struct _GsfXmlSAXState {
/* private */
	GsfXmlSAXNode	*node;
	GSList	 	*state_stack;
	GString		*content;
	gint		 unknown_depth;	/* handle recursive unknown tags */

/* public */
	GsfXmlSAXNode	*root;
};

struct _GsfXmlSAXNode {
	char const *id;
	char const *name;
	char const *parent_id;
	gboolean parent_initialized;
	GSList *first_child;

	gboolean	has_content;

	void (*start) (GsfXmlSAXState *state, xmlChar const **attrs);
	void (*end)   (GsfXmlSAXState *state);

	union {
		int	    v_int;
		gboolean    v_bool;
		gpointer    v_blob;
		char const *v_str;
	} user_data;
};

#define GSF_XML_SAX_NODE(parent_id, id, name, has_content, start, end, user)	\
{ #id, name, #parent_id, FALSE, NULL, has_content, start, end, { user } }

gboolean gsf_xmlSAX_prep_dtd (GsfXmlSAXNode *node);
gboolean gsf_xmlSAX_parse    (GsfInput *input, GsfXmlSAXState *doc);

G_END_DECLS

#endif /* GSF_LIBXML_H */
