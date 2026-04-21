/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-csv.c: test program for CSV output
 */

#include <gsf/gsf-utils.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-output-csv.h>

#include <stdio.h>
#include <string.h>

static int
test (void)
{
	GsfOutputMemory *sink;
	GsfOutputCsv *csv;
	guint8 const *buf;
	gsf_off_t size;

	/* Test 1: Standard CSV with auto-quoting */
	sink = GSF_OUTPUT_MEMORY (gsf_output_memory_new ());
	csv = g_object_new (GSF_OUTPUT_CSV_TYPE,
			    "sink", sink,
			    "quoting-mode", GSF_OUTPUT_CSV_QUOTING_MODE_AUTO,
			    "quoting-triggers", ",\"",
			    NULL);

	gsf_output_csv_write_field (csv, "Field1", -1);
	gsf_output_csv_write_field (csv, "Field,2", -1);
	gsf_output_csv_write_field (csv, "Field\"3\"", -1);
	gsf_output_csv_write_eol (csv);
	gsf_output_close (GSF_OUTPUT (csv));

	size = gsf_output_size (GSF_OUTPUT (sink));
	buf = gsf_output_memory_get_bytes (sink);
	const char expected1[] = "Field1,\"Field,2\",\"Field\"\"3\"\"\"\n";
	
	if (size != (gsf_off_t)strlen (expected1) || memcmp (buf, expected1, size) != 0) {
		fprintf (stderr, "Test 1 failed\n");
		fprintf (stderr, "Expected: %s", expected1);
		fprintf (stderr, "Got:      %.*s", (int)size, (const char *)buf);
		return 1;
	}
	g_object_unref (csv);
	g_object_unref (sink);

	/* Test 2: Custom separator and quoting-always */
	sink = GSF_OUTPUT_MEMORY (gsf_output_memory_new ());
	csv = g_object_new (GSF_OUTPUT_CSV_TYPE,
			    "sink", sink,
			    "separator", ";",
			    "quoting-mode", GSF_OUTPUT_CSV_QUOTING_MODE_ALWAYS,
			    NULL);

	gsf_output_csv_write_field (csv, "A", -1);
	gsf_output_csv_write_field (csv, "B", -1);
	gsf_output_csv_write_eol (csv);
	gsf_output_close (GSF_OUTPUT (csv));

	size = gsf_output_size (GSF_OUTPUT (sink));
	buf = gsf_output_memory_get_bytes (sink);
	const char expected2[] = "\"A\";\"B\"\n";

	if (size != (gsf_off_t)strlen (expected2) || memcmp (buf, expected2, size) != 0) {
		fprintf (stderr, "Test 2 failed\n");
		return 1;
	}
	g_object_unref (csv);
	g_object_unref (sink);

	printf ("CSV test passed\n");
	return 0;
}

int
main (int argc, char *argv[])
{
	int res;
	gsf_init ();
	res = test ();
	gsf_shutdown ();
	return res;
}
