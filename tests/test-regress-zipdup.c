/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-zipdup.c: regression test for gsf-infile-zip.c
 */

#include <gsf/gsf.h>
#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf-outfile-zip.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-input-memory.h>

#include "libgsf-regress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * gsf_input_dup() of a zip member must succeed and carry the member's size
 * and content.  gsf_infile_zip_dup() never set the duplicate's size, so the
 * base class rejected it with "Duplicate size mismatch" for any non-empty
 * member.
 */
static void
test_zip_member_dup (void)
{
	GsfInput *input = regress_make_zip_input ();
	GsfInfile *zip = gsf_infile_zip_new (input, NULL);
	GsfInput *child, *dup;
	GError *err = NULL;

	check (zip != NULL, "zip opened");
	if (zip == NULL) {
		if (err != NULL)
			g_printerr ("zip error: %s\n", err->message);
		g_clear_error (&err);
		g_object_unref (input);
		return;
	}

	child = gsf_infile_child_by_name (zip, "aaa");
	check (child != NULL, "member aaa found");
	if (child == NULL) {
		g_object_unref (zip);
		g_object_unref (input);
		return;
	}

	/* sanity: the member itself reads fine */
	{
		guint8 buf[5];
		guint8 const *r = gsf_input_read (child, 5, buf);
		check (r != NULL && memcmp (r, "hello", 5) == 0,
		       "member content readable");
		gsf_input_seek (GSF_INPUT (child), 0, G_SEEK_SET);
	}

	dup = gsf_input_dup (child, &err);
	check (dup != NULL, "gsf_input_dup of zip member");
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

	g_object_unref (child);
	g_object_unref (zip);
	g_object_unref (input);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_zip_member_dup ();

	gsf_shutdown ();
	return regress_exit_status ();
}
