/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-infile-leak.c: exercises for gsf-infile.c
 * (meant to be run under valgrind; see t8050-valgrind-regress.pl)
 */

#include <gsf/gsf.h>
#include <gsf/gsf-infile-tar.h>
#include <gsf/gsf-input-memory.h>

#include "libgsf-regress.h"

#include <stdio.h>
#include <string.h>

/*
 * gsf_infile_child_by_vaname() with a multi-level path whose intermediate
 * component is a plain file (not a GsfInfile) leaked the reference taken
 * for that component: the g_return_val_if_fail returned NULL without
 * unreffing it.
 */
static void
test_lookup_leak (void)
{
	GsfInput *inf_in = regress_make_tar_input ();
	GsfInfile *inf = GSF_INFILE (inf_in);
	GsfInput *child = gsf_infile_child_by_vname (inf, "aaa", "nope", NULL);

	check (child == NULL, "path through a plain file fails");
	g_object_unref (inf_in);
}

/*
 * With an empty name list, gsf_infile_child_by_vaname() returned the
 * infile itself without taking a reference, although the API is
 * documented as transfer-full; unref-ing the result therefore consumed
 * the caller's own reference, and the subsequent use of the infile was a
 * use-after-free.  After the fix the result carries a proper reference,
 * so the infile must still be alive after it is unreffed.
 */
static void
test_borrowed_reference (void)
{
	GsfInput *inf_in = regress_make_tar_input ();
	GsfInfile *inf = GSF_INFILE (inf_in);
	gpointer alive_marker = inf_in;
	GsfInput *borrowed;

	g_object_add_weak_pointer (G_OBJECT (inf_in), &alive_marker);
	borrowed = gsf_infile_child_by_vname (inf, NULL);
	check (borrowed == inf_in, "empty path returns the infile");
	g_object_unref (borrowed);
	check (alive_marker != NULL, "infile still alive after unref");
	g_object_remove_weak_pointer (G_OBJECT (inf_in), &alive_marker);

	g_object_unref (inf_in);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_lookup_leak ();
	test_borrowed_reference ();

	gsf_shutdown ();
	return 0;
}
