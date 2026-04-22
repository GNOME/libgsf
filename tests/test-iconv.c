/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-iconv.c: test program for character set conversion output
 */

#include <gsf/gsf-utils.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-output-iconv.h>

#include <stdio.h>
#include <string.h>

static int
test (void)
{
	GsfOutputMemory *sink;
	GsfOutput *iconv;
	const char *input = "Hello, world! \xc2\xa1Hola! \xe2\x82\xac"; /* UTF-8: Euro sign at the end */
	/* ISO-8859-1: "Hello, world! \xa1Hola! " and then fallback for Euro */
	const guint8 expected[] = "Hello, world! \xa1Hola! \\u20ac";
	guint8 const *buf;
	gsf_off_t size;

	sink = GSF_OUTPUT_MEMORY (gsf_output_memory_new ());
	iconv = gsf_output_iconv_new (GSF_OUTPUT (sink), "ISO-8859-1", "UTF-8");
	if (!iconv) {
		g_printerr ("Failed to create GsfOutputIconv\n");
		return 1;
	}

	if (!gsf_output_write (iconv, strlen (input), (const guint8 *)input)) {
		g_printerr ("Failed to write to iconv\n");
		return 1;
	}

	if (!gsf_output_close (iconv)) {
		g_printerr ("Failed to close iconv\n");
		return 1;
	}

	size = gsf_output_size (GSF_OUTPUT (sink));
	buf = gsf_output_memory_get_bytes (sink);

	if (size != (gsf_off_t)strlen ((const char *)expected)) {
		g_printerr ("Size mismatch: expected %zd, got %" GSF_OFF_T_FORMAT "\n",
			 strlen ((const char *)expected), size);
		return 1;
	}

	if (memcmp (buf, expected, size) != 0) {
		g_printerr ("Content mismatch\n");
		g_printerr ("Expected: %s\n", (const char *)expected);
		g_printerr ("Got:      %.*s\n", (int)size, (const char *)buf);
		return 1;
	}

	g_object_unref (iconv);
	g_object_unref (sink);

	/* Test custom fallback */
	sink = GSF_OUTPUT_MEMORY (gsf_output_memory_new ());
	iconv = gsf_output_iconv_new (GSF_OUTPUT (sink), "ISO-8859-1", "UTF-8");
	g_object_set (iconv, "fallback", "[?]", NULL);

	if (!gsf_output_write (iconv, strlen (input), (const guint8 *)input)) {
		g_printerr ("Failed to write to iconv with fallback\n");
		return 1;
	}

	if (!gsf_output_close (iconv)) {
		g_printerr ("Failed to close iconv with fallback\n");
		return 1;
	}

	size = gsf_output_size (GSF_OUTPUT (sink));
	buf = gsf_output_memory_get_bytes (sink);

	const guint8 expected_fallback[] = "Hello, world! \xa1Hola! [?]";
	if (size != (gsf_off_t)strlen ((const char *)expected_fallback)) {
		g_printerr ("Fallback size mismatch: expected %zd, got %" GSF_OFF_T_FORMAT "\n",
			 strlen ((const char *)expected_fallback), size);
		return 1;
	}

	if (memcmp (buf, expected_fallback, size) != 0) {
		g_printerr ("Fallback content mismatch\n");
		return 1;
	}

	g_object_unref (iconv);
	g_object_unref (sink);

	g_print ("Iconv test passed\n");
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
