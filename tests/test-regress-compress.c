/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-compress.c: regression tests for the compression inputs
 */

#include <gsf/gsf.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-output-bzip.h>
#include <gsf/gsf-input-bzip.h>
#include <gsf/gsf-output-gzip.h>
#include <gsf/gsf-output-iochannel.h>
#include <gsf/gsf-input-gzip.h>

#include "libgsf-regress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Probe bzip2 support: the bzip constructors fail cleanly with "BZ2
 * support not enabled" when the library was built without it
 * (--without-bz2), and the bzip2 checks below only make sense with
 * support present.  Prints a skip notice and returns FALSE when absent.
 * (zlib support, and therefore the gzip wrappers, is not conditional.)
 */
static gboolean
probe_bzip2 (void)
{
	GsfOutput *probe_sink = gsf_output_memory_new ();
	GError *probe_err = NULL;
	GsfOutput *probe = gsf_output_bzip_new (probe_sink, &probe_err);
	gboolean ok;

	if (probe != NULL) {
		gsf_output_close (probe);
		g_object_unref (probe);
		ok = TRUE;
	} else {
		printf ("skipping bzip2 checks: %s\n",
			(probe_err != NULL)
				? probe_err->message
				: "bzip2 support not available");
		g_clear_error (&probe_err);
		ok = FALSE;
	}
	g_object_unref (probe_sink);
	return ok;
}

/* Compress @len bytes of @content through the bzip2 or gzip output
 * wrapper and return the compressed bytes (owned by @mem). */
static guint8 const *
compress_bytes (GsfOutput *mem, gboolean is_bzip2,
		guint8 const *content, size_t len, gsf_off_t *size)
{
	GsfOutput *comp = is_bzip2
		? (GsfOutput *)gsf_output_bzip_new (mem, NULL)
		: gsf_output_gzip_new (mem, NULL);
	guint8 const *bytes = NULL;

	if (comp == NULL)
		return NULL;
	if (len > 0)
		gsf_output_write (comp, len, content);
	gsf_output_close (comp);
	g_object_unref (comp);
	bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
	*size = gsf_output_size (mem);
	return bytes;
}

/*
 * Decompress @size bytes from @bytes (bzip2 or gzip format) and return
 * the resulting input.  The returned input owns its source; the caller
 * only unrefs the result.
 */
static GsfInput *
decompress_bytes (guint8 const *bytes, gsf_off_t size, gboolean is_bzip2)
{
	GsfInput *in = gsf_input_memory_new_clone (bytes, size);
	GsfInput *res;

	if (is_bzip2) {
		res = gsf_input_memory_new_from_bzip (in, NULL);
		g_object_unref (in);
	} else {
		res = gsf_input_gzip_new (in, NULL);
		g_object_unref (in);	/* the wrapper holds its own reference */
	}
	return res;
}

/* Check that @input contains exactly @len bytes matching @content. */
static void
check_content (GsfInput *input, guint8 const *content, size_t len,
	       char const *what)
{
	guint8 *buf = g_new (guint8, len > 0 ? len : 1);
	guint8 const *r;

	check (input != NULL, "%s", what);
	if (input == NULL) {
		g_free (buf);
		return;
	}
	check (gsf_input_size (input) == (gsf_off_t)len, "%s: size matches",
	       what);
	r = (len > 0) ? gsf_input_read (input, len, buf) : NULL;
	check ((len == 0 && r == NULL) ||
	       (len > 0 && r != NULL && memcmp (r, content, len) == 0),
	       "%s: content matches", what);
	g_free (buf);
}

/*
 * gsf_input_uncompress() must actually decompress bzip2 data.  The 4-byte
 * header probe consumed up front was never rewound, so the bzip2 branch
 * decompressed from offset 4, failed silently, and the still-compressed
 * stream was returned as if it were plain data.
 */
static void
test_uncompress_bzip2 (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	guint8 const *bytes;
	gsf_off_t size;

	bytes = compress_bytes (mem, TRUE, (guint8 const *)"hello world", 11, &size);
	check (bytes != NULL && size > 11, "bzip compression produced data");
	if (bytes == NULL) {
		g_object_unref (mem);
		return;
	}

	{
		GsfInput *in = gsf_input_memory_new_clone (bytes, size);
		GsfInput *res = gsf_input_uncompress (in); /* takes ownership */
		check (res != NULL, "uncompress returned a stream");
		check_content (res, (guint8 const *)"hello world", 11,
			       "uncompress decompressed the bzip2 stream");
		if (res != NULL)
			g_object_unref (res);
	}

	g_object_unref (mem);
}

/*
 * A truncated bzip2 stream must be rejected with an error instead of
 * silently succeeding with truncated output: the decompress loop also
 * exits on input exhaustion, so BZ_STREAM_END has to be required.
 */
static void
test_truncated_bzip2 (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	guint8 const *bytes;
	gsf_off_t size;

	bytes = compress_bytes (mem, TRUE, (guint8 const *)"hello world", 11, &size);
	if (bytes == NULL) {
		g_object_unref (mem);
		return;
	}

	{
		GsfInput *trunc = gsf_input_memory_new_clone (bytes, size - 2);
		GsfInput *part = gsf_input_memory_new_from_bzip (trunc, NULL);
		check (part == NULL, "truncated bzip2 stream is rejected");
		if (part != NULL)
			g_object_unref (part);
		g_object_unref (trunc);
	}

	g_object_unref (mem);
}

/*
 * Basic bzip2 round trip: compress a small string and read it back
 * through the bzip2 input wrapper.
 */
static void
test_bzip2_roundtrip (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	guint8 const *bytes;
	gsf_off_t size;

	bytes = compress_bytes (mem, TRUE, (guint8 const *)"hello world", 11, &size);
	if (bytes == NULL) {
		g_object_unref (mem);
		return;
	}
	check_content (decompress_bytes (bytes, size, TRUE),
		       (guint8 const *)"hello world", 11,
		       "bzip2 round trip works");

	g_object_unref (mem);
}

/*
 * A multi-chunk bzip2 round trip: the compressor and the decompressor
 * both work in 32 KB chunks, so a payload larger than one chunk
 * exercises the chunked loops on both sides.
 */
static void
test_bzip2_large_roundtrip (void)
{
	guint8 *pattern = g_new (guint8, 100000);
	GsfOutput *mem;
	guint8 const *bytes;
	gsf_off_t size;

	regress_fill_pattern (pattern, 100000);
	mem = gsf_output_memory_new ();
	bytes = compress_bytes (mem, TRUE, pattern, 100000, &size);
	if (bytes == NULL) {
		g_object_unref (mem);
		g_free (pattern);
		return;
	}
	check (size > 0 && size < 100000, "large payload compressed");
	check_content (decompress_bytes (bytes, size, TRUE), pattern, 100000,
		       "large bzip2 round trip works");

	g_object_unref (mem);
	g_free (pattern);
}

/*
 * Compressing nothing must still produce a valid (empty) bzip2 stream,
 * and decompressing it must yield an empty input without errors.
 */
static void
test_bzip2_empty (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	guint8 const *bytes;
	gsf_off_t size;

	bytes = compress_bytes (mem, TRUE, NULL, 0, &size);
	check (bytes != NULL && size > 0, "empty stream still has a header");
	if (bytes == NULL) {
		g_object_unref (mem);
		return;
	}
	check_content (decompress_bytes (bytes, size, TRUE), NULL, 0,
		       "empty bzip2 round trip works");

	g_object_unref (mem);
}

/*
 * Basic gzip round trip: compress a small string and read it back
 * through the gzip input wrapper.
 */
static void
test_gzip_roundtrip (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	guint8 const *bytes;
	gsf_off_t size;

	bytes = compress_bytes (mem, FALSE, (guint8 const *)"hello", 5, &size);
	if (bytes == NULL) {
		g_object_unref (mem);
		return;
	}
	check_content (decompress_bytes (bytes, size, FALSE),
		       (guint8 const *)"hello", 5,
		       "gzip round trip works");

	g_object_unref (mem);
}

/*
 * A multi-chunk gzip round trip (32 KB compression buffer).
 */
static void
test_gzip_large_roundtrip (void)
{
	guint8 *pattern = g_new (guint8, 100000);
	GsfOutput *mem;
	guint8 const *bytes;
	gsf_off_t size;

	regress_fill_pattern (pattern, 100000);
	mem = gsf_output_memory_new ();
	bytes = compress_bytes (mem, FALSE, pattern, 100000, &size);
	if (bytes == NULL) {
		g_object_unref (mem);
		g_free (pattern);
		return;
	}
	check_content (decompress_bytes (bytes, size, FALSE), pattern, 100000,
		       "large gzip round trip works");

	g_object_unref (mem);
	g_free (pattern);
}

/*
 * gsf_input_uncompress() must also handle gzip data (the gzip branch),
 * not just bzip2.
 */
static void
test_uncompress_gzip (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	guint8 const *bytes;
	gsf_off_t size;

	bytes = compress_bytes (mem, FALSE, (guint8 const *)"hello world", 11, &size);
	if (bytes == NULL) {
		g_object_unref (mem);
		return;
	}

	{
		GsfInput *in = gsf_input_memory_new_clone (bytes, size);
		GsfInput *res = gsf_input_uncompress (in); /* takes ownership */
		check_content (res, (guint8 const *)"hello world", 11,
			       "uncompress decompressed the gzip stream");
		if (res != NULL)
			g_object_unref (res);
	}

	g_object_unref (mem);
}

/*
 * Data that is neither gzip nor bzip2 must pass through uncompress
 * unchanged, with the read position restored after the header probe.
 */
static void
test_uncompress_passthrough (void)
{
	char const *text = "plain payload, not compressed";
	GsfInput *in = gsf_input_memory_new_clone
		((guint8 const *)text, strlen (text));
	GsfInput *res = gsf_input_uncompress (in); /* takes ownership */

	check (res == in, "unrecognized data is passed through");
	check (res != NULL && gsf_input_tell (res) == 0,
	       "read position restored after the probe");
	check_content (res, (guint8 const *)text, strlen (text),
		       "passthrough content intact");
	if (res != NULL)
		g_object_unref (res);
}

/*
 * gsf_input_dup() of a gzip input must succeed when the caller passes an
 * err pointer.  gsf_input_gzip_dup() tested `if (err)` (the pointer
 * itself) instead of the dup result, so every dup with a non-NULL err
 * pointer failed and leaked the dup'd source.
 */
static void
test_gzip_dup (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	guint8 const *bytes;
	gsf_off_t size;

	bytes = compress_bytes (mem, FALSE, (guint8 const *)"hello", 5, &size);
	if (bytes == NULL) {
		g_object_unref (mem);
		return;
	}

	{
		GsfInput *in = gsf_input_memory_new_clone (bytes, size);
		GsfInput *gz = gsf_input_gzip_new (in, NULL);
		GsfInput *dup;
		GError *err = NULL;

		check (gz != NULL, "gzip input opened");
		if (gz == NULL) {
			g_object_unref (in);
			g_object_unref (mem);
			return;
		}

		dup = gsf_input_dup (gz, &err);
		check (dup != NULL, "gsf_input_dup of gzip input");
		if (dup != NULL) {
			guint8 buf[5];
			guint8 const *r;
			check (gsf_input_size (dup) == 5, "duplicate size is 5");
			r = gsf_input_read (dup, 5, buf);
			check (r != NULL && memcmp (r, "hello", 5) == 0,
			       "duplicate content matches");
			g_object_unref (dup);
		}
		if (err != NULL) {
			g_printerr ("dup error: %s\n", err->message);
			g_error_free (err);
		}

		g_object_unref (gz);
		g_object_unref (in);
	}

	g_object_unref (mem);
}

/*
 * When the sink of a bzip2 output fails, gsf_output_write returned FALSE
 * but left gsf_output_error() NULL on the bzip output (every failure
 * path in gsf-output-bzip.c only logged or returned).  Use /dev/full as
 * the sink and write incompressible data, so that compressed blocks
 * really reach the sink.
 */
static void
test_bzip2_sink_failure_sets_error (void)
{
	enum { N = 256 * 1024 };
	GIOChannel *chan;
	GsfOutput *sink, *bz;
	guint8 *buf;
	guint32 x = 2463534242u;
	int i;

	if (!g_file_test ("/dev/full", G_FILE_TEST_EXISTS)) {
		g_print ("skipped: /dev/full not available\n");
		return;
	}
	chan = g_io_channel_new_file ("/dev/full", "w", NULL);
	if (chan == NULL) {
		g_print ("skipped: cannot open /dev/full\n");
		return;
	}
	g_io_channel_set_encoding (chan, NULL, NULL);
	sink = gsf_output_iochannel_new (chan);

	bz = gsf_output_bzip_new (sink, NULL);
	check (bz != NULL, "bzip2 output created");
	if (bz == NULL) {
		g_object_unref (sink);
		return;
	}

	buf = g_malloc (N);
	for (i = 0; i < N; i++) {	/* xorshift32: incompressible */
		x ^= x << 13; x ^= x >> 17; x ^= x << 5;
		buf[i] = (guint8)x;
	}

	/* The sink may accept some data in its buffer, so write until
	 * something fails (bounded by a few rounds).  */
	for (i = 0; i < 8 && gsf_output_write (bz, N, buf); i++)
		;
	check (i < 8, "writing through a failing sink eventually fails");
	check (gsf_output_error (bz) != NULL,
	       "the sink failure is recorded as the bzip2 output's error");

	g_free (buf);
	gsf_output_close (bz);
	g_object_unref (bz);
	gsf_output_close (sink);
	g_object_unref (sink);
}

/* A simple in-memory input whose next read can be made to fail. */
typedef struct { GsfInput base; guint8 const *data; } FlakyInput;
typedef struct { GsfInputClass base; } FlakyInputClass;
static gboolean flaky_fail_next;

static guint8 const *
flaky_read (GsfInput *in, size_t n, guint8 *buf)
{
	FlakyInput *f = (FlakyInput *)in;
	guint8 const *p = f->data + in->cur_offset;

	if (flaky_fail_next) {
		flaky_fail_next = FALSE;
		return NULL;
	}
	if (buf == NULL)
		return p;
	memcpy (buf, p, n);
	return buf;
}

static gboolean
flaky_seek (G_GNUC_UNUSED GsfInput *in, G_GNUC_UNUSED gsf_off_t off,
	    G_GNUC_UNUSED GSeekType whence)
{
	return FALSE;
}

static void
flaky_class_init (gpointer k, G_GNUC_UNUSED gpointer d)
{
	GsfInputClass *c = k;
	c->Read = flaky_read;
	c->Seek = flaky_seek;
}

static GType
flaky_get_type (void)
{
	static GType t;
	if (!t) {
		GTypeInfo i = { sizeof (FlakyInputClass), NULL, NULL, flaky_class_init,
				NULL, NULL, sizeof (FlakyInput), 0, NULL, NULL };
		t = g_type_register_static (GSF_INPUT_TYPE, "RegressFlakyInput", &i, 0);
	}
	return t;
}

/*
 * If refilling from the source failed after inflate had already consumed
 * input, the failed read left the zlib state advanced while the input's
 * position stayed at the start of the read.  A retry at the same position
 * then re-fed the stream and delivered shifted data: silently wrong
 * output.  The failed read must fail, and a retry must deliver the correct
 * data (the documented contract is that the position is unchanged on
 * error).
 */
static void
test_gzip_read_after_source_failure (void)
{
	enum { N = 400000, FIRST = 1000, SPAN = 120000 };
	guint8 *data = g_malloc (N);
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutput *gz = gsf_output_gzip_new (mem, NULL);
	FlakyInput *src;
	GsfInput *in;
	guint8 const *r;
	guint32 x = 2463534242u;
	int i;

	for (i = 0; i < N; i++) {		/* xorshift32: incompressible */
		x ^= x << 13; x ^= x >> 17; x ^= x << 5;
		data[i] = (guint8)x;
	}
	check (gsf_output_write (gz, N, data), "gzip data written");
	check (gsf_output_close (gz), "gzip output closed");
	g_object_unref (gz);

	src = g_object_new (flaky_get_type (), NULL);
	src->data = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
	gsf_input_set_size (GSF_INPUT (src), gsf_output_size (mem));

	in = gsf_input_gzip_new (GSF_INPUT (src), NULL);
	check (in != NULL, "gzip input opens");
	if (in == NULL)
		goto done;

	r = gsf_input_read (in, FIRST, NULL);
	check (r != NULL && memcmp (r, data, FIRST) == 0, "first read is correct");

	flaky_fail_next = TRUE;
	r = gsf_input_read (in, SPAN, NULL);
	check (r == NULL, "read fails when the source fails");
	check (gsf_input_tell (in) == FIRST, "position is unchanged after the failure");

	r = gsf_input_read (in, SPAN, NULL);
	check (r != NULL && memcmp (r, data + FIRST, SPAN) == 0,
	       "retry at the same position delivers the correct data");

	r = gsf_input_read (in, N - FIRST - SPAN, NULL);
	check (r != NULL && memcmp (r, data + FIRST + SPAN, N - FIRST - SPAN) == 0,
	       "the rest of the stream is correct");

	g_object_unref (in);
done:
	g_object_unref (src);
	g_object_unref (mem);
	g_free (data);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	if (probe_bzip2 ()) {
		test_uncompress_bzip2 ();
		test_truncated_bzip2 ();
		test_bzip2_roundtrip ();
		test_bzip2_large_roundtrip ();
		test_bzip2_empty ();
		test_bzip2_sink_failure_sets_error ();
	}
	test_gzip_roundtrip ();
	test_gzip_large_roundtrip ();
	test_uncompress_gzip ();
	test_uncompress_passthrough ();
	test_gzip_dup ();
	test_gzip_read_after_source_failure ();

	gsf_shutdown ();
	return regress_exit_status ();
}
