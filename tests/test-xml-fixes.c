/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-xml-fixes.c: Tests for libgsf XML fixes
 *
 * Copyright (C) 2026 The libgsf authors
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

#include <gsf/gsf.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>

static int failures;

static void
check (gboolean ok, char const *what)
{
	if (!ok) {
		g_printerr ("FAIL: %s\n", what);
		failures++;
	}
}

static char *
output_to_string (GsfOutput *output)
{
	gsf_off_t size = gsf_output_size (output);
	guint8 const *data =
		gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (output));

	return g_strndup ((char const *) data, (gsize) size);
}

/* Fix 1: close callbacks return 0, so the dump returns bytes written. */
static void
test_dump_return (void)
{
	xmlDoc *doc = xmlNewDoc (BAD_CAST "1.0");
	GsfOutput *output = gsf_output_memory_new ();
	int ret;

	xmlDocSetRootElement (doc, xmlNewNode (NULL, BAD_CAST "root"));

	ret = gsf_xmlDocFormatDump (output, doc, NULL, TRUE);
	check (ret > 0, "dump returns a positive byte count");
	check ((gsf_off_t) ret == gsf_output_size (output),
	       "dump return value matches output size");

	g_object_unref (output);
	xmlFreeDoc (doc);
}

/*
 * Fix 6: an unknown encoding is rejected without touching the caller's
 * document.
 */
static void
test_dump_bad_encoding (void)
{
	xmlDoc *doc = xmlNewDoc (BAD_CAST "1.0");
	GsfOutput *output = gsf_output_memory_new ();
	int ret;

	xmlDocSetRootElement (doc, xmlNewNode (NULL, BAD_CAST "root"));
	if (doc->encoding != NULL)
		xmlFree ((char *) doc->encoding);
	doc->encoding = xmlStrdup (BAD_CAST "UTF-8");

	ret = gsf_xmlDocFormatDump (output, doc, "BOGUS-ENCODING", TRUE);
	check (ret == -1, "dump rejects an unknown encoding");
	check (gsf_output_size (output) == 0, "no output for unknown encoding");
	check (doc->encoding != NULL &&
	       strcmp ((char *) doc->encoding, "UTF-8") == 0,
	       "dump leaves the document encoding alone");

	g_object_unref (output);
	xmlFreeDoc (doc);
}

/* Fix 3: failure to find a root node returns NULL without leaking. */
static void
test_doc_new_failure (void)
{
	static GsfXMLInNode const empty_nodes[] = {
		GSF_XML_IN_NODE_END
	};
	GsfXMLInDoc *doc = gsf_xml_in_doc_new (empty_nodes, NULL);

	check (doc == NULL, "doc_new returns NULL when there is no root");
}

static gboolean saw_ok_content;

static void
data_end (GsfXMLIn *xin, G_GNUC_UNUSED GsfXMLBlob *blob)
{
	if (xin->content != NULL && strcmp (xin->content->str, "ok") == 0)
		saw_ok_content = TRUE;
}

static GsfXMLInNode const parse_nodes[] = {
	GSF_XML_IN_NODE (root, root, -1, "root", GSF_XML_NO_CONTENT,
			 NULL, NULL),
	GSF_XML_IN_NODE (root, data, -1, "data", GSF_XML_CONTENT,
			 NULL, data_end),
	GSF_XML_IN_NODE (data, data2, -1, "data", GSF_XML_CONTENT,
			 NULL, data_end),
	GSF_XML_IN_NODE_END
};

static gboolean
parse_string (GsfXMLInDoc *doc, char const *xml)
{
	GsfInput *input =
		gsf_input_memory_new ((guint8 const *) xml, strlen (xml), FALSE);
	gboolean res = gsf_xml_in_doc_parse (doc, input, NULL);

	g_object_unref (input);
	return res;
}

/*
 * Fix 2: a failed parse releases its state and leaves the document usable.
 * Fix 11: empty input takes the explicit EOF path.
 */
static void
test_parse_failure_cleanup (void)
{
	GsfXMLInDoc *doc = gsf_xml_in_doc_new (parse_nodes, NULL);

	check (doc != NULL, "doc with root created");
	if (doc == NULL)
		return;

	check (!parse_string (doc, "<root><data>abc<data>def"),
	       "truncated nested parse fails");
	check (!parse_string (doc, ""), "empty input fails cleanly");

	saw_ok_content = FALSE;
	check (parse_string (doc, "<root><data>ok</data></root>"),
	       "document is reusable after a failed parse");
	check (saw_ok_content, "reused document parses content correctly");

	gsf_xml_in_doc_free (doc);
}

static GEnumValue const enum_values[] = {
	{ 1, "ONE", "one" },
	{ 2, "TWO", "two" },
	{ 0, NULL, NULL }
};

static GFlagsValue const flag_values[] = {
	{ 1, "A", "a" },
	{ 2, "B", "b" },
	{ 0, NULL, NULL }
};

/* Fix 4: long, ulong, derived enums and derived flags all serialize. */
static void
test_gvalue_output (void)
{
	GType enum_type =
		g_enum_register_static ("TestXMLFixesEnum", enum_values);
	GType flags_type =
		g_flags_register_static ("TestXMLFixesFlags", flag_values);
	GsfOutput *output = gsf_output_memory_new ();
	GsfXMLOut *xout = gsf_xml_out_new (output);
	GValue val = G_VALUE_INIT;
	GValue parsed = G_VALUE_INIT;
#if GLIB_SIZEOF_LONG > 4
	long long_value = -1234567890123L;
	unsigned long ulong_value = 12345678901234567890UL;
#else
	long long_value = -1234567890L;
	unsigned long ulong_value = 4000000000UL;
#endif
	char *expected;
	char *actual;

	gsf_xml_out_set_pretty_print (xout, FALSE);
	gsf_xml_out_start_element (xout, "root");

	g_value_init (&val, G_TYPE_LONG);
	g_value_set_long (&val, long_value);
	gsf_xml_out_add_gvalue (xout, "l", &val);
	g_value_unset (&val);

	g_value_init (&val, G_TYPE_ULONG);
	g_value_set_ulong (&val, ulong_value);
	gsf_xml_out_add_gvalue (xout, "u", &val);
	g_value_unset (&val);

	g_value_init (&val, enum_type);
	g_value_set_enum (&val, 1);
	gsf_xml_out_add_gvalue (xout, "e", &val);
	g_value_unset (&val);

	g_value_init (&val, flags_type);
	g_value_set_flags (&val, 3);
	gsf_xml_out_add_gvalue (xout, "f", &val);
	g_value_unset (&val);

	gsf_xml_out_end_element (xout);

	expected = g_strdup_printf (
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<root l=\"%ld\" u=\"%lu\" e=\"ONE\" f=\"A|B\"/>",
		long_value, ulong_value);
	actual = output_to_string (output);
	check (g_strcmp0 (actual, expected) == 0,
	       "long, ulong, enum, and flags serialize correctly");
	g_free (actual);
	g_free (expected);

	check (gsf_xml_gvalue_from_str (&parsed, enum_type, "ONE"),
	       "derived enum parses");
	check (g_value_get_enum (&parsed) == 1,
	       "derived enum value is correct");
	g_value_unset (&parsed);

	check (gsf_xml_gvalue_from_str (&parsed, flags_type, "A|B"),
	       "derived flags parse");
	check (g_value_get_flags (&parsed) == 3,
	       "derived flags value is correct");
	g_value_unset (&parsed);

	g_object_unref (xout);
	g_object_unref (output);
}

/* Fix 7: finalizing an output object with open elements is clean. */
static void
test_unbalanced_finalize (void)
{
	GsfOutput *output = gsf_output_memory_new ();
	GsfXMLOut *xout = gsf_xml_out_new (output);

	gsf_xml_out_start_element (xout, "a");
	gsf_xml_out_start_element (xout, "b");
	gsf_xml_out_start_element (xout, "c");

	g_object_unref (xout);
	g_object_unref (output);
}

/*
 * Log handler that swallows and counts warnings so that tests exercising
 * deliberately odd input stay quiet.
 */
static int swallowed_logs;

static void
swallow_log (G_GNUC_UNUSED char const *domain,
	     G_GNUC_UNUSED GLogLevelFlags level,
	     G_GNUC_UNUSED char const *message,
	     G_GNUC_UNUSED gpointer user_data)
{
	swallowed_logs++;
}

static GFlagsValue const sep_flag_values[] = {
	{ 1, "A", "a" },
	{ 2, "B", "b" },
	{ 0, NULL, NULL }
};

static guint
parse_flags (GType type, char const *str)
{
	GValue v = G_VALUE_INIT;
	guint res;

	if (!gsf_xml_gvalue_from_str (&v, type, str))
		return (guint)-1;
	res = g_value_get_flags (&v);
	g_value_unset (&v);
	return res;
}

/*
 * Flag separators: a trailing '|' used to read past the end of the string
 * and an empty field ("A||B") swallowed the following flag.
 */
static void
test_flags_separators (void)
{
	GType type =
		g_flags_register_static ("TestXMLFixesSepFlags", sep_flag_values);

	g_log_set_default_handler (swallow_log, NULL);
	swallowed_logs = 0;

	check (parse_flags (type, "A|B") == 3, "flags: A|B");
	check (parse_flags (type, " A | B ") == 3, "flags: padded A | B");
	check (parse_flags (type, "A|") == 1, "flags: trailing separator");
	check (parse_flags (type, "A|B|") == 3, "flags: trailing separator 2");
	check (parse_flags (type, "|A") == 1, "flags: leading separator");
	check (parse_flags (type, "A||B") == 3, "flags: empty field");
	check (parse_flags (type, "|") == 0, "flags: lone separator");
	check (parse_flags (type, "") == 0, "flags: empty string");
	check (parse_flags (type, "5") == 5, "flags: numeric");
	check (swallowed_logs == 0, "flags: no warnings for valid input");

	g_log_set_default_handler (g_log_default_handler, NULL);
}

static GsfXMLInNS const test_ns[] = {
	GSF_XML_IN_NS (1, "http://example.org/known"),
	GSF_XML_IN_NS_END
};

static GsfXMLInNode const ns_nodes[] = {
	GSF_XML_IN_NODE_FULL (root, root, -1, "root", GSF_XML_NO_CONTENT,
			      FALSE, TRUE, NULL, NULL, 0),
	GSF_XML_IN_NODE (root, data, -1, "data", GSF_XML_CONTENT,
			 NULL, data_end),
	GSF_XML_IN_NODE_END
};

/*
 * An unknown default namespace ("xmlns" with no prefix) used to be
 * treated as "xmlns:prefix" and read one byte past the attribute name.
 * Most useful under valgrind or AddressSanitizer.
 */
static void
test_unknown_default_ns (void)
{
	GsfXMLInDoc *doc = gsf_xml_in_doc_new (ns_nodes, test_ns);

	check (doc != NULL, "ns doc created");
	if (doc == NULL)
		return;

	g_log_set_default_handler (swallow_log, NULL);

	saw_ok_content = FALSE;
	check (parse_string (doc,
			     "<root><data xmlns=\"http://example.org/unknown\">"
			     "ok</data></root>"),
	       "unknown default namespace parses");
	check (saw_ok_content, "content survives unknown default namespace");

	saw_ok_content = FALSE;
	check (parse_string (doc,
			     "<root><data xmlns:x=\"http://example.org/unknown\">"
			     "ok</data></root>"),
	       "unknown prefixed namespace parses");
	check (saw_ok_content, "content survives unknown prefixed namespace");

	g_log_set_default_handler (g_log_default_handler, NULL);
	gsf_xml_in_doc_free (doc);
}

/* Extension machinery used by the next two tests. */
static int ext_child_count;
static gboolean push_ext_from_root;
static GsfXMLInDoc *ext_doc;

static void
ext_child_start (G_GNUC_UNUSED GsfXMLIn *xin,
		 G_GNUC_UNUSED xmlChar const **attrs)
{
	ext_child_count++;
}

static GsfXMLInNode const ext_nodes[] = {
	GSF_XML_IN_NODE (extroot, extroot, -1, "extroot", GSF_XML_NO_CONTENT,
			 NULL, NULL),
	GSF_XML_IN_NODE (extroot, extchild, -1, "ext", GSF_XML_NO_CONTENT,
			 ext_child_start, NULL),
	GSF_XML_IN_NODE_END
};

static void
push_ext_start (GsfXMLIn *xin, xmlChar const **attrs)
{
	if (push_ext_from_root)
		gsf_xml_in_push_state (xin, ext_doc, NULL, NULL, attrs);
}

static GsfXMLInNode const ext_host_nodes[] = {
	GSF_XML_IN_NODE (root, root, -1, "root", GSF_XML_NO_CONTENT,
			 push_ext_start, NULL),
	GSF_XML_IN_NODE_END
};

/*
 * A failed parse used to leave its extensions attached to the shared doc,
 * leaking them and re-activating them in later parses.
 */
static void
test_stale_extension (void)
{
	GsfXMLInDoc *doc;

	ext_doc = gsf_xml_in_doc_new (ext_nodes, NULL);
	doc = gsf_xml_in_doc_new (ext_host_nodes, NULL);
	check (ext_doc != NULL && doc != NULL, "extension docs created");
	if (ext_doc == NULL || doc == NULL)
		goto done;

	g_log_set_default_handler (swallow_log, NULL);

	/*
	 * The extension handles <ext/> and then the document is truncated
	 * while root (which owns the extension) is still open.
	 */
	push_ext_from_root = TRUE;
	ext_child_count = 0;
	check (!parse_string (doc, "<root><ext/><other>"),
	       "truncated parse fails");
	check (ext_child_count == 1, "extension active in failed parse");

	/* ...but must not survive into a parse that does not push it. */
	push_ext_from_root = FALSE;
	ext_child_count = 0;
	check (parse_string (doc, "<root><ext/></root>"),
	       "later parse succeeds");
	check (ext_child_count == 0,
	       "extension from failed parse is not reused");

	g_log_set_default_handler (g_log_default_handler, NULL);
done:
	if (doc != NULL)
		gsf_xml_in_doc_free (doc);
	if (ext_doc != NULL)
		gsf_xml_in_doc_free (ext_doc);
	ext_doc = NULL;
}

static void
push_ext_start_always (GsfXMLIn *xin, xmlChar const **attrs)
{
	gsf_xml_in_push_state (xin, ext_doc, NULL, NULL, attrs);
}

static GsfXMLInNode const sharing_nodes[] = {
	GSF_XML_IN_NODE (root, root, -1, "root", GSF_XML_NO_CONTENT,
			 NULL, NULL),
	GSF_XML_IN_NODE_FULL (root, sh, -1, "sh", GSF_XML_NO_CONTENT,
			      TRUE, FALSE, push_ext_start_always, NULL, 0),
	GSF_XML_IN_NODE_END
};

/*
 * Extensions belong to the node that registered them.  They used to be
 * looked up on whichever ancestor the share-children search ended on.
 */
static void
test_extension_on_sharing_node (void)
{
	GsfXMLInDoc *doc;

	ext_doc = gsf_xml_in_doc_new (ext_nodes, NULL);
	doc = gsf_xml_in_doc_new (sharing_nodes, NULL);
	check (ext_doc != NULL && doc != NULL, "sharing docs created");
	if (ext_doc == NULL || doc == NULL)
		goto done;

	g_log_set_default_handler (swallow_log, NULL);

	ext_child_count = 0;
	check (parse_string (doc, "<root><sh><ext/></sh></root>"),
	       "sharing document parses");
	check (ext_child_count == 1,
	       "extension on sharing node handles its child");

	g_log_set_default_handler (g_log_default_handler, NULL);
done:
	if (doc != NULL)
		gsf_xml_in_doc_free (doc);
	if (ext_doc != NULL)
		gsf_xml_in_doc_free (ext_doc);
	ext_doc = NULL;
}

static GsfXMLInNode const orphan_nodes[] = {
	GSF_XML_IN_NODE (root, root, -1, "root", GSF_XML_NO_CONTENT,
			 NULL, NULL),
	{ "orphan", -1, "orphan", NULL, NULL, NULL, { 0 },
	  GSF_XML_NO_CONTENT, FALSE, FALSE },
	GSF_XML_IN_NODE_END
};

/* A node without a parent_id is rejected instead of crashing. */
static void
test_null_parent_id (void)
{
	GsfXMLInDoc *doc;

	g_log_set_default_handler (swallow_log, NULL);
	swallowed_logs = 0;

	doc = gsf_xml_in_doc_new (orphan_nodes, NULL);
	check (doc != NULL, "doc survives a node with no parent_id");
	check (swallowed_logs > 0, "node with no parent_id is reported");
	if (doc != NULL) {
		check (parse_string (doc, "<root/>"),
		       "doc with rejected node still parses");
		gsf_xml_in_doc_free (doc);
	}

	g_log_set_default_handler (g_log_default_handler, NULL);
}

/* '\r' in element content must be escaped or the parser eats it. */
static void
test_cr_escaping (void)
{
	GsfOutput *output = gsf_output_memory_new ();
	GsfXMLOut *xout = gsf_xml_out_new (output);
	char *text;
	xmlDoc *parsed;
	xmlChar *content;

	gsf_xml_out_set_pretty_print (xout, FALSE);
	gsf_xml_out_start_element (xout, "root");
	gsf_xml_out_add_cstr (xout, "attr", "x\ry");
	gsf_xml_out_add_cstr (xout, NULL, "a\rb\r\nc");
	gsf_xml_out_end_element (xout);

	text = output_to_string (output);
	check (strstr (text, "\r") == NULL, "no raw CR in output");
	check (strstr (text, "a&#13;b&#13;\nc") != NULL,
	       "CR in content is escaped");
	check (strstr (text, "attr=\"x&#13;y\"") != NULL,
	       "CR in attribute is escaped");

	parsed = xmlReadMemory (text, strlen (text), "mem.xml", NULL, 0);
	check (parsed != NULL, "escaped output is well formed");
	if (parsed != NULL) {
		xmlNode *root = xmlDocGetRootElement (parsed);
		xmlChar *attr = xmlGetProp (root, BAD_CAST "attr");

		content = xmlNodeGetContent (root);
		check (content != NULL &&
		       strcmp ((char *) content, "a\rb\r\nc") == 0,
		       "CR in content round-trips");
		check (attr != NULL && strcmp ((char *) attr, "x\ry") == 0,
		       "CR in attribute round-trips");
		xmlFree (content);
		xmlFree (attr);
		xmlFreeDoc (parsed);
	}

	g_free (text);
	g_object_unref (xout);
	g_object_unref (output);
}


int
main (void)
{
	gsf_init ();

	test_dump_return ();
	test_dump_bad_encoding ();
	test_doc_new_failure ();
	test_parse_failure_cleanup ();
	test_gvalue_output ();
	test_unbalanced_finalize ();
	test_flags_separators ();
	test_unknown_default_ns ();
	test_stale_extension ();
	test_extension_on_sharing_node ();
	test_cr_escaping ();
	/* Crashes unpatched libgsf, so keep it last. */
	test_null_parent_id ();

	gsf_shutdown ();

	return failures != 0;
}
