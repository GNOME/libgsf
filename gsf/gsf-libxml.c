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
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-utils.h>

#include <math.h>

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
 * NOTE : This is _not_ releated to GsfOutputXML
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
gsf_input_xml_start_element (GsfInputXML *state, xmlChar const *name, xmlChar const **attrs)
{
	GSList *ptr;
	GsfInputXMLNode *node;

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
			puts (((GsfInputXMLNode *)(ptr->data))->name);
	}
}

static void
gsf_input_xml_end_element (GsfInputXML *state, const xmlChar *name)
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
gsf_input_xml_characters (GsfInputXML *state, const xmlChar *chars, int len)
{
	if (state->node->has_content)
		g_string_append_len (state->content, chars, len);
}

static xmlEntityPtr
gsf_input_xml_get_entity (GsfInputXML *state, const xmlChar *name)
{
	(void)state;
	return xmlGetPredefinedEntity (name);
}

static void
gsf_input_xml_start_document (GsfInputXML *state)
{
	state->node = state->root;
	state->unknown_depth = 0;
	state->state_stack = NULL;
}

static void
gsf_input_xml_end_document (GsfInputXML *state)
{
	g_string_free (state->content, TRUE);
	state->content = NULL;

	g_return_if_fail (state->node == state->root);
	g_return_if_fail (state->unknown_depth == 0);
}

static void
gsf_input_xml_warning (GsfInputXML *state, const char *msg, ...)
{
	va_list args;

	(void)state;
	va_start (args, msg);
	g_logv ("XML", G_LOG_LEVEL_WARNING, msg, args);
	va_end (args);
}

static void
gsf_input_xml_error (GsfInputXML *state, const char *msg, ...)
{
	va_list args;

	(void)state;
	va_start (args, msg);
	g_logv ("XML", G_LOG_LEVEL_CRITICAL, msg, args);
	va_end (args);
}

static void
gsf_input_xml_fatal_error (GsfInputXML *state, const char *msg, ...)
{
	va_list args;

	(void)state;
	va_start (args, msg);
	g_logv ("XML", G_LOG_LEVEL_ERROR, msg, args);
	va_end (args);
}

static xmlSAXHandler gsfInputXMLParser = {
	0, /* internalSubset */
	0, /* isStandalone */
	0, /* hasInternalSubset */
	0, /* hasExternalSubset */
	0, /* resolveEntity */
	(getEntitySAXFunc)gsf_input_xml_get_entity, /* getEntity */
	0, /* entityDecl */
	0, /* notationDecl */
	0, /* attributeDecl */
	0, /* elementDecl */
	0, /* unparsedEntityDecl */
	0, /* setDocumentLocator */
	(startDocumentSAXFunc)gsf_input_xml_start_document, /* startDocument */
	(endDocumentSAXFunc)gsf_input_xml_end_document, /* endDocument */
	(startElementSAXFunc)gsf_input_xml_start_element, /* startElement */
	(endElementSAXFunc)gsf_input_xml_end_element, /* endElement */
	0, /* reference */
	(charactersSAXFunc)gsf_input_xml_characters, /* characters */
	0, /* ignorableWhitespace */
	0, /* processingInstruction */
	0, /* comment */
	(warningSAXFunc)gsf_input_xml_warning, /* warning */
	(errorSAXFunc)gsf_input_xml_error, /* error */
	(fatalErrorSAXFunc)gsf_input_xml_fatal_error, /* fatalError */
	0, /* getParameterEntity */
	0, /* cdataBlock */
	0, /* externalSubset */
	0
};

/**
 * gsf_input_xml_prep_dtd :
 * @node :
 *
 * link  up the static parent descriptors
 *
 * Returns : TRUE on success
 **/
gboolean
gsf_input_xml_prep_dtd (GsfInputXMLNode *node)
{
	GHashTable *symbols;
	GsfInputXMLNode *tmp;
	GsfInputXMLNode *real_node;

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
gsf_input_xml_parse (GsfInput *input, GsfInputXML *doc)
{
	xmlParserCtxt *ctxt;
	gboolean res;
	gboolean valid_dtd = gsf_input_xml_prep_dtd (doc->root);

	g_return_val_if_fail (valid_dtd, FALSE);
	g_return_val_if_fail (GSF_IS_INPUT (input), FALSE);

	ctxt = gsf_xml_parser_context (input);

	g_return_val_if_fail (ctxt != NULL, FALSE);

	ctxt->userData = doc;
	doc->content = g_string_sized_new (128);
	ctxt->sax = &gsfInputXMLParser;
	xmlParseDocument (ctxt);
	ctxt->sax = NULL;
	res = ctxt->wellFormed;
	xmlFreeParserCtxt (ctxt);

	return res;
}

/****************************************************************************/

typedef enum {
	GSF_OUTPUT_XML_NOCONTENT,
	GSF_OUTPUT_XML_CHILD,
	GSF_OUTPUT_XML_CONTENT
} GsfOutputXMLState;

struct _GsfOutputXML {
	GObject	   base;

	GsfOutput	 *output;
	GSList		 *stack;
	char const 	 *name_space;
	GsfOutputXMLState state;
	unsigned   	  indent;
	gboolean	  needs_header;
};

typedef struct {
	GObjectClass  base;
} GsfOutputXMLClass;

static void
gsf_output_xml_finalize (GObject *obj)
{
	GObjectClass *parent_class;

	/* already unwrapped */

	parent_class = g_type_class_peek (G_TYPE_OBJECT);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static void
gsf_output_xml_init (GObject *obj)
{
	GsfOutputXML *xml = GSF_OUTPUT_XML (obj);
	xml->output = NULL;
	xml->name_space = NULL;
	xml->stack  = NULL;
	xml->state  = GSF_OUTPUT_XML_CHILD;
	xml->indent = 0;
	xml->needs_header = TRUE;
}

static void
gsf_output_xml_class_init (GObjectClass *gobject_class)
{
	gobject_class->finalize = gsf_output_xml_finalize;
}

GSF_CLASS (GsfOutputXML, gsf_output_xml,
	   gsf_output_xml_class_init, gsf_output_xml_init,
	   G_TYPE_OBJECT)

GsfOutputXML *
gsf_output_xml_new (GsfOutput *output)
{
	GsfOutputXML *xml = g_object_new (GSF_OUTPUT_XML_TYPE, NULL);
	if (!gsf_output_wrap (G_OBJECT (xml), output))
		return NULL;
	xml->output = output;
	return xml;
}

static inline void
gsf_output_xml_indent (GsfOutputXML *xml)
{
	static char const spaces [] =
		"                                        "
		"                                        "
		"                                        "
		"                                        "
		"                                        "
		"                                        ";
	unsigned i;
	for (i = xml->indent ; i > (sizeof (spaces)/2) ; i -= sizeof (spaces)/2)
		gsf_output_write (xml->output, sizeof (spaces) - 1, spaces);
	gsf_output_write (xml->output, i*2, spaces);
}

void
gsf_output_xml_set_namespace (GsfOutputXML *xml, char const *ns)
{
	g_return_if_fail (xml != NULL);
	xml->name_space = ns; 
}

/**
 * gsf_output_xml_start_elem :
 * @xml :
 * @id  :
 */
void
gsf_output_xml_start_element (GsfOutputXML *xml, char const *id)
{
	g_return_if_fail (id != NULL);
	g_return_if_fail (xml != NULL);
	g_return_if_fail (xml->state != GSF_OUTPUT_XML_CONTENT);

	if (xml->needs_header) {
		static char const header[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
		gsf_output_write (xml->output, sizeof (header) - 1, header);
		xml->needs_header = FALSE;
	}
	if (xml->state == GSF_OUTPUT_XML_NOCONTENT)
		gsf_output_write (xml->output, 2, ">\n");

	gsf_output_xml_indent (xml);
	if (xml->name_space != NULL)
		gsf_output_printf (xml->output, "<%s:%s", xml->name_space, id);
	else
		gsf_output_printf (xml->output, "<%s", id);

	xml->stack = g_slist_prepend (xml->stack, (gpointer)id);
	xml->indent++;
	xml->state = GSF_OUTPUT_XML_NOCONTENT;
}

/**
 * gsf_output_xml_end_elem :
 * @xml :
 */
char const *
gsf_output_xml_end_element (GsfOutputXML *xml)
{
	char const *id;

	g_return_val_if_fail (xml != NULL, NULL);
	g_return_val_if_fail (xml->stack != NULL, NULL);

	id = xml->stack->data;
	xml->stack = g_slist_remove (xml->stack, id);
	xml->indent--;
	switch (xml->state) {
	case GSF_OUTPUT_XML_NOCONTENT :
		gsf_output_write (xml->output, 3, "/>\n");
		break;

	case GSF_OUTPUT_XML_CHILD :
		gsf_output_xml_indent (xml);
		/* fall through */
	case GSF_OUTPUT_XML_CONTENT :
		if (xml->name_space != NULL)
			gsf_output_printf (xml->output, "</%s:%s>\n", xml->name_space, id);
		else
			gsf_output_printf (xml->output, "</%s>\n", id);
	};
	xml->state = GSF_OUTPUT_XML_CHILD;
	return id;
}

/**
 * gsf_output_xml_simple_element :
 * @xml :
 * @id  :
 * @content :
 *
 * A convenience routine
 **/
void
gsf_output_xml_simple_element (GsfOutputXML *xml, char const *id,
			       char const *content)
{
	gsf_output_xml_start_element (xml, id);
	gsf_output_xml_add_attr_cstr (xml, NULL, content);
	gsf_output_xml_end_element (xml);
}

/**
 * gsf_output_xml_add_attr_cstr :
 * @xml :
 * @id : optionally NULL for content
 */
void
gsf_output_xml_add_attr_cstr (GsfOutputXML *xml, char const *id,
			      char const *val_utf8)
{
	g_return_if_fail (xml != NULL);
	g_return_if_fail (xml->state == GSF_OUTPUT_XML_NOCONTENT);

	if (id == NULL) {
		xml->state = GSF_OUTPUT_XML_CONTENT;
		gsf_output_write (xml->output, 1, ">");
		gsf_output_write (xml->output, strlen (val_utf8), val_utf8);
	} else
		gsf_output_printf (xml->output, " %s=\"%s\"", id, val_utf8);
}

/**
 * gsf_output_xml_add_attr_cstr_safe :
 * @xml :
 * @id : optionally NULL for content
 * @val_utf8 : a utf8 encoded string
 *
 * dump @val_utf8 to an attribute named @id or as the nodes content escaping
 * characters as necessary.
 */
void
gsf_output_xml_add_attr_cstr_safe (GsfOutputXML *xml, char const *id,
				   char const *val_utf8)
{
	guint8 const *cur   = val_utf8;
	guint8 const *start = val_utf8;

	if (id == NULL) {
		xml->state = GSF_OUTPUT_XML_CONTENT;
		gsf_output_write (xml->output, 1, ">");
	} else
		gsf_output_printf (xml->output, " %s=\"", id);
	while (*cur != '\0') {
		if (*cur == '<') {
			if (cur != start)
				gsf_output_write (xml->output, cur-start, start);
			start = ++cur;
			gsf_output_write (xml->output, 4, "&lt;");
		} else if (*cur == '>') {
			if (cur != start)
				gsf_output_write (xml->output, cur-start, start);
			start = ++cur;
			gsf_output_write (xml->output, 4, "&gt;");
		} else if (*cur == '&') {
			if (cur != start)
				gsf_output_write (xml->output, cur-start, start);
			start = ++cur;
			gsf_output_write (xml->output, 5, "&amp;");
		} else if (*cur == '"') {
			if (cur != start)
				gsf_output_write (xml->output, cur-start, start);
			start = ++cur;
			gsf_output_write (xml->output, 6, "&quot;");
		} else if (((*cur >= 0x20) && (*cur < 0x80)) ||
			   (*cur == '\n') || (*cur == '\r') || (*cur == '\t')) {
			cur++;
		} else if (*cur >= 0x80) {
			gsf_output_printf (xml->output, "&#%d;",
					   g_utf8_get_char (cur));
			cur = g_utf8_next_char (cur);
		} else {
			g_warning ("Unknown char 0x%hhx in string", *cur);
			cur++;
		}
	}
	if (cur != start)
		gsf_output_write (xml->output, cur-start, start);
	if (id != NULL)
		gsf_output_write (xml->output, 1, "\"");
}

/**
 * gsf_output_xml_add_attr_int :
 * @xml :
 * @id  : optionally NULL for content
 */
void
gsf_output_xml_add_attr_bool (GsfOutputXML *xml, char const *id,
			      gboolean val)
{
	gsf_output_xml_add_attr_cstr (xml, id,
		val ? "1" : "0");
}

/**
 * gsf_output_xml_add_attr_int :
 * @xml :
 * @id  : optionally NULL for content
 */
void
gsf_output_xml_add_attr_int (GsfOutputXML *xml, char const *id,
			     int val)
{
	char buf [4 * sizeof (int)];
	sprintf (buf, "%d", val);
	gsf_output_xml_add_attr_cstr (xml, id, buf);
}

/**
 * gsf_output_xml_add_attr_uint :
 * @xml :
 * @id  : optionally NULL for content
 */
void
gsf_output_xml_add_attr_uint (GsfOutputXML *xml, char const *id,
			      unsigned val)
{
	char buf [4 * sizeof (int)];
	sprintf (buf, "%u", val);
	gsf_output_xml_add_attr_cstr (xml, id, buf);
}

/**
 * gsf_output_xml_add_attr_float :
 * @xml :
 * @id  : optionally NULL for content
 */
void
gsf_output_xml_add_attr_float (GsfOutputXML *xml, char const *id,
			       double val, int precision)
{
	char buf [101 + DBL_DIG];

	if (precision < 0 || precision > DBL_DIG)
		precision = DBL_DIG;

	if (fabs (val) < 1e9 && fabs (val) > 1e-5)
		snprintf (buf, sizeof buf-1, "%.*g", precision, val);
	else
		snprintf (buf, sizeof buf-1, "%f", val);
	gsf_output_xml_add_attr_cstr (xml, id, buf);
}

/**
 * gsf_output_xml_add_attr_color :
 * @xml :
 * @id  : optionally NULL for content
 */
void
gsf_output_xml_add_attr_color (GsfOutputXML *xml, char const *id,
			       unsigned r, unsigned g, unsigned b)
{
	char buf [4 * sizeof (unsigned)];
	sprintf (buf, "%X:%X:%X", r, g, b);
	gsf_output_xml_add_attr_cstr (xml, id, buf);
}

/**
 * gsf_output_xml_add_attr_base64 :
 * @xml :
 * @id  : optionally NULL for content
 */
void
gsf_output_xml_add_attr_base64 (GsfOutputXML *xml, char const *id,
				guint8 const *data, unsigned len)
{
	/* We could optimize and stream right to the output,
	 * or even just keep the buffer around
	 * for now we start simple */
	guint8 *tmp = gsf_base64_encode_simple (data, len);
	if (tmp == NULL)
		return;
	if (id != NULL)
		g_warning ("Stream a binary blob into an attribute ??");
	gsf_output_xml_add_attr_cstr (xml, id, tmp);
	g_free (tmp);
}

