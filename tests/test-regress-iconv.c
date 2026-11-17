/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-iconv.c: regression tests for gsf-output-iconv.c
 */

#include <gsf/gsf.h>
#include <gsf/gsf-output-iconv.h>
#include <gsf/gsf-output-memory.h>

#include "libgsf-regress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Build a GsfOutputIconv over @sink converting from @src to @dst.
 * A non-NULL @fallback sets the "fallback" property.
 */
static GsfOutput *
make_iconv (GsfOutput *sink, char const *dst, char const *src,
	    char const *fallback)
{
	if (fallback != NULL)
		return g_object_new (GSF_OUTPUT_ICONV_TYPE,
				     "sink", sink,
				     "input-charset", src,
				     "output-charset", dst,
				     "fallback", fallback,
				     NULL);
	return gsf_output_iconv_new (sink, dst, src);
}

/*
 * Sanity: a plain short write of 100 ASCII bytes must convert to 200
 * UTF-16LE bytes with no error.
 */
static void
test_short_write (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutput *ic = make_iconv (mem, "UTF-16LE", "UTF-8", NULL);
	guint8 as[100];
	guint8 const *out;

	memset (as, 'x', sizeof as);
	check (ic != NULL, "iconv output created");
	if (ic == NULL) {
		g_object_unref (mem);
		return;
	}

	check (gsf_output_write (ic, sizeof as, as), "short write ok");
	check (gsf_output_error (ic) == NULL, "no error after short write");
	check (gsf_output_close (ic), "close succeeds");
	out = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
	check (gsf_output_size (mem) == 200, "100 bytes became 200 UTF-16 bytes");
	check (out != NULL && out[0] == 'x' && out[1] == 0 &&
	       out[198] == 'x' && out[199] == 0,
	       "content is UTF-16LE encoded");

	g_object_unref (ic);
	g_object_unref (mem);
}

/*
 * Basic conversion: "Aé" written as UTF-8 (41 C3 A9) must come out of the
 * sink as UTF-16LE (41 00 E9 00).  This also pins the constructor's
 * (dst, src) argument order.
 */
static void
test_basic_conversion (void)
{
	static guint8 const in[]  = { 0x41, 0xC3, 0xA9 };	/* "Aé" */
	static guint8 const out[] = { 0x41, 0x00, 0xE9, 0x00 };	/* UTF-16LE */
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutput *ic = make_iconv (mem, "UTF-16LE", "UTF-8", NULL);
	guint8 const *bytes;

	check (ic != NULL, "iconv output created");
	if (ic == NULL) {
		g_object_unref (mem);
		return;
	}

	check (gsf_output_write (ic, sizeof in, in), "write ok");
	check (gsf_output_close (ic), "close flushes the buffer");
	bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
	check (gsf_output_size (mem) == sizeof out, "converted size is 4");
	check (bytes != NULL && memcmp (bytes, out, sizeof out) == 0,
	       "converted bytes match UTF-16LE");

	g_object_unref (ic);
	g_object_unref (mem);
}

/*
 * A multi-byte character may arrive split across several write calls: the
 * pending partial sequence must be carried over until it is complete.
 * The result must be identical to writing the whole string at once.
 */
static void
test_split_multibyte (void)
{
	static guint8 const a = 'a';
	static guint8 const lead[] = { 0xC3 };
	static guint8 const cont[] = { 0xA9 };
	static guint8 const out[] = { 0x61, 0x00, 0xE9, 0x00 };
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutput *ic = make_iconv (mem, "UTF-16LE", "UTF-8", NULL);
	guint8 const *bytes;

	check (ic != NULL, "iconv output created");
	if (ic == NULL) {
		g_object_unref (mem);
		return;
	}

	check (gsf_output_write (ic, 1, &a), "write 'a'");
	check (gsf_output_write (ic, sizeof lead, lead), "write lead byte");
	check (gsf_output_write (ic, sizeof cont, cont), "write continuation byte");
	check (gsf_output_error (ic) == NULL, "no error while splitting");
	check (gsf_output_close (ic), "close flushes the buffer");
	bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
	check (gsf_output_size (mem) == sizeof out, "converted size is 4");
	check (bytes != NULL && memcmp (bytes, out, sizeof out) == 0,
	       "split character converted correctly");

	g_object_unref (ic);
	g_object_unref (mem);
}

/*
 * The 1024-byte flush boundary: a multi-byte character that straddles the
 * boundary must be carried over to the next flush rather than aborting the
 * conversion.  g_convert_with_fallback consumes only the complete prefix
 * and reports the consumed length, which the carry logic in iconv_flush()
 * handles correctly on glib 2.84 -- this test guards that behaviour.
 */
static void
test_boundary_carry (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutput *ic = make_iconv (mem, "UTF-16LE", "UTF-8", NULL);
	guint8 *buf;

	check (ic != NULL, "iconv output created");
	if (ic == NULL) {
		g_object_unref (mem);
		return;
	}

	/* The FIRST write fills the buffer exactly, so that the flush
	 * boundary (1024) splits a multi-byte character.  1023 'a' plus the
	 * first byte of a two-byte UTF-8 character (U+00E9 = 0xC3 0xA9). */
	buf = g_new (guint8, 1024);
	memset (buf, 'a', 1023);
	buf[1023] = 0xC3;
	check (gsf_output_write (ic, 1024, buf),
	       "write ending inside a multi-byte character");
	check (gsf_output_error (ic) == NULL,
	       "no error while the sequence is still incomplete");

	/* the second half of the character, plus one more character */
	{
		guint8 b2[2] = { 0xA9, 0x62 };
		check (gsf_output_write (ic, sizeof b2, b2),
		       "continuation write completes the character");
		check (gsf_output_error (ic) == NULL,
		       "stream still usable after boundary flush");
	}

	check (gsf_output_close (ic), "close succeeds");
	check (gsf_output_size (mem) == 2 * 1025,
	       "all 1025 input bytes converted");

	g_object_unref (ic);
	g_object_unref (mem);
	g_free (buf);
}

/*
 * A single-byte target charset: "abé" written as UTF-8 must come out of
 * the sink as 61 62 E9 in ISO-8859-1.
 */
static void
test_latin1_target (void)
{
	static guint8 const in[]  = { 0x61, 0x62, 0xC3, 0xA9 };	/* "abé" */
	static guint8 const out[] = { 0x61, 0x62, 0xE9 };	/* latin-1 */
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutput *ic = make_iconv (mem, "ISO-8859-1", "UTF-8", NULL);
	guint8 const *bytes;

	check (ic != NULL, "iconv output created");
	if (ic == NULL) {
		g_object_unref (mem);
		return;
	}

	check (gsf_output_write (ic, sizeof in, in), "write ok");
	check (gsf_output_close (ic), "close succeeds");
	bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
	check (gsf_output_size (mem) == sizeof out, "converted size is 3");
	check (bytes != NULL && memcmp (bytes, out, sizeof out) == 0,
	       "latin-1 bytes match");

	g_object_unref (ic);
	g_object_unref (mem);
}

/*
 * A character that cannot be represented in the target charset is
 * converted to a Unicode escape (\uXXXX) when the fallback property is
 * NULL -- U+0100 must come out as the 6 ASCII bytes "\u0100".
 */
static void
test_unicode_escape_fallback (void)
{
	static guint8 const in[] = { 0xC4, 0x80 };		/* U+0100 */
	static char const out[] = "\\u0100";
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutput *ic = make_iconv (mem, "ISO-8859-1", "UTF-8", NULL);
	guint8 const *bytes;

	check (ic != NULL, "iconv output created");
	if (ic == NULL) {
		g_object_unref (mem);
		return;
	}

	check (gsf_output_write (ic, sizeof in, in), "write ok");
	check (gsf_output_close (ic), "close succeeds");
	check (gsf_output_error (ic) == NULL, "no error for unrepresentable char");
	bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
	check (gsf_output_size (mem) == (gsf_off_t)(strlen (out)),
	       "escape is 6 bytes long");
	check (bytes != NULL && memcmp (bytes, out, strlen (out)) == 0,
	       "unrepresentable char became \\u0100");

	g_object_unref (ic);
	g_object_unref (mem);
}

/*
 * With an explicit fallback string, an unrepresentable character is
 * replaced by that string: U+0100 must come out as a single '?' byte.
 */
static void
test_fallback_string (void)
{
	static guint8 const in[] = { 0xC4, 0x80 };		/* U+0100 */
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutput *ic = make_iconv (mem, "ISO-8859-1", "UTF-8", "?");
	guint8 const *bytes;

	check (ic != NULL, "iconv output created");
	if (ic == NULL) {
		g_object_unref (mem);
		return;
	}

	check (gsf_output_write (ic, sizeof in, in), "write ok");
	check (gsf_output_close (ic), "close succeeds");
	check (gsf_output_error (ic) == NULL, "no error with a fallback set");
	bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
	check (gsf_output_size (mem) == 1, "fallback is one byte");
	check (bytes != NULL && bytes[0] == '?', "unrepresentable char became '?'");

	g_object_unref (ic);
	g_object_unref (mem);
}

/*
 * GsfOutputIconv cannot seek: gsf_output_seek must fail.  Note that a
 * "null seek" to the current position is short-circuited by the base
 * class before the Seek vfunc is reached, so the first real seek must
 * target a different position.
 */
static void
test_seek_fails (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutput *ic = make_iconv (mem, "UTF-16LE", "UTF-8", NULL);

	check (ic != NULL, "iconv output created");
	if (ic == NULL) {
		g_object_unref (mem);
		return;
	}

	check (!gsf_output_seek (ic, 1, G_SEEK_SET), "seek fails");
	check (!gsf_output_seek (ic, 10, G_SEEK_CUR), "seek fails relative too");

	gsf_output_close (ic);
	g_object_unref (ic);
	g_object_unref (mem);
}

/*
 * A zero-length write is a no-op that succeeds, and closing without any
 * content produces an empty sink.
 */
static void
test_empty_write (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutput *ic = make_iconv (mem, "UTF-16LE", "UTF-8", NULL);

	check (ic != NULL, "iconv output created");
	if (ic == NULL) {
		g_object_unref (mem);
		return;
	}

	check (gsf_output_write (ic, 0, NULL), "zero-length write succeeds");
	check (gsf_output_error (ic) == NULL, "no error after empty write");
	check (gsf_output_close (ic), "close succeeds");
	check (gsf_output_size (mem) == 0, "sink is empty");

	g_object_unref (ic);
	g_object_unref (mem);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_short_write ();
	test_basic_conversion ();
	test_split_multibyte ();
	test_boundary_carry ();
	test_latin1_target ();
	test_unicode_escape_fallback ();
	test_fallback_string ();
	test_seek_fails ();
	test_empty_write ();

	gsf_shutdown ();
	return regress_exit_status ();
}
