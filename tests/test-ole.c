/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-ole.c: test program for OLE read/write
 */

#include <gsf/gsf-utils.h>
#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-outfile.h>
#include <gsf/gsf-outfile-msole.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-doc-meta-data.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-meta-names.h>

#include <stdio.h>
#include <string.h>

static const char data1[] = "Stream 1 content";
static const char data2[] = "Another stream with different content";
static const char prop_title[] = "Test Title";
static const char prop_author[] = "Test Author";
static const char prop_company[] = "Test Company";

static gboolean
test_write_streams (GsfOutfile *outfile)
{
	GsfOutput *child;
	GsfDocMetaData *meta;
	GValue *val;

	printf ("Creating archive\n");

	/* Data streams */
	child = gsf_outfile_new_child (outfile, "Stream1", FALSE);
	if (!child) return FALSE;
	if (!gsf_output_write (child, sizeof (data1) - 1, (const guint8 *)data1)) return FALSE;
	gsf_output_close (child);
	g_object_unref (child);

	child = gsf_outfile_new_child (outfile, "Stream2", FALSE);
	if (!child) return FALSE;
	if (!gsf_output_write (child, sizeof (data2) - 1, (const guint8 *)data2)) return FALSE;
	gsf_output_close (child);
	g_object_unref (child);

	/* Summary Information */
	child = gsf_outfile_new_child (outfile, "\005SummaryInformation", FALSE);
	if (!child) return FALSE;
	meta = gsf_doc_meta_data_new ();
	
	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_STRING);
	g_value_set_static_string (val, prop_title);
	gsf_doc_meta_data_insert (meta, g_strdup (GSF_META_NAME_TITLE), val);

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_STRING);
	g_value_set_static_string (val, prop_author);
	gsf_doc_meta_data_insert (meta, g_strdup (GSF_META_NAME_CREATOR), val);

	if (!gsf_doc_meta_data_write_to_msole (meta, child, FALSE)) return FALSE;
	g_object_unref (meta);
	gsf_output_close (child);
	g_object_unref (child);

	/* Document Summary Information */
	child = gsf_outfile_new_child (outfile, "\005DocumentSummaryInformation", FALSE);
	if (!child) return FALSE;
	meta = gsf_doc_meta_data_new ();

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_STRING);
	g_value_set_static_string (val, prop_company);
	gsf_doc_meta_data_insert (meta, g_strdup (GSF_META_NAME_COMPANY), val);

	if (!gsf_doc_meta_data_write_to_msole (meta, child, TRUE)) return FALSE;
	g_object_unref (meta);
	gsf_output_close (child);
	g_object_unref (child);

	return TRUE;
}

static gboolean
verify_prop (GsfDocMetaData *meta, const char *name, const char *expected_val)
{
	GsfDocProp *prop = gsf_doc_meta_data_lookup (meta, name);
	if (!prop) {
		fprintf (stderr, "Property %s not found\n", name);
		return FALSE;
	}
	const GValue *val = gsf_doc_prop_get_val (prop);
	if (!G_VALUE_HOLDS_STRING (val)) {
		fprintf (stderr, "Property %s is not a string\n", name);
		return FALSE;
	}
	if (strcmp (g_value_get_string (val), expected_val) != 0) {
		fprintf (stderr, "Property %s mismatch: expected %s, got %s\n", 
			 name, expected_val, g_value_get_string (val));
		return FALSE;
	}
	return TRUE;
}

static gboolean
test_read_streams (GsfInfile *infile)
{
	GsfInput *child;
	guint8 buf[1024];
	gsf_off_t size;
	GsfDocMetaData *meta;
	GError *err = NULL;

	printf ("Verifying archive\n");

	/* Data streams */
	child = gsf_infile_child_by_name (infile, "Stream1");
	if (!child) {
		fprintf (stderr, "Stream1 not found\n");
		return FALSE;
	}
	size = gsf_input_size (child);
	if (size != (gsf_off_t)(sizeof (data1) - 1)) return FALSE;
	if (!gsf_input_read (child, size, buf)) return FALSE;
	if (memcmp (buf, data1, size) != 0) return FALSE;
	g_object_unref (child);

	child = gsf_infile_child_by_name (infile, "Stream2");
	if (!child) {
		fprintf (stderr, "Stream2 not found\n");
		return FALSE;
	}
	size = gsf_input_size (child);
	if (size != (gsf_off_t)(sizeof (data2) - 1)) return FALSE;
	if (!gsf_input_read (child, size, buf)) return FALSE;
	if (memcmp (buf, data2, size) != 0) return FALSE;
	g_object_unref (child);

	/* Summary Information */
	child = gsf_infile_child_by_name (infile, "\005SummaryInformation");
	if (!child) {
		fprintf (stderr, "SummaryInformation not found\n");
		return FALSE;
	}
	meta = gsf_doc_meta_data_new ();
	err = gsf_doc_meta_data_read_from_msole (meta, child);
	if (err) {
		fprintf (stderr, "Error reading SummaryInformation: %s\n", err->message);
		g_error_free (err);
		return FALSE;
	}
	if (!verify_prop (meta, GSF_META_NAME_TITLE, prop_title)) return FALSE;
	if (!verify_prop (meta, GSF_META_NAME_CREATOR, prop_author)) return FALSE;
	g_object_unref (meta);
	g_object_unref (child);

	/* Document Summary Information */
	child = gsf_infile_child_by_name (infile, "\005DocumentSummaryInformation");
	if (!child) {
		fprintf (stderr, "DocumentSummaryInformation not found\n");
		return FALSE;
	}
	meta = gsf_doc_meta_data_new ();
	err = gsf_doc_meta_data_read_from_msole (meta, child);
	if (err) {
		fprintf (stderr, "Error reading DocumentSummaryInformation: %s\n", err->message);
		g_error_free (err);
		return FALSE;
	}
	if (!verify_prop (meta, GSF_META_NAME_COMPANY, prop_company)) return FALSE;
	g_object_unref (meta);
	g_object_unref (child);

	return TRUE;
}

static gboolean
test_write_many_streams (GsfOutfile *outfile, int n)
{
	int i;

	printf ("Creating %d children\n", n);

	for (i = 0; i < n; i++) {
		char name[32];
		sprintf (name, "S%d", i);
		GsfOutput *child = gsf_outfile_new_child (outfile, name, FALSE);
		if (!child) return FALSE;
		if (!gsf_output_write (child, 1, (const guint8 *)"X")) return FALSE;
		gsf_output_close (child);
		g_object_unref (child);
	}
	return TRUE;
}

static gboolean
test_read_many_streams (GsfInfile *infile, int n)
{
	int i;

	printf ("Verifying %d children\n", n);

	for (i = 0; i < n; i++) {
		char name[32];
		sprintf (name, "S%d", i);
		GsfInput *child = gsf_infile_child_by_name (infile, name);
		if (!child) {
			fprintf (stderr, "Stream %s not found\n", name);
			return FALSE;
		}
		g_object_unref (child);
	}
	return TRUE;
}

static int
test (int argc, char *argv[])
{
	GsfOutput  *output;
	GsfOutfile *outfile;
	GsfInput   *input;
	GsfInfile  *infile;
	GError     *err = NULL;
	const char *filename = "test-archive.ole";
	int num_extra = 0;

	if (argc > 1)
		filename = argv[1];
	if (argc > 2)
		num_extra = atoi (argv[2]);

	/* Write */
	output = gsf_output_stdio_new (filename, &err);
	if (!output) {
		fprintf (stderr, "Failed to create output: %s\n", err->message);
		g_error_free (err);
		return 1;
	}
	outfile = gsf_outfile_msole_new (output);
	g_object_unref (output);

	if (!test_write_streams (outfile)) {
		fprintf (stderr, "Failed to write streams\n");
		return 1;
	}

	if (num_extra > 0) {
		if (!test_write_many_streams (outfile, num_extra)) {
			fprintf (stderr, "Failed to write many streams\n");
			return 1;
		}
	}

	if (!gsf_output_close (GSF_OUTPUT (outfile))) return 1;
	g_object_unref (outfile);
	printf ("Archive created.\n");

	/* Read */
	input = gsf_input_stdio_new (filename, &err);
	if (!input) {
		fprintf (stderr, "Failed to open input: %s\n", err->message);
		g_error_free (err);
		return 1;
	}
	infile = gsf_infile_msole_new (input, &err);
	if (!infile) {
		fprintf (stderr, "Failed to open OLE infile: %s\n", err ? err->message : "unknown error");
		if (err) g_error_free (err);
		g_object_unref (input);
		return 1;
	}

	if (!test_read_streams (infile)) {
		fprintf (stderr, "Failed to read streams back correctly\n");
		return 1;
	}

	if (num_extra > 0) {
		if (!test_read_many_streams (infile, num_extra)) {
			fprintf (stderr, "Failed to read many streams back correctly\n");
			return 1;
		}
	}

	g_object_unref (infile);
	g_object_unref (input);

	printf ("OLE read/write test passed\n");
	return 0;
}

int
main (int argc, char *argv[])
{
	int res;
	gsf_init ();
	res = test (argc, argv);
	gsf_shutdown ();
	return res;
}
