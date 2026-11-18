/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-stdio.c: regression tests for gsf-input-stdio.c and
 * gsf-output-stdio.c
 * (the pipe exercise is meant to be run under valgrind with --track-fds;
 * see t8050-valgrind-regress.pl)
 */

#include <gsf/gsf.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-output-stdio.h>

#include <glib/gstdio.h>

#include "libgsf-regress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Build a unique temp path for this process. */
static char *
make_temp_path (char const *tag)
{
	return g_strdup_printf ("/tmp/gsf-regress-stdio-%d-%s",
				(int)getpid (), tag);
}

/* Write @len bytes of @content to @path using GsfOutputStdio. */
static gboolean
write_file (char const *path, guint8 const *content, size_t len)
{
	GsfOutput *out = gsf_output_stdio_new (path, NULL);
	gboolean ok;

	if (out == NULL)
		return FALSE;
	ok = (len == 0) || gsf_output_write (out, len, content);
	ok = gsf_output_close (out) && ok;
	g_object_unref (out);
	return ok;
}

/*
 * Reading a regular file: size and content must match what was written,
 * and the name must be propagated from the filename.
 */
static void
test_read_regular_file (void)
{
	char *path = make_temp_path ("read");
	static guint8 const content[] = "hello stdio world";
	GsfInput *in;

	check (write_file (path, content, sizeof content - 1), "file written");

	in = gsf_input_stdio_new (path, NULL);
	check (in != NULL, "input opened");
	if (in != NULL) {
		guint8 buf[32];
		guint8 const *r;

		check (gsf_input_size (in) == (gsf_off_t)(sizeof content - 1),
		       "size matches the file");
		check (gsf_input_name (in) != NULL &&
		       strstr (gsf_input_name (in), "read") != NULL,
		       "name derived from the filename");
		r = gsf_input_read (in, sizeof content - 1, buf);
		check (r != NULL && memcmp (r, content, sizeof content - 1) == 0,
		       "content matches");
		g_object_unref (in);
	}
	g_unlink (path);
	g_free (path);
}

/*
 * Seeking within a stdio input must reposition reads, and gsf_input_tell
 * must track the position.
 */
static void
test_input_seek (void)
{
	char *path = make_temp_path ("seek");
	static guint8 const content[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	GsfInput *in;

	check (write_file (path, content, sizeof content - 1), "file written");

	in = gsf_input_stdio_new (path, NULL);
	if (in != NULL) {
		guint8 buf[8];
		guint8 const *r;

		/* gsf_input_seek returns TRUE on error. */
		check (!gsf_input_seek (in, 10, G_SEEK_SET), "seek to 10");
		check (gsf_input_tell (in) == 10, "position is 10");
		r = gsf_input_read (in, 6, buf);
		check (r != NULL && memcmp (r, "abcdef", 6) == 0,
		       "read continues at the seek position");
		check (!gsf_input_seek (in, 0, G_SEEK_END), "seek to the end");
		check (gsf_input_tell (in) == (gsf_off_t)(sizeof content - 1),
		       "end position matches the size");
		g_object_unref (in);
	}
	g_unlink (path);
	g_free (path);
}

/*
 * Duplication of a stdio input must produce an independent, same-sized
 * stream with the same name.
 */
static void
test_input_dup (void)
{
	char *path = make_temp_path ("dup");
	static guint8 const content[] = "duplicate me";
	GsfInput *in, *dup;

	check (write_file (path, content, sizeof content - 1), "file written");

	in = gsf_input_stdio_new (path, NULL);
	check (in != NULL, "input opened");
	if (in != NULL) {
		guint8 buf[16];
		guint8 const *r;

		dup = gsf_input_dup (in, NULL);
		check (dup != NULL, "dup works");
		if (dup != NULL) {
			check (gsf_input_size (dup) == gsf_input_size (in),
			       "duplicate has the same size");
			check (0 == strcmp (gsf_input_name (dup) ? gsf_input_name (dup) : "",
					    gsf_input_name (in) ? gsf_input_name (in) : ""),
			       "duplicate has the same name");
			r = gsf_input_read (dup, 5, buf);
			check (r != NULL && memcmp (r, "dupli", 5) == 0,
			       "duplicate content readable");
			g_object_unref (dup);
		}
		/* the original must be unaffected by the duplicate's reads */
		r = gsf_input_read (in, 5, buf);
		check (r != NULL && memcmp (r, "dupli", 5) == 0,
		       "original still reads from its own position");
		g_object_unref (in);
	}
	g_unlink (path);
	g_free (path);
}

/*
 * Writing through GsfOutputStdio and reading the result back through
 * GsfInputStdio must round-trip the content.
 */
static void
test_output_roundtrip (void)
{
	char *path = make_temp_path ("roundtrip");
	static guint8 const content[] = "written through gsf";
	GsfOutput *out = gsf_output_stdio_new (path, NULL);
	GsfInput *in;

	check (out != NULL, "output created");
	if (out != NULL) {
		check (gsf_output_write (out, sizeof content - 1, content),
		       "write ok");
		check (gsf_output_close (out), "close succeeds");
		g_object_unref (out);

		in = gsf_input_stdio_new (path, NULL);
		check (in != NULL, "file readable after close");
		if (in != NULL) {
			guint8 buf[32];
			guint8 const *r;
			check (gsf_input_size (in) == (gsf_off_t)(sizeof content - 1),
			       "size matches what was written");
			r = gsf_input_read (in, sizeof content - 1, buf);
			check (r != NULL && memcmp (r, content, sizeof content - 1) == 0,
			       "content round-trips");
			g_object_unref (in);
		}
	}
	g_unlink (path);
	g_free (path);
}

/*
 * printf through GsfOutputStdio must land in the file.
 */
static void
test_output_printf (void)
{
	char *path = make_temp_path ("printf");
	GsfOutput *out = gsf_output_stdio_new (path, NULL);
	GsfInput *in;

	check (out != NULL, "output created");
	if (out != NULL) {
		check (gsf_output_printf (out, "%s=%d", "answer", 42) > 0,
		       "printf returns the length");
		check (gsf_output_close (out), "close succeeds");
		g_object_unref (out);

		in = gsf_input_stdio_new (path, NULL);
		if (in != NULL) {
			guint8 buf[32];
			guint8 const *r;
			check (gsf_input_size (in) == 9, "printf output size is 9");
			r = gsf_input_read (in, 9, buf);
			check (r != NULL && memcmp (r, "answer=42", 9) == 0,
			       "formatted content matches");
			g_object_unref (in);
		}
	}
	g_unlink (path);
	g_free (path);
}

/*
 * Opening a non-existent file must fail with an error set.
 */
static void
test_input_error (void)
{
	char *path = make_temp_path ("nonexistent");
	GsfInput *in;
	GError *err = NULL;

	g_unlink (path);		/* make sure it is not there */
	in = gsf_input_stdio_new (path, &err);
	check (in == NULL, "opening a missing file fails");
	check (err != NULL, "an error is reported");
	if (err != NULL)
		g_error_free (err);

	g_free (path);
}

/*
 * gsf_input_stdio_new_FILE on a regular FILE* wraps the stream directly
 * (no local copy), and with keep_open == FALSE the input owns the FILE*.
 */
static void
test_new_FILE_regular (void)
{
	char *path = make_temp_path ("newfile");
	static guint8 const content[] = "wrapped FILE";
	FILE *f;
	GsfInput *in;

	check (write_file (path, content, sizeof content - 1), "file written");

	f = fopen (path, "rb");
	check (f != NULL, "fopen succeeded");
	if (f != NULL) {
		guint8 buf[16];
		guint8 const *r;

		in = gsf_input_stdio_new_FILE ("wrapped", f, FALSE);
		check (in != NULL, "new_FILE wraps a regular file");
		if (in != NULL) {
			check (gsf_input_size (in) == (gsf_off_t)(sizeof content - 1),
			       "size from the FILE*");
			r = gsf_input_read (in, sizeof content - 1, buf);
			check (r != NULL && memcmp (r, content, sizeof content - 1) == 0,
			       "content via the FILE* wrapper");
			g_object_unref (in);	/* closes the FILE* (keep_open == FALSE) */
		}
	}
	g_unlink (path);
	g_free (path);
}

/*
 * On the make_local_copy path (pipes, ttys and other non-regular files)
 * the FILE* was never closed, leaking a file descriptor (and the stdio
 * buffer) on every call.  This function deliberately exercises that
 * path; on a fixed libgsf valgrind's --track-fds reports no open
 * descriptor when run from t8050-valgrind-regress.pl.
 */
static void
test_stdio_file_leak (void)
{
	FILE *pipe = popen ("echo hello", "r");
	GsfInput *in;

	if (pipe == NULL)
		return;

	/* A pipe is not a regular file, so this takes the make_local_copy
	 * path. */
	in = gsf_input_stdio_new_FILE ("pipe", pipe, FALSE);
	if (in != NULL)
		g_object_unref (in);
}

/*
 * gsf_output_printf() on a closed stdio output passed a NULL FILE* to
 * vfprintf and crashed.  It must fail cleanly (Write and Seek already do).
 */
static void
test_printf_after_close (void)
{
	char *path = make_temp_path ("printf-closed");
	GsfOutput *out = gsf_output_stdio_new (path, NULL);

	check (out != NULL, "output created");
	if (out != NULL) {
		check (gsf_output_close (out), "close succeeds");
		inhibit_g_log_messages ();
		check (gsf_output_printf (out, "late %d", 1) == FALSE,
		       "printf after close fails instead of crashing");
		restore_g_log_messages ();
		g_object_unref (out);
	}
	g_unlink (path);
	g_free (path);
}

static int
count_save_files (char const *dir)
{
	GDir *d = g_dir_open (dir, 0, NULL);
	char const *name;
	int n = 0;

	if (d == NULL)
		return 0;
	while ((name = g_dir_read_name (d)) != NULL)
		if (g_str_has_prefix (name, ".gsf-save-"))
			n++;
	g_dir_close (d);
	return n;
}

/*
 * When the final rename of the temp file onto the target fails, the temp
 * file (holding the whole just-written content) used to be left behind.
 */
static void
test_failed_rename_cleans_temp (void)
{
	char *dir = make_temp_path ("rename-fail-dir");
	char *path = g_build_filename (dir, "target", NULL);
	char *inner = g_build_filename (path, "x", NULL);
	int before;
	GsfOutput *out;

	/* Count only in our own directory: other tests may be writing
	 * .gsf-save-* files elsewhere in /tmp at the same time. */
	g_assert (g_mkdir_with_parents (dir, 0755) == 0);
	before = count_save_files (dir);

	g_unlink (path);
	out = gsf_output_stdio_new (path, NULL);
	check (out != NULL, "output created");
	if (out != NULL) {
		FILE *f;

		check (gsf_output_write (out, 5, (guint8 const *) "hello"),
		       "write succeeds");
		/* Make the target a non-empty directory so rename() fails. */
		g_assert (g_mkdir (path, 0755) == 0);
		f = g_fopen (inner, "w");
		g_assert (f != NULL);
		fclose (f);

		check (!gsf_output_close (out), "close fails when the rename fails");
		check (count_save_files (dir) == before,
		       "no .gsf-save-* temp file left behind");
		g_object_unref (out);
	}
	g_unlink (inner);
	g_rmdir (path);
	g_rmdir (dir);
	g_free (inner);
	g_free (path);
	g_free (dir);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_read_regular_file ();
	test_input_seek ();
	test_input_dup ();
	test_output_roundtrip ();
	test_output_printf ();
	test_input_error ();
	test_new_FILE_regular ();
	test_stdio_file_leak ();
	test_printf_after_close ();
	test_failed_rename_cleans_temp ();

	gsf_shutdown ();
	return regress_exit_status ();
}
