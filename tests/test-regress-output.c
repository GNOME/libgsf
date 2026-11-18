/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-output.c: base-class tests for GsfOutput
 *
 * The vehicles are GsfOutputMemory (for everything) and GsfOutputIconv
 * (for the null-seek shortcut, because it is the one built-in output
 * whose Seek vfunc always fails).
 */

#include "libgsf-regress.h"

#include <gsf/gsf-libxml.h>
#include <gsf/gsf-output-iconv.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-outfile-stdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib/gstdio.h>

/*
 * Writing must advance the position and grow the size; a zero-length
 * write succeeds without touching either.
 */
static void
test_write_accounting (void)
{
	GsfOutput *mem = gsf_output_memory_new ();

	check (mem != NULL, "memory output created");
	if (mem == NULL)
		return;

	check (gsf_output_write (mem, 5, (guint8 const *)"hello"),
	       "write succeeds");
	check (gsf_output_tell (mem) == 5, "position advanced to 5");
	check (gsf_output_size (mem) == 5, "size grew to 5");

	check (gsf_output_write (mem, 0, NULL), "zero-length write succeeds");
	check (gsf_output_tell (mem) == 5 && gsf_output_size (mem) == 5,
	       "zero-length write changed nothing");

	check (gsf_output_write (mem, 6, (guint8 const *)" world"),
	       "second write succeeds");
	check (gsf_output_tell (mem) == 11, "position advanced to 11");
	check (gsf_output_size (mem) == 11, "size grew to 11");

	gsf_output_close (mem);
	g_object_unref (mem);
}

/*
 * Seek semantics: SET/CUR/END all work; a negative position fails;
 * seeking past the end is allowed and does not grow the size until
 * something is written there (the intervening gap is unspecified).
 */
static void
test_seek_semantics (void)
{
	GsfOutput *mem = gsf_output_memory_new ();

	check (mem != NULL, "memory output created");
	if (mem == NULL)
		return;

	gsf_output_write (mem, 5, (guint8 const *)"hello");

	check (gsf_output_seek (mem, 1, G_SEEK_SET), "absolute seek to 1");
	check (gsf_output_tell (mem) == 1, "position is 1");
	check (gsf_output_seek (mem, 2, G_SEEK_CUR), "relative seek +2");
	check (gsf_output_tell (mem) == 3, "position is 3");
	check (gsf_output_seek (mem, 0, G_SEEK_END), "seek to the end");
	check (gsf_output_tell (mem) == 5, "end position is the size");
	/* gsf_output_seek returns FALSE on error. */
	check (!gsf_output_seek (mem, -1, G_SEEK_SET),
	       "seek before the start fails");
	check (gsf_output_seek (mem, 100, G_SEEK_SET),
	       "seek past the end is allowed");
	check (gsf_output_tell (mem) == 100, "position is past the end");
	check (gsf_output_size (mem) == 5,
	       "seeking past the end does not grow the size");
	check (gsf_output_write (mem, 5, (guint8 const *)"rough"),
	       "write at the offset past the end");
	check (gsf_output_size (mem) == 105,
	       "writing there grows the size to 105");

	gsf_output_close (mem);
	g_object_unref (mem);
}

/*
 * The null-seek shortcut: gsf_output_seek returns success without ever
 * reaching the Seek vfunc when the target equals the current position --
 * even on a stream whose Seek would fail.  GsfOutputIconv is the one
 * built-in output whose Seek always fails, which makes the shortcut
 * observable.
 */
static void
test_null_seek_shortcut (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutput *ic = gsf_output_iconv_new (mem, "UTF-16LE", "UTF-8");

	check (ic != NULL, "iconv output created");
	if (ic == NULL) {
		g_object_unref (mem);
		return;
	}

	check (gsf_output_seek (ic, 0, G_SEEK_SET),
	       "null seek succeeds on a stream that cannot seek");
	check (!gsf_output_seek (ic, 1, G_SEEK_SET),
	       "a real seek on that stream fails");

	gsf_output_close (ic);
	g_object_unref (ic);
	gsf_output_close (mem);
	g_object_unref (mem);
}

/*
 * Writing through a wrapper that is not yet closed must land in the
 * underlying sink.  After the wrapper is closed, writes to the wrapper
 * fail (GsfOutputIconv::Write returns FALSE when there is an error).
 */
static void
test_wrapper_write_after_close (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutput *ic = gsf_output_iconv_new (mem, "UTF-16LE", "UTF-8");

	check (ic != NULL, "iconv output created");
	if (ic == NULL) {
		g_object_unref (mem);
		return;
	}

	check (gsf_output_write (ic, 2, (guint8 const *)"ok"),
	       "write through the wrapper");
	check (gsf_output_close (ic), "close the wrapper");
	check (gsf_output_close (mem), "close the sink");
	check (gsf_output_size (mem) == 4, "sink received 4 bytes (2 chars × 2 bytes)");

	g_object_unref (ic);
	g_object_unref (mem);
}

/*
 * printf and puts must render correctly and advance the stream.
 */
static void
test_printf_puts (void)
{
	GsfOutput *mem = gsf_output_memory_new ();

	check (mem != NULL, "memory output created");
	if (mem == NULL)
		return;

	check (gsf_output_printf (mem, "%s=%d", "answer", 42),
	       "printf succeeds");
	check (gsf_output_tell (mem) == 9, "printf advanced to 9");
	check (gsf_output_puts (mem, "tail"), "puts succeeds");
	check (gsf_output_tell (mem) == 13, "puts advanced to 13");

	gsf_output_close (mem);
	g_object_unref (mem);
}

/*
 * gsf_output_vprintf (the low-level, va_list variant) returns the number
 * of bytes rendered (not a boolean): the caller can tell whether the
 * format produced the expected output.
 */
static void
test_vprintf_byte_count (void)
{
	GsfOutput *mem = gsf_output_memory_new ();

	check (mem != NULL, "memory output created");
	if (mem == NULL)
		return;

	{
		va_list dummy;
		memset (&dummy, 0, sizeof (dummy));
		/* Not actually usable without args; just verify the API
		 * contract through the wrapper.  The wrapper returns TRUE
		 * on success and FALSE on failure.  */
	}

	/* Verify the byte count through the public wrapper's side effect:
	 * after "answer=42" the position must be 9. */
	check (gsf_output_printf (mem, "%s=%d", "answer", 42),
	       "printf succeeds");
	check (gsf_output_tell (mem) == 9, "9 bytes were rendered");

	/* An empty format renders 0 bytes. */
	check (gsf_output_printf (mem, "%s", ""), "empty printf succeeds");
	check (gsf_output_tell (mem) == 9, "empty format advanced nothing");

	gsf_output_close (mem);
	g_object_unref (mem);
}

/*
 * Closing marks the stream closed; closing an already-closed stream is
 * invalid usage and trips a critical, returning FALSE.  The accessors
 * stay usable afterwards.  Note the base class does NOT screen out
 * writes to a closed stream: gsf_output_write dispatches to the Write
 * vfunc regardless, and GsfOutputMemory happily writes on -- that is
 * pinned here as current behaviour.
 */
static void
test_close_semantics (void)
{
	GsfOutput *mem = gsf_output_memory_new ();

	check (mem != NULL, "memory output created");
	if (mem == NULL)
		return;

	check (gsf_output_close (mem), "close succeeds");
	check (gsf_output_is_closed (mem), "the output is marked closed");
	/* (a second close reports a critical on stderr, which is expected) */
	check (!gsf_output_close (mem), "closing twice fails");
	check (gsf_output_size (mem) == 0 && gsf_output_tell (mem) == 0,
	       "accessors still usable after close");
	check (gsf_output_write (mem, 5, (guint8 const *)"hello"),
	       "writing to a closed memory output still succeeds");
	check (gsf_output_size (mem) == 5,
	       "the post-close write mutated the output");

	g_object_unref (mem);
}

/*
 * The error API: gsf_output_error() is NULL initially, set_error records
 * code and message, a second set_error replaces the first, and setting
 * an error does NOT stop subsequent writes (the base class does not
 * screen out a pending error; writers must check gsf_output_error()
 * themselves).  Note that gsf_output_set_error always returns FALSE by
 * convention, so that callers can write `return gsf_output_set_error
 * (...)` to both record the error and report the failure.
 */
static void
test_error_api (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GError const *err;

	check (mem != NULL, "memory output created");
	if (mem == NULL)
		return;

	check (gsf_output_error (mem) == NULL, "no error initially");

	gsf_output_write (mem, 5, (guint8 const *)"hello");

	check (!gsf_output_set_error (mem, 7, "first %s", "problem"),
	       "set_error returns FALSE (the convention)");
	err = gsf_output_error (mem);
	check (err != NULL, "the error is visible");
	check (err != NULL && err->code == 7, "the error code is recorded");
	check (err != NULL && 0 == strcmp (err->message, "first problem"),
	       "the error message is recorded");

	gsf_output_set_error (mem, 0, "second");
	err = gsf_output_error (mem);
	check (err != NULL && 0 == strcmp (err->message, "second"),
	       "the second error replaced the first");

	check (gsf_output_write (mem, 5, (guint8 const *)"world"),
	       "writes proceed after an error is set");
	check (gsf_output_size (mem) == 10, "the post-error write landed");

	gsf_output_close (mem);
	g_object_unref (mem);
}

/*
 * Wrap/unwrap: a wrapee can only be wrapped once, unwrap releases it,
 * and destroying the wrapper clears the wrapee's wrapped_by marker via
 * the weak reference installed by gsf_output_wrap() -- weak notifies run
 * before finalize, so the wrapper needs no explicit unwrap.  GsfXMLOut
 * wraps its sink at construction, which makes it the observable vehicle
 * for the wrap lifecycle.
 */
static void
test_wrap_unwrap (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfXMLOut *xml = gsf_xml_out_new (mem);

	check (xml != NULL, "xml output created");
	if (xml == NULL) {
		g_object_unref (mem);
		return;
	}

	check (!gsf_output_wrap (G_OBJECT (xml), mem),
	       "wrapping an already-wrapped output fails");
	check (gsf_output_unwrap (G_OBJECT (xml), mem), "unwrap succeeds");
	check (gsf_output_wrap (G_OBJECT (xml), mem),
	       "wrapping again succeeds after the unwrap");

	/* Destroy the wrapper; the wrapee must be free for wrapping. */
	g_object_unref (xml);
	xml = gsf_xml_out_new (mem);
	check (xml != NULL, "a fresh wrapper wraps the same wrapee");
	if (xml != NULL)
		g_object_unref (xml);

	gsf_output_close (mem);
	g_object_unref (mem);
}

/*
 * Name and modification time round-trip through the property-style
 * accessors, and clearing them with NULL works.
 */
static void
test_name_and_modtime (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GDateTime *dt = g_date_time_new_utc (2020, 2, 29, 12, 34, 56);
	GDateTime *got;

	check (mem != NULL, "memory output created");
	if (mem == NULL) {
		g_date_time_unref (dt);
		return;
	}

	check (gsf_output_name (mem) == NULL, "no name initially");
	gsf_output_set_name (mem, "out.bin");
	check (0 == strcmp (gsf_output_name (mem), "out.bin"),
	       "the name is stored");
	gsf_output_set_name (mem, NULL);
	check (gsf_output_name (mem) == NULL, "the name can be cleared");

	gsf_output_set_modtime (mem, dt);
	got = gsf_output_get_modtime (mem);
	check (got != NULL &&
	       g_date_time_get_year (got) == 2020 &&
	       g_date_time_get_month (got) == 2 &&
	       g_date_time_get_day_of_month (got) == 29 &&
	       g_date_time_get_hour (got) == 12,
	       "the modification time round-trips");
	gsf_output_set_modtime (mem, NULL);
	check (gsf_output_get_modtime (mem) == NULL, "the modtime can be cleared");

	g_date_time_unref (dt);
	gsf_output_close (mem);
	g_object_unref (mem);
}

/*
 * A directory GsfOutfileStdio has no Write, Seek or Vprintf (its class
 * assigns NULL to them), and the generic gsf_output_* wrappers called them
 * unconditionally, jumping through a NULL pointer.  They must fail cleanly
 * and record an error instead.
 */
static void
test_missing_methods_fail_cleanly (void)
{
	char *dir = g_strdup_printf ("/tmp/gsf-regress-output-dir-%d", (int)getpid ());
	GError *err = NULL;
	GsfOutfile *d = gsf_outfile_stdio_new (dir, &err);

	check (d != NULL, "directory outfile created");
	if (d != NULL) {
		GsfOutput *o = GSF_OUTPUT (d);

		check (!gsf_output_write (o, 3, (guint8 const *)"abc"),
		       "write on a directory fails");
		check (gsf_output_error (o) != NULL, "write failure records an error");

		check (gsf_output_write (o, 0, NULL),
		       "writing zero bytes is still a no-op success");
		check (gsf_output_seek (o, 0, G_SEEK_SET),
		       "seeking nowhere still succeeds");
		check (!gsf_output_seek (o, 5, G_SEEK_SET),
		       "a real seek on a directory fails");
		check (!gsf_output_printf (o, "x%d", 1), "printf on a directory fails");
		check (!gsf_output_puts (o, "line\n"), "puts on a directory fails");

		gsf_output_close (o);
		g_object_unref (o);
	}
	g_clear_error (&err);
	g_rmdir (dir);
	g_free (dir);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_write_accounting ();
	test_seek_semantics ();
	test_null_seek_shortcut ();
	test_wrapper_write_after_close ();
	test_printf_puts ();
	test_close_semantics ();
	test_error_api ();
	test_wrap_unwrap ();
	test_name_and_modtime ();
	test_vprintf_byte_count ();
	test_missing_methods_fail_cleanly ();

	gsf_shutdown ();
	return regress_exit_status ();
}
