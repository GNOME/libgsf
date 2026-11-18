/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-stdio-fdopen.c: GsfOutputStdio when fdopen() fails after
 * mkstemp() has succeeded.
 *
 * Making fdopen() fail on demand requires overriding it in this program
 * (glibc), so this is a separate binary; the override only acts while
 * fail_fdopen is set.
 */

#define _GNU_SOURCE
#include <gsf/gsf.h>
#include <gsf/gsf-output-stdio.h>

#include "libgsf-regress.h"

#include <glib/gstdio.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef __GLIBC__
#include <dirent.h>
#include <dlfcn.h>

static int fail_fdopen;

FILE *
fdopen (int fd, char const *mode)
{
	FILE *(*real) (int, char const *);

	if (fail_fdopen) {
		errno = ENOMEM;
		return NULL;
	}
	real = (FILE *(*) (int, char const *)) dlsym (RTLD_NEXT, "fdopen");
	return real (fd, mode);
}

static int
count_open_fds (void)
{
	DIR *d = opendir ("/proc/self/fd");
	int n = 0;

	if (d == NULL)
		return -1;
	while (readdir (d) != NULL)
		n++;
	closedir (d);
	return n;
}

static int
count_save_files (char const *dir)
{
	DIR *d = opendir (dir);
	struct dirent *e;
	int n = 0;

	if (d == NULL)
		return -1;
	while ((e = readdir (d)) != NULL)
		if (strncmp (e->d_name, ".gsf-save-", 10) == 0)
			n++;
	closedir (d);
	return n;
}

/*
 * If mkstemp() succeeded but fdopen() failed, the descriptor and the temp
 * file it created were never released.  The GError code was also taken from
 * errno after g_filename_display_name() had run, so it could disagree with
 * the message.
 */
static void
test_fdopen_failure (void)
{
	enum { N = 50 };
	char *dir = g_strdup_printf ("/tmp/gsf-regress-stdio-fdopen-%d", (int)getpid ());
	char *target = g_build_filename (dir, "target", NULL);
	int fds_before, fds_after, files_after;
	int i, nulls = 0, codes_ok = 0;

	g_assert (g_mkdir_with_parents (dir, 0755) == 0);
	fds_before = count_open_fds ();

	fail_fdopen = 1;
	for (i = 0; i < N; i++) {
		GError *err = NULL;
		GsfOutput *out = gsf_output_stdio_new (target, &err);

		if (out == NULL)
			nulls++;
		else
			g_object_unref (out);
		if (err != NULL && err->code == ENOMEM)
			codes_ok++;
		g_clear_error (&err);
	}
	fail_fdopen = 0;

	fds_after = count_open_fds ();
	files_after = count_save_files (dir);

	check (nulls == N, "every attempt fails when fdopen fails (%d of %d)", nulls, N);
	check (codes_ok == N, "the error code is the saved errno, ENOMEM (%d of %d)",
	       codes_ok, N);
	check (files_after == 0, "no temp file is left behind (%d found)", files_after);
	if (fds_before >= 0)
		check (fds_after == fds_before,
		       "no file descriptor is leaked (%d before, %d after)",
		       fds_before, fds_after);

	g_rmdir (dir);
	g_free (target);
	g_free (dir);
}
#endif

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();
#ifdef __GLIBC__
	test_fdopen_failure ();
#else
	g_print ("skipping: needs glibc to override fdopen\n");
#endif
	gsf_shutdown ();
	return regress_exit_status ();
}
