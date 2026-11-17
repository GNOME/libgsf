/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-outzip-open.c: exercise for gsf-outfile-zip.c
 * (meant to be run under valgrind; see t8050-valgrind-regress.pl)
 */

#include <gsf/gsf.h>
#include <gsf/gsf-outfile-zip.h>
#include <gsf/gsf-output-memory.h>

/*
 * Closing a zip archive that contains a created-but-never-written member:
 * zip_close_root() dereferenced child->vdir->dirent before the
 * gsf_output_is_closed() guard, so the close dereferenced a NULL dirent
 * and segfaulted.  This program deliberately exercises that path; on a
 * fixed libgsf the close fails cleanly and no memory error is reported.
 */
static void
test_unwritten_member_close (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *zip = gsf_outfile_zip_new (mem, NULL);
	GsfOutput *child = gsf_outfile_new_child (zip, "unwritten", FALSE);

	(void) child;			/* created, but never written nor closed */

	/* child->vdir->dirent is NULL here. */
	gsf_output_close (GSF_OUTPUT (zip));

	g_object_unref (child);
	g_object_unref (zip);
	g_object_unref (mem);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_unwritten_member_close ();

	gsf_shutdown ();
	return 0;
}
