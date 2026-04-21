/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-compress.c: test program for GZip and BZip2 compression/decompression
 */

#include <gsf/gsf-utils.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-output-gzip.h>
#include <gsf/gsf-output-bzip.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-input-gzip.h>
#include <gsf/gsf-input-bzip.h>

#include <stdio.h>
#include <string.h>

static GString *
generate_test_data (void)
{
	GString *data = g_string_new (NULL);
	int i;
	for (i = 1; i <= 1000; i++) {
		g_string_append_printf (data, "%d bottles of beer\n", i);
	}
	return data;
}

static int
test_gzip (const char *orig_data, size_t orig_len)
{
	GsfOutputMemory *sink;
	GsfOutput *gzip_out;
	GsfInput *input_compressed;
	GsfInput *gzip_in;
	guint8 *decompressed;
	gsf_off_t compressed_size;
	GError *err = NULL;

	printf ("Testing GZip...\n");

	/* Compress */
	sink = GSF_OUTPUT_MEMORY (gsf_output_memory_new ());
	gzip_out = gsf_output_gzip_new (GSF_OUTPUT (sink), &err);
	if (!gzip_out) {
		fprintf (stderr, "Failed to create GsfOutputGZip: %s\n", err->message);
		g_error_free (err);
		return 1;
	}

	if (!gsf_output_write (gzip_out, orig_len, (const guint8 *)orig_data)) {
		fprintf (stderr, "Failed to write to GZip output\n");
		return 1;
	}
	gsf_output_close (gzip_out);
	compressed_size = gsf_output_size (GSF_OUTPUT (sink));
	printf ("  Original size: %zu, Compressed size: %" GSF_OFF_T_FORMAT "\n", orig_len, compressed_size);

	if (compressed_size * 4 > (gsf_off_t)orig_len) {
		fprintf (stderr, "  Compression ratio too low!\n");
		return 1;
	}

	/* Decompress */
	input_compressed = gsf_input_memory_new (gsf_output_memory_get_bytes (sink), compressed_size, FALSE);
	gzip_in = gsf_input_gzip_new (input_compressed, &err);
	if (!gzip_in) {
		fprintf (stderr, "Failed to create GsfInputGZip: %s\n", err ? err->message : "unknown");
		return 1;
	}

	decompressed = g_malloc (orig_len);
	if (!gsf_input_read (gzip_in, orig_len, decompressed)) {
		fprintf (stderr, "Failed to read from GZip input\n");
		return 1;
	}

	if (memcmp (decompressed, orig_data, orig_len) != 0) {
		fprintf (stderr, "  Decompressed data mismatch!\n");
		return 1;
	}

	g_free (decompressed);
	g_object_unref (gzip_in);
	g_object_unref (input_compressed);
	g_object_unref (gzip_out);
	g_object_unref (sink);

	printf ("  GZip test passed.\n");
	return 0;
}

static int
test_bzip (const char *orig_data, size_t orig_len)
{
	GsfOutputMemory *sink;
	GsfOutput *bzip_out;
	GsfInput *input_compressed;
	GsfInput *bzip_in;
	guint8 const *decompressed;
	gsf_off_t compressed_size;
	GError *err = NULL;

	printf ("Testing BZip2...\n");

	/* Compress */
	sink = GSF_OUTPUT_MEMORY (gsf_output_memory_new ());
	bzip_out = gsf_output_bzip_new (GSF_OUTPUT (sink), &err);
	if (!bzip_out) {
		printf ("  BZip2 not supported or initialization failed: %s\n", err->message);
		g_error_free (err);
		g_object_unref (sink);
		return 0; /* Not an error */
	}

	if (!gsf_output_write (bzip_out, orig_len, (const guint8 *)orig_data)) {
		fprintf (stderr, "Failed to write to BZip2 output\n");
		return 1;
	}
	gsf_output_close (bzip_out);
	compressed_size = gsf_output_size (GSF_OUTPUT (sink));
	printf ("  Original size: %zu, Compressed size: %" GSF_OFF_T_FORMAT "\n", orig_len, compressed_size);

	if (compressed_size * 4 > (gsf_off_t)orig_len) {
		fprintf (stderr, "  Compression ratio too low!\n");
		return 1;
	}

	/* Decompress */
	input_compressed = gsf_input_memory_new (gsf_output_memory_get_bytes (sink), compressed_size, FALSE);
	bzip_in = gsf_input_memory_new_from_bzip (input_compressed, &err);
	if (!bzip_in) {
		fprintf (stderr, "Failed to create BZip input: %s\n", err ? err->message : "unknown");
		return 1;
	}

	if (gsf_input_size (bzip_in) != (gsf_off_t)orig_len) {
		fprintf (stderr, "  Decompressed size mismatch!\n");
		return 1;
	}

	decompressed = gsf_input_read (bzip_in, orig_len, NULL);
	if (!decompressed) {
		fprintf (stderr, "Failed to read from BZip input\n");
		return 1;
	}

	if (memcmp (decompressed, orig_data, orig_len) != 0) {
		fprintf (stderr, "  Decompressed data mismatch!\n");
		return 1;
	}

	g_object_unref (bzip_in);
	g_object_unref (input_compressed);
	g_object_unref (bzip_out);
	g_object_unref (sink);

	printf ("  BZip2 test passed.\n");
	return 0;
}

int
main (int argc, char *argv[])
{
	GString *data;
	int res;

	gsf_init ();

	data = generate_test_data ();
	res = test_gzip (data->str, data->len);
	if (res == 0)
		res = test_bzip (data->str, data->len);

	g_string_free (data, TRUE);

	gsf_shutdown ();
	return res;
}
