/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-libxml.c :
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

#include <gsf-config.h>
#include <gsf/gsf-libxml.h>
#include <gsf/gsf-input.h>
#include <gsf/gsf-output.h>
#include <gsf/gsf-input-gzip.h>

/* Note: libxml erroneously declares the length argument as int.  */
static int
gsf_libxml_read (void *context, char *buffer, int len)
{
	gsf_off_t remaining = gsf_input_remaining ((GsfInput *)context);
	guint8* res;

	if (len > remaining)
		len = remaining;
	res = (guint8 *) gsf_input_read ((GsfInput *)context,
					 (size_t)len, buffer);
	if (res == NULL && len > 0) /* Not an error if len == 0 */
		return -1;
	return len;
}

static int
gsf_libxml_write (void *context, char const *buffer, int len)
{
	if (!gsf_output_write ((GsfOutput *)context, (size_t)len, buffer))
		return -1;
	return len;
}

static int
gsf_libxml_close (void *context)
{
	g_object_unref (G_OBJECT (context));
	return TRUE;
}

/**
 * gsf_xml_parser_context :
 * @input :
 *
 * Create a libxml2 pull style parser context wrapper around a gsf input.
 * This signature will probably change to supply a SAX structure.
 *
 * NOTE : adds a reference to @input
 *
 * Returns : A parser context or NULL
 **/
xmlParserCtxtPtr
gsf_xml_parser_context (GsfInput *input)
{
	GsfInputGZip *gzip;

	g_return_val_if_fail (GSF_IS_INPUT (input), NULL);

	gzip = gsf_input_gzip_new (input, NULL);
	if (gzip != NULL)
		input = GSF_INPUT (gzip);
	else
		g_object_ref (G_OBJECT (input));

	return xmlCreateIOParserCtxt (
		NULL, NULL,
		(xmlInputReadCallback) gsf_libxml_read, 
		(xmlInputCloseCallback) gsf_libxml_close,
		input, XML_CHAR_ENCODING_NONE);
}

/**
 * gsf_xml_output_buffer_new :
 * @output :
 * @encoding : optionally NULL.
 *
 * NOTE : adds a reference to @output
 *
 */
static xmlOutputBufferPtr
gsf_xml_output_buffer_new (GsfOutput *output,
			   xmlCharEncodingHandlerPtr handler)
{
	xmlOutputBufferPtr res = xmlAllocOutputBuffer (handler);
	if (res != NULL) {
		g_object_ref (G_OBJECT (output));
		res->context = (void *)output;
		res->writecallback = gsf_libxml_write;
		res->closecallback = gsf_libxml_close;
	}

	return res;
}

int
gsf_xmlDocFormatDump (GsfOutput *output, xmlDocPtr cur, const char * encoding,
		      gboolean format)
{
	xmlOutputBufferPtr buf;
	xmlCharEncodingHandlerPtr handler = NULL;

	if (cur == NULL) {
#ifdef DEBUG_TREE
		xmlGenericError(xmlGenericErrorContext,
				"xmlDocDump : document == NULL\n");
#endif
		return(-1);
	}

	if (encoding != NULL) {
		xmlCharEncoding enc;

		enc = xmlParseCharEncoding(encoding);

		if (cur->charset != XML_CHAR_ENCODING_UTF8) {
			xmlGenericError(xmlGenericErrorContext,
					"xmlDocDump: document not in UTF8\n");
			return(-1);
		}
		if (enc != XML_CHAR_ENCODING_UTF8) {
			handler = xmlFindCharEncodingHandler(encoding);
			if (handler == NULL) {
				xmlFree((char *) cur->encoding);
				cur->encoding = NULL;
			}
		}
	}
	buf = gsf_xml_output_buffer_new (output, handler);
	return xmlSaveFormatFileTo (buf, cur, encoding, format);
}

/***************************************************************************/

static void
xml_sax_start_element (GsfXmlSAXState *state, xmlChar const *name, xmlChar const **attrs)
{
	GSList *ptr;
	GsfXmlSAXNode *node;

	for (ptr = state->node->first_child ; ptr != NULL ; ptr = ptr->next) {
		node = ptr->data;
		if (node->name != NULL && !strcmp (name, node->name)) {
			state->state_stack = g_slist_prepend (state->state_stack, state->node);
			state->node = node;
			if (node->start != NULL)
				node->start (state, attrs);
			return;
		}
	}

	if (state->unknown_depth++)
		return;
	g_warning ("Unexpected element '%s' in state %s.", name, state->node->name);
	{
		GSList *ptr = state->state_stack;
		for (;ptr != NULL && ptr->next != NULL; ptr = ptr->next)
			puts (((GsfXmlSAXNode *)(ptr->data))->name);
	}
}

static void
xml_sax_end_element (GsfXmlSAXState *state, const xmlChar *name)
{
	if (state->unknown_depth > 0) {
		state->unknown_depth--;
		return;
	}

	g_return_if_fail (state->state_stack != NULL);
	g_return_if_fail (!strcmp (name, state->node->name));

	if (state->node->end)
		state->node->end (state);
	if (state->node->has_content)
		g_string_truncate (state->content, 0);

	/* pop the state stack */
	state->node = state->state_stack->data;
	state->state_stack = g_slist_remove (state->state_stack, state->node);
}

static void
xml_sax_characters (GsfXmlSAXState *state, const xmlChar *chars, int len)
{
	if (state->node->has_content)
		g_string_append_len (state->content, chars, len);
}

static xmlEntityPtr
xml_sax_get_entity (GsfXmlSAXState *state, const xmlChar *name)
{
	(void)state;
	return xmlGetPredefinedEntity (name);
}

static void
xml_sax_start_document (GsfXmlSAXState *state)
{
	state->node = state->root;
	state->unknown_depth = 0;
	state->state_stack = NULL;
}

static void
xml_sax_end_document (GsfXmlSAXState *state)
{
	g_string_free (state->content, TRUE);
	state->content = NULL;

	g_return_if_fail (state->node == state->root);
	g_return_if_fail (state->unknown_depth == 0);
}

static void
xml_sax_warning (GsfXmlSAXState *state, const char *msg, ...)
{
	va_list args;

	(void)state;
	va_start (args, msg);
	g_logv ("XML", G_LOG_LEVEL_WARNING, msg, args);
	va_end (args);
}

static void
xml_sax_error (GsfXmlSAXState *state, const char *msg, ...)
{
	va_list args;

	(void)state;
	va_start (args, msg);
	g_logv ("XML", G_LOG_LEVEL_CRITICAL, msg, args);
	va_end (args);
}

static void
xml_sax_fatal_error (GsfXmlSAXState *state, const char *msg, ...)
{
	va_list args;

	(void)state;
	va_start (args, msg);
	g_logv ("XML", G_LOG_LEVEL_ERROR, msg, args);
	va_end (args);
}

static xmlSAXHandler xmlSaxSAXParser = {
	0, /* internalSubset */
	0, /* isStandalone */
	0, /* hasInternalSubset */
	0, /* hasExternalSubset */
	0, /* resolveEntity */
	(getEntitySAXFunc)xml_sax_get_entity, /* getEntity */
	0, /* entityDecl */
	0, /* notationDecl */
	0, /* attributeDecl */
	0, /* elementDecl */
	0, /* unparsedEntityDecl */
	0, /* setDocumentLocator */
	(startDocumentSAXFunc)xml_sax_start_document, /* startDocument */
	(endDocumentSAXFunc)xml_sax_end_document, /* endDocument */
	(startElementSAXFunc)xml_sax_start_element, /* startElement */
	(endElementSAXFunc)xml_sax_end_element, /* endElement */
	0, /* reference */
	(charactersSAXFunc)xml_sax_characters, /* characters */
	0, /* ignorableWhitespace */
	0, /* processingInstruction */
	0, /* comment */
	(warningSAXFunc)xml_sax_warning, /* warning */
	(errorSAXFunc)xml_sax_error, /* error */
	(fatalErrorSAXFunc)xml_sax_fatal_error, /* fatalError */
	0, /* getParameterEntity */
	0, /* cdataBlock */
	0, /* externalSubset */
	0
};

/**
 * gsf_xmlSAX_prep_dtd :
 * @node :
 *
 * link  up the static parent descriptors
 *
 * Returns : TRUE on success
 **/
gboolean
gsf_xmlSAX_prep_dtd (GsfXmlSAXNode *node)
{
	GHashTable *symbols;
	GsfXmlSAXNode *tmp;
	GsfXmlSAXNode *real_node;

	if (node->parent_initialized)
		return TRUE;

	symbols = g_hash_table_new (g_str_hash, g_str_equal);
	for (; node->id != NULL ; node++ ) {
		g_return_val_if_fail (!node->parent_initialized, FALSE);

		tmp = g_hash_table_lookup (symbols, node->id);
		if (tmp != NULL) {
			/* if its empty then this is just a recusion */
			if (node->start != NULL || node->end != NULL ||
			    node->has_content != FALSE || node->user_data.v_int != 0) {
				g_warning ("ID '%s' has already been registered", node->id);
				return FALSE;
			}
			real_node = tmp;
		} else {
			/* be anal, the macro probably initialized this, but do it just in case */
			node->first_child = NULL;
			g_hash_table_insert (symbols, (gpointer)node->id, node);
			real_node = node;
		}

		tmp = g_hash_table_lookup (symbols, node->parent_id);
		if (tmp == NULL) {
			if (strcmp (node->id, node->parent_id)) {
				g_warning ("Parent ID '%s' unknown", node->parent_id);
				return FALSE;
			}
		} else
			tmp->first_child = g_slist_prepend (tmp->first_child, real_node);
		node->parent_initialized = TRUE;
	}

	g_hash_table_destroy (symbols);

	return TRUE;
}

gboolean
gsf_xmlSAX_parse (GsfInput *input, GsfXmlSAXState *doc)
{
	xmlParserCtxt *ctxt;
	gboolean res;
	gboolean valid_dtd = gsf_xmlSAX_prep_dtd (doc->root);

	g_return_val_if_fail (valid_dtd, FALSE);
	g_return_val_if_fail (GSF_IS_INPUT (input), FALSE);

	ctxt = gsf_xml_parser_context (input);

	g_return_val_if_fail (ctxt != NULL, FALSE);

	ctxt->userData = doc;
	doc->content = g_string_sized_new (128);
	ctxt->sax = &xmlSaxSAXParser;
	xmlParseDocument (ctxt);
	ctxt->sax = NULL;
	res = ctxt->wellFormed;
	xmlFreeParserCtxt (ctxt);

	return res;
}
