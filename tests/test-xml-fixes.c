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

	gsf_shutdown ();

	return failures != 0;
}
