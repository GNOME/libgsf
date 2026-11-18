/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-gio.c: GsfOutputGio regressions
 */

#include <gsf/gsf.h>
#include <gsf/gsf-input-gio.h>
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

/*
 * When reading the source fails, make_local_copy() in gsf-input-gio.c has
 * no copy to name, but it still called set_name_from_file (NULL, file),
 * which trips gsf_input_set_name's g_return_val_if_fail (a CRITICAL, fatal
 * under G_DEBUG=fatal-criticals).  /proc/self/mem opens fine but every
 * read at offset 0 fails with EIO, which gives us a failing read.
 */
static void
test_failed_local_copy_is_quiet (void)
{
	const char *path = "/proc/self/mem";
	GsfInput *in;
	FILE *f = fopen (path, "r");

	if (f == NULL) {
		g_print ("skipped: %s not available\n", path);
		return;
	}
	fclose (f);

	inhibit_g_log_messages ();
	in = gsf_input_gio_new_for_path (path, NULL);
	restore_g_log_messages ();

	if (in != NULL) {
		/* Reads work on this system; nothing to test. */
		g_print ("skipped: %s is readable here\n", path);
		g_object_unref (in);
		return;
	}

	check (inhibited_g_log_messages == 0,
	       "failed local copy logs no warnings or criticals (got %d)",
	       inhibited_g_log_messages);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();
	test_close_failure_is_reported ();
	test_failed_local_copy_is_quiet ();
	gsf_shutdown ();
	return regress_exit_status ();
}
