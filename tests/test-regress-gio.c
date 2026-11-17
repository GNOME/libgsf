/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-gio.c: GsfOutputGio regressions
 */

#include <gsf/gsf.h>
#include <gsf/gsf-output-gio.h>

#include <glib/gstdio.h>

#include "libgsf-regress.h"

#include <stdio.h>
#include <unistd.h>

/*
 * A failure when closing the underlying GOutputStream (flush error, failed
 * rename of the replacement file, ...) used to be discarded: gsf_output_close
 * returned TRUE and recorded no error, although the data was lost.
 *
 * GIO writes a replacement file and renames it over the target when the
 * target exists, so turning the target into a non-empty directory just
 * before the close makes the stream's close fail.
 */
static void
test_close_failure_is_reported (void)
{
	char *dir = g_strdup_printf ("/tmp/gsf-regress-gio-%d", (int)getpid ());
	char *target = g_build_filename (dir, "target", NULL);
	char *inner = g_build_filename (target, "x", NULL);
	GsfOutput *out;
	FILE *f;

	g_assert (g_mkdir_with_parents (dir, 0755) == 0);
	f = g_fopen (target, "w");
	g_assert (f != NULL);
	fputs ("old", f);
	fclose (f);

	out = gsf_output_gio_new_for_path (target, NULL);
	check (out != NULL, "output created");
	if (out != NULL) {
		gboolean closed;

		check (gsf_output_write (out, 5, (guint8 const *)"hello"),
		       "write succeeds");

		g_assert (g_unlink (target) == 0);
		g_assert (g_mkdir (target, 0755) == 0);
		f = g_fopen (inner, "w");
		g_assert (f != NULL);
		fclose (f);

		closed = gsf_output_close (out);
		check (!closed, "close reports the failed rename");
		check (gsf_output_error (out) != NULL,
		       "close failure is recorded as the output's error");
		g_object_unref (out);
	}

	g_unlink (inner);
	g_rmdir (target);
	g_rmdir (dir);
	g_free (inner);
	g_free (target);
	g_free (dir);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();
	test_close_failure_is_reported ();
	gsf_shutdown ();
	return regress_exit_status ();
}
