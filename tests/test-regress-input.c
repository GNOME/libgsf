/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-input.c: base-class tests for GsfInput
 *
 * The vehicle is GsfInputMemory.  Note that gsf_input_seek returns TRUE
 * on ERROR (the opposite convention of gsf_output_seek, which returns
 * TRUE on success) -- the tests pin that asymmetry explicitly.
 */

#include "libgsf-regress.h"

#include <gsf/gsf-input-memory.h>

#include <stdio.h>
#include <stdlib.h>

/*
 * Reading: a read advances the position; a zero-length read returns NULL;
 * a read past the end returns NULL and leaves the position unchanged.
 */
static void
test_read_semantics (void)
{
	GsfInput *in = regress_make_named_memory ("hello world", 11, "in");
	guint8 buf[16];
	guint8 const *r;

	check (in != NULL, "memory input created");
	if (in == NULL)
		return;

	r = gsf_input_read (in, 5, buf);
	check (r != NULL && memcmp (r, "hello", 5) == 0, "read succeeds");
	check (gsf_input_tell (in) == 5, "position advanced to 5");

	check (gsf_input_read (in, 0, buf) == NULL,
	       "a zero-length read returns NULL");
	check (gsf_input_tell (in) == 5,
	       "the zero-length read changed nothing");

	r = gsf_input_read (in, 6, buf);
	check (r != NULL && memcmp (r, " world", 6) == 0,
	       "the rest is readable");
	check (gsf_input_tell (in) == 11, "position advanced to the end");

	r = gsf_input_read (in, 1, buf);
	check (r == NULL, "reading at eof fails");
	check (gsf_input_tell (in) == 11,
	       "a failed read leaves the position unchanged");

	g_object_unref (in);
}

/*
 * Seek semantics: SET/CUR/END work (and return FALSE, since
 * gsf_input_seek returns TRUE on error); positions before the start or
 * after the end fail and leave the position unchanged.
 */
static void
test_seek_semantics (void)
{
	GsfInput *in = regress_make_named_memory ("0123456789", 10, "in");
	guint8 buf[4];
	guint8 const *r;

	check (in != NULL, "memory input created");
	if (in == NULL)
		return;

	check (!gsf_input_seek (in, 4, G_SEEK_SET), "absolute seek to 4");
	check (gsf_input_tell (in) == 4, "position is 4");
	check (!gsf_input_seek (in, -2, G_SEEK_CUR), "relative seek -2");
	check (gsf_input_tell (in) == 2, "position is 2");
	check (!gsf_input_seek (in, 0, G_SEEK_END), "seek to the end");
	check (gsf_input_tell (in) == 10, "end position is the size");
	check (gsf_input_seek (in, -1, G_SEEK_SET),
	       "seek before the start fails");
	check (gsf_input_tell (in) == 10,
	       "the failed seek left the position unchanged");
	check (gsf_input_seek (in, 11, G_SEEK_SET),
	       "seek past the end fails");
	r = gsf_input_read (in, 2, buf);
	check (r == NULL, "no reads after the failed seek");
	check (!gsf_input_seek (in, 5, G_SEEK_SET),
	       "seeking back works after failures");

	g_object_unref (in);
}

/*
 * remaining() decreases with reads, and eof() turns on exactly when the
 * position reaches the size (via a read or a seek).
 */
static void
test_eof_and_remaining (void)
{
	GsfInput *in = regress_make_named_memory ("abc", 3, "in");
	guint8 buf[4];

	check (in != NULL, "memory input created");
	if (in == NULL)
		return;

	check (!gsf_input_eof (in), "not at eof initially");
	check (gsf_input_remaining (in) == 3, "remaining is 3");
	check (gsf_input_read (in, 2, buf) != NULL, "read 2 bytes");
	check (gsf_input_remaining (in) == 1, "remaining is 1");
	check (!gsf_input_eof (in), "not at eof yet");
	check (gsf_input_read (in, 1, buf) != NULL, "read the last byte");
	check (gsf_input_eof (in), "eof after the last byte");
	check (gsf_input_remaining (in) == 0, "remaining is 0");
	check (!gsf_input_seek (in, 0, G_SEEK_SET), "seek back to 0");
	check (!gsf_input_eof (in), "eof cleared by the seek");
	check (gsf_input_remaining (in) == 3, "remaining is 3 again");

	g_object_unref (in);
}

/*
 * Duplication: the duplicate starts at the SAME position as the source,
 * and the two streams are independent afterwards.
 */
static void
test_dup_position (void)
{
	GsfInput *in = regress_make_named_memory ("abcdefgh", 8, "in");
	GsfInput *dup;
	guint8 buf[4];
	guint8 const *r;

	check (in != NULL, "memory input created");
	if (in == NULL)
		return;

	check (!gsf_input_seek (in, 3, G_SEEK_SET), "seek the source to 3");
	dup = gsf_input_dup (in, NULL);
	check (dup != NULL, "dup works");
	if (dup != NULL) {
		check (gsf_input_tell (dup) == 3,
		       "the duplicate starts at the same position");
		r = gsf_input_read (dup, 3, buf);
		check (r != NULL && memcmp (r, "def", 3) == 0,
		       "the duplicate reads from that position");
		check (gsf_input_tell (in) == 3,
		       "reading the duplicate left the source alone");
		r = gsf_input_read (in, 2, buf);
		check (r != NULL && memcmp (r, "de", 2) == 0,
		       "the source continues independently");
		g_object_unref (dup);
	}
	g_object_unref (in);
}

/*
 * The size setter validates its argument (negative rejected) and
 * shrinking limits subsequent reads; the position stays put.  (Growing
 * the size past the real data is NOT exercised here: GsfInputMemory
 * then reads out of bounds -- recorded separately in problems.txt.)
 */
static void
test_setters (void)
{
	GsfInput *in = regress_make_named_memory ("hello world", 11, "in");
	guint8 buf[4];

	check (in != NULL, "memory input created");
	if (in == NULL)
		return;

	check (!gsf_input_set_size (in, (gsf_off_t)-1),
	       "a negative size is rejected");
	check (gsf_input_size (in) == 11, "the size was not changed");

	check (gsf_input_set_size (in, 3), "shrinking the size works");
	check (gsf_input_size (in) == 3, "the size is 3");
	check (gsf_input_read (in, 4, buf) == NULL,
	       "reads beyond the shrunken size fail");
	check (gsf_input_tell (in) == 0, "the position is unchanged");
	check (gsf_input_read (in, 3, buf) != NULL, "reads within it work");

	g_object_unref (in);
}

/*
 * The modification time accessor round-trips a GDateTime and can be
 * cleared.
 */
static void
test_modtime (void)
{
	GsfInput *in = regress_make_named_memory ("x", 1, "in");
	GDateTime *dt = g_date_time_new_utc (1999, 12, 31, 23, 59, 59);
	GDateTime *got;

	check (in != NULL, "memory input created");
	if (in == NULL) {
		g_date_time_unref (dt);
		return;
	}

	check (gsf_input_get_modtime (in) == NULL, "no modtime initially");
	gsf_input_set_modtime (in, dt);
	got = gsf_input_get_modtime (in);
	check (got != NULL &&
	       g_date_time_get_year (got) == 1999 &&
	       g_date_time_get_month (got) == 12 &&
	       g_date_time_get_day_of_month (got) == 31,
	       "the modtime round-trips");
	gsf_input_set_modtime (in, NULL);
	check (gsf_input_get_modtime (in) == NULL, "the modtime can be cleared");

	g_date_time_unref (dt);
	g_object_unref (in);
}

/*
 * Reading from a tar infile root fails cleanly: the root has size 0
 * (the base class rejects the read before the stub Read vfunc is
 * reached), so this exercises the base-class bounds check.
 */
static void
test_stub_read (void)
{
	GsfInput *input = regress_make_tar_input ();
	GsfInfile *tar = gsf_infile_tar_new (input, NULL);
	guint8 buf[4];
	guint8 const *r;

	check (tar != NULL, "tar opened");
	if (tar == NULL) {
		g_object_unref (input);
		return;
	}

	check (gsf_input_size (GSF_INPUT (tar)) == 0,
	       "tar root has size 0");
	r = gsf_input_read (GSF_INPUT (tar), 1, buf);
	check (r == NULL, "reading from the tar root returns NULL");
	check (gsf_input_read (GSF_INPUT (tar), 0, buf) == NULL,
	       "zero-length read from the tar root also returns NULL");

	g_object_unref (tar);
	g_object_unref (input);
}

/*
 * Reading a member through the tar infile: a child read is delegated to
 * the underlying proxy and returns the right content.
 */
static void
test_tar_child_read (void)
{
	GsfInput *input = regress_make_tar_input ();
	GsfInfile *tar = gsf_infile_tar_new (input, NULL);
	GsfInput *child;
	guint8 buf[8];
	guint8 const *r;

	check (tar != NULL, "tar opened");
	if (tar == NULL) {
		g_object_unref (input);
		return;
	}

	child = gsf_infile_child_by_name (tar, "aaa");
	check (child != NULL, "member 'aaa' found");
	if (child != NULL) {
		check (gsf_input_size (child) == 5, "member size is 5");
		r = gsf_input_read (child, 5, buf);
		check (r != NULL && memcmp (r, "hello", 5) == 0,
		       "member content readable");
		g_object_unref (child);
	}

	g_object_unref (tar);
	g_object_unref (input);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_read_semantics ();
	test_seek_semantics ();
	test_eof_and_remaining ();
	test_dup_position ();
	test_setters ();
	test_modtime ();
	test_stub_read ();
	test_tar_child_read ();

	gsf_shutdown ();
	return regress_exit_status ();
}
