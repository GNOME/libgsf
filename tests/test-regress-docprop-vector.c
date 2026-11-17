/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-docprop-vector.c: tests for gsf-docprop-vector.c
 */

#include "libgsf-regress.h"

#include <gsf/gsf-docprop-vector.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * A fresh vector is empty: no elements, and as_string returns an empty
 * string (or at least something with no content).
 */
static void
test_empty (void)
{
	GsfDocPropVector *vec = gsf_docprop_vector_new ();
	GValue wrapper = G_VALUE_INIT;
	GArray *ga;
	char *str;

	check (vec != NULL, "vector created");
	if (vec == NULL)
		return;

	g_value_init (&wrapper, GSF_DOCPROP_VECTOR_TYPE);
	g_value_set_object (&wrapper, vec);
	ga = gsf_value_get_docprop_array (&wrapper);
	check (ga != NULL && ga->len == 0, "fresh vector has 0 elements");
	str = gsf_docprop_vector_as_string (vec);
	check (str != NULL, "as_string returns a string");
	check (str != NULL && strlen (str) == 0,
	       "as_string on an empty vector is empty");
	g_free (str);
	g_value_unset (&wrapper);
	g_object_unref (vec);
}

/*
 * Appending values: each append increases the array length and the
 * values are stored in order.  The two parallel containers (GArray and
 * GValueArray) must stay in sync.
 */
static void
test_append (void)
{
	GsfDocPropVector *vec = gsf_docprop_vector_new ();
	GValue sv = G_VALUE_INIT;
	GValue wrapper = G_VALUE_INIT;
	GArray *ga;
	char *str;

	check (vec != NULL, "vector created");
	if (vec == NULL)
		return;

	g_value_init (&sv, G_TYPE_STRING);
	g_value_set_string (&sv, "alpha");
	gsf_docprop_vector_append (vec, &sv);
	g_value_set_string (&sv, "beta");
	gsf_docprop_vector_append (vec, &sv);
	g_value_set_string (&sv, "gamma");
	gsf_docprop_vector_append (vec, &sv);
	g_value_unset (&sv);

	g_value_init (&wrapper, GSF_DOCPROP_VECTOR_TYPE);
	g_value_set_object (&wrapper, vec);

	ga = gsf_value_get_docprop_array (&wrapper);
	check (ga != NULL && ga->len == 3, "array has 3 elements after 3 appends");
	check (ga != NULL &&
	       0 == strcmp (g_value_get_string (&g_array_index (ga, GValue, 0)),
			    "alpha"),
	       "element 0 is 'alpha'");
	check (ga != NULL &&
	       0 == strcmp (g_value_get_string (&g_array_index (ga, GValue, 1)),
			    "beta"),
	       "element 1 is 'beta'");
	check (ga != NULL &&
	       0 == strcmp (g_value_get_string (&g_array_index (ga, GValue, 2)),
			    "gamma"),
	       "element 2 is 'gamma'");

	{
		GValueArray *gva;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gva = gsf_value_get_docprop_varray (&wrapper);
		check (gva != NULL && gva->n_values == 3,
		       "the legacy GValueArray has 3 elements too");
G_GNUC_END_IGNORE_DEPRECATIONS
	}

	str = gsf_docprop_vector_as_string (vec);
	/* g_strdup_value_contents wraps strings in double quotes.  */
	check (str != NULL && strstr (str, "alpha") != NULL,
	       "as_string contains 'alpha'");
	check (str != NULL && strstr (str, "beta") != NULL,
	       "as_string contains 'beta'");
	check (str != NULL && strstr (str, "gamma") != NULL,
	       "as_string contains 'gamma'");
	g_free (str);

	g_value_unset (&wrapper);
	g_object_unref (vec);
}

/*
 * Appending values of different types (int, double, string) must all
 * work and be rendered by as_string.
 */
static void
test_mixed_types (void)
{
	GsfDocPropVector *vec = gsf_docprop_vector_new ();
	GValue iv = G_VALUE_INIT, dv = G_VALUE_INIT, sv = G_VALUE_INIT;
	GValue wrapper = G_VALUE_INIT;
	char *str;

	check (vec != NULL, "vector created");
	if (vec == NULL)
		return;

	g_value_init (&iv, G_TYPE_INT);
	g_value_set_int (&iv, 42);
	gsf_docprop_vector_append (vec, &iv);

	g_value_init (&dv, G_TYPE_DOUBLE);
	g_value_set_double (&dv, 3.14);
	gsf_docprop_vector_append (vec, &dv);

	g_value_init (&sv, G_TYPE_STRING);
	g_value_set_string (&sv, "text");
	gsf_docprop_vector_append (vec, &sv);

	g_value_init (&wrapper, GSF_DOCPROP_VECTOR_TYPE);
	g_value_set_object (&wrapper, vec);

	{
		GArray *ga = gsf_value_get_docprop_array (&wrapper);
		check (ga != NULL && ga->len == 3, "3 mixed-type elements");
	}

	str = gsf_docprop_vector_as_string (vec);
	check (str != NULL && strstr (str, "42") != NULL,
	       "as_string contains '42'");
	check (str != NULL && strstr (str, "3.14") != NULL,
	       "as_string contains '3.14'");
	check (str != NULL && strstr (str, "text") != NULL,
	       "as_string contains 'text'");
	g_free (str);

	g_value_unset (&iv);
	g_value_unset (&dv);
	g_value_unset (&sv);
	g_value_unset (&wrapper);
	g_object_unref (vec);
}

/*
 * Appending an unset GValue (no type) should be a no-op, not a crash.
 */
static void
test_append_unset (void)
{
	GsfDocPropVector *vec = gsf_docprop_vector_new ();
	GValue unset_val = G_VALUE_INIT;
	GValue wrapper = G_VALUE_INIT;
	GArray *ga;

	check (vec != NULL, "vector created");
	if (vec == NULL)
		return;

	gsf_docprop_vector_append (vec, &unset_val);

	g_value_init (&wrapper, GSF_DOCPROP_VECTOR_TYPE);
	g_value_set_object (&wrapper, vec);
	ga = gsf_value_get_docprop_array (&wrapper);
	check (ga != NULL && ga->len == 0,
	       "appending an unset value is a no-op");

	g_value_unset (&wrapper);
	g_object_unref (vec);
}

/*
 * The boxed type: a vector stored in a GValue can be copied and read
 * back through g_value_get_object.
 */
static void
test_boxed_roundtrip (void)
{
	GsfDocPropVector *vec = gsf_docprop_vector_new ();
	GValue sv = G_VALUE_INIT;
	GValue wrapper = G_VALUE_INIT;
	GValue copy = G_VALUE_INIT;
	GsfDocPropVector *got;
	GArray *ga;

	check (vec != NULL, "vector created");
	if (vec == NULL)
		return;

	g_value_init (&sv, G_TYPE_STRING);
	g_value_set_string (&sv, "boxed");
	gsf_docprop_vector_append (vec, &sv);
	g_value_unset (&sv);

	g_value_init (&wrapper, GSF_DOCPROP_VECTOR_TYPE);
	g_value_set_object (&wrapper, vec);

	g_value_init (&copy, GSF_DOCPROP_VECTOR_TYPE);
	g_value_copy (&wrapper, &copy);
	got = g_value_get_object (&copy);
	/* g_value_copy on an object type just refs the same instance.  */
	check (got != NULL, "the copy holds a vector");
	check (got != NULL && got == vec, "the copy is the same object");

	ga = gsf_value_get_docprop_array (&copy);
	check (ga != NULL && ga->len == 1, "the copy has 1 element");
	check (ga != NULL &&
	       0 == strcmp (g_value_get_string (&g_array_index (ga, GValue, 0)),
			    "boxed"),
	       "the copy has the right content");

	g_value_unset (&copy);
	g_value_unset (&wrapper);
	g_object_unref (vec);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_empty ();
	test_append ();
	test_mixed_types ();
	test_append_unset ();
	test_boxed_roundtrip ();

	gsf_shutdown ();
	return regress_exit_status ();
}
