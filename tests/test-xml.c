/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-xml.c: Test libxml2 wrappers
 *
 * Copyright (C) 2015	Morten Welinder <terra@gnome.org>
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

#include <stdio.h>
#include <gsf/gsf.h>

static int
test_xml_indent (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfXMLOut *xml = gsf_xml_out_new (mem);
	const char *data;
	gboolean pprint;
	int err;
	const char *expected =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<outer>\n"
		"  <data/>\n"
		"  <data attr=\"val\"/>\n"
		"  <data>text</data>\n"
		"  <data>\n"
		"    <inner>text</inner>\n"
		"  </data>\n"
		"  <data>text</data>\n"
		"  <data><inner>text</inner></data>\n"
		"</outer>\n";

	gsf_xml_out_start_element (xml, "outer");

	gsf_xml_out_start_element (xml, "data");
	gsf_xml_out_end_element (xml);

	gsf_xml_out_start_element (xml, "data");
	gsf_xml_out_add_cstr_unchecked (xml, "attr", "val");
	gsf_xml_out_end_element (xml);

	gsf_xml_out_start_element (xml, "data");
	gsf_xml_out_add_cstr_unchecked (xml, NULL, "text");
	gsf_xml_out_end_element (xml);

	gsf_xml_out_start_element (xml, "data");
	gsf_xml_out_start_element (xml, "inner");
	gsf_xml_out_add_cstr_unchecked (xml, NULL, "text");
	gsf_xml_out_end_element (xml);
	gsf_xml_out_end_element (xml);

	gsf_xml_out_start_element (xml, "data");
	pprint = gsf_xml_out_set_pretty_print (xml, FALSE);
	gsf_xml_out_add_cstr_unchecked (xml, NULL, "text");
	gsf_xml_out_set_pretty_print (xml, pprint);
	gsf_xml_out_end_element (xml);

	gsf_xml_out_start_element (xml, "data");
	pprint = gsf_xml_out_set_pretty_print (xml, FALSE);
	gsf_xml_out_start_element (xml, "inner");
	gsf_xml_out_add_cstr_unchecked (xml, NULL, "text");
	gsf_xml_out_end_element (xml);
	gsf_xml_out_set_pretty_print (xml, pprint);
	gsf_xml_out_end_element (xml);

	gsf_xml_out_end_element (xml);

	g_object_unref (xml);

	data = (const char *)gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
	g_printerr ("Got\n%s\n", data);

	err = !g_str_equal (data, expected);
	if (err)
		g_printerr ("Expected\n%s\n", expected);

	g_object_unref (mem);

	return err;
}

int
main (int argc, char *argv[])
{
	int res;

	(void)argc;
	(void)argv;

	gsf_init ();
	res = test_xml_indent ();
	gsf_shutdown ();

	return res;
}
