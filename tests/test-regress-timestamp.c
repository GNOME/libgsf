/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-timestamp.c: regression tests for gsf-timestamp.c
 */

#include <gsf/gsf.h>
#include <gsf/gsf-timestamp.h>

#include "libgsf-regress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * A freshly created (invalid) stamp must render as "<invalid>".  The
 * invalid marker is (guint32)-1 -- exactly what gsf_timestamp_new()
 * stores -- and gsf_timestamp_as_string() must recognize it; previously
 * it converted the marker to a perfectly valid 2106-02-07T06:28:15Z.
 */
static void
test_invalid_marker (void)
{
	GsfTimestamp *stamp = gsf_timestamp_new ();
	char *str = gsf_timestamp_as_string (stamp);

	check (str != NULL && strstr (str, "invalid") != NULL,
	       "invalid stamp renders as <invalid>");
	g_free (str);
	gsf_timestamp_free (stamp);
}

/*
 * The 32-bit timet field round-trips through the string form for the
 * documented range: values in [0, (guint32)-2] survive, values outside it
 * wrap.  (Widening the field would break the GsfTimestamp ABI, so
 * pre-epoch times are a documented limitation rather than something the
 * test can demand a fix for.)  The extremes of the representable range
 * must round-trip exactly through the string form, including a re-parse.
 */
static void
test_roundtrip (void)
{
	GsfTimestamp *stamp;
	char *str;

	/* sanity: the epoch is rendered correctly */
	stamp = gsf_timestamp_new ();
	gsf_timestamp_set_time (stamp, 0);
	str = gsf_timestamp_as_string (stamp);
	check (str != NULL && strcmp (str, "1970-01-01T00:00:00Z") == 0,
	       "epoch rendered correctly");
	g_free (str);
	gsf_timestamp_free (stamp);

	/* the last representable second: 2038-01-19T03:14:07Z */
	stamp = gsf_timestamp_new ();
	gsf_timestamp_set_time (stamp, (guint64)G_MAXINT32);
	str = gsf_timestamp_as_string (stamp);
	check (str != NULL && strcmp (str, "2038-01-19T03:14:07Z") == 0,
	       "the last representable second round-trips");
	{
		GsfTimestamp *stamp2 = gsf_timestamp_new ();
		check (gsf_timestamp_load_from_string (stamp2, str),
		       "the rendered stamp re-parses");
		check (stamp2->timet == stamp->timet,
		       "the parsed stamp matches the original");
		gsf_timestamp_free (stamp2);
	}
	g_free (str);
	gsf_timestamp_free (stamp);
}

/*
 * gsf_timestamp_set_time() must store the value it is given (within the
 * documented 32-bit range), and the field must be readable back.
 */
static void
test_set_get (void)
{
	static guint64 const values[] = { 0, 1, 86400, G_MAXINT32, (guint32)-2 };
	unsigned i;
	gboolean ok = TRUE;

	for (i = 0; i < G_N_ELEMENTS (values); i++) {
		GsfTimestamp *stamp = gsf_timestamp_new ();
		gsf_timestamp_set_time (stamp, values[i]);
		if (stamp->timet != values[i]) {
			printf ("     (value %u came back as %u)\n",
				(unsigned)values[i], stamp->timet);
			ok = FALSE;
		}
		gsf_timestamp_free (stamp);
	}
	check (ok, "set_time stores values in the documented range");
}

/*
 * gsf_timestamp_load_from_string() parses ISO 8601 strings into the
 * timet field and reports failures without corrupting the stamp.
 */
static void
test_load_from_string (void)
{
	GsfTimestamp *stamp = gsf_timestamp_new ();

	check (gsf_timestamp_load_from_string (stamp, "1970-01-01T00:00:00Z"),
	       "the epoch string parses");
	check (stamp->timet == 0, "the epoch parses to 0");

	check (gsf_timestamp_load_from_string (stamp, "2038-01-19T03:14:07Z"),
	       "the maximal string parses");
	check (stamp->timet == G_MAXINT32,
	       "the maximal string parses to G_MAXINT32");

	check (!gsf_timestamp_load_from_string (stamp, "not a timestamp"),
	       "garbage is rejected");
	check (stamp->timet == G_MAXINT32,
	       "a failed parse leaves the stamp unchanged");

	gsf_timestamp_free (stamp);
}

/*
 * gsf_timestamp_copy() must produce an equal stamp, and gsf_timestamp_free
 * releases it without touching the original.
 */
static void
test_copy_free (void)
{
	GsfTimestamp *orig = gsf_timestamp_new ();
	GsfTimestamp *copy;

	gsf_timestamp_set_time (orig, 12345);
	copy = gsf_timestamp_copy (orig);
	check (copy != NULL, "copy works");
	check (copy != NULL && copy->timet == orig->timet,
	       "copy carries the same time");
	gsf_timestamp_free (copy);
	check (orig->timet == 12345, "the original is untouched");

	gsf_timestamp_free (orig);
}

/*
 * gsf_timestamp_equal() and gsf_timestamp_hash() must agree: equal stamps
 * hash equally, and stamps with different times are not equal.
 */
static void
test_equal_hash (void)
{
	GsfTimestamp *a = gsf_timestamp_new ();
	GsfTimestamp *b = gsf_timestamp_new ();

	gsf_timestamp_set_time (a, 4711);
	gsf_timestamp_set_time (b, 4711);
	check (gsf_timestamp_equal (a, b), "equal stamps are equal");
	check (gsf_timestamp_hash (a) == gsf_timestamp_hash (b),
	       "equal stamps hash equally");

	gsf_timestamp_set_time (b, 4712);
	check (!gsf_timestamp_equal (a, b), "different stamps differ");

	gsf_timestamp_free (a);
	gsf_timestamp_free (b);
}

/*
 * The GValue integration: a timestamp can be stored in a GValue of
 * GSF_TIMESTAMP_TYPE and read back, and the registered transform to
 * G_TYPE_STRING produces the same rendering as gsf_timestamp_as_string().
 */
static void
test_boxed_value (void)
{
	GsfTimestamp *stamp = gsf_timestamp_new ();
	GValue v = G_VALUE_INIT;
	GValue sv = G_VALUE_INIT;
	GsfTimestamp const *got;
	char *str;

	gsf_timestamp_set_time (stamp, 1000000000); /* 2001-09-09T01:46:40Z */

	g_value_init (&v, GSF_TIMESTAMP_TYPE);
	gsf_timestamp_to_value (stamp, &v);
	check (VAL_IS_GSF_TIMESTAMP (&v), "the value holds a timestamp");
	got = g_value_get_boxed (&v);
	check (got != NULL && got->timet == 1000000000,
	       "the boxed value round-trips");

	g_value_init (&sv, G_TYPE_STRING);
	check (g_value_transform (&v, &sv), "the transform to string works");
	str = gsf_timestamp_as_string (stamp);
	check (g_value_get_string (&sv) != NULL &&
	       0 == strcmp (g_value_get_string (&sv), str),
	       "the transformed string matches as_string()");
	g_free (str);

	g_value_unset (&sv);
	g_value_unset (&v);
	gsf_timestamp_free (stamp);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_invalid_marker ();
	test_roundtrip ();
	test_set_get ();
	test_load_from_string ();
	test_copy_free ();
	test_equal_hash ();
	test_boxed_value ();

	gsf_shutdown ();
	return regress_exit_status ();
}
