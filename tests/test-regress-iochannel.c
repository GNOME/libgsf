/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-iochannel.c: tests for gsf-output-iochannel.c and
 * gsf-input-iochannel.c, using temp files and pipes as GIOChannel
 * vehicles.
 */

#include "libgsf-regress.h"

#include <gsf/gsf-output-iochannel.h>
#include <gsf/gsf-input-iochannel.h>
#include <gsf/gsf-input-memory.h>

#include <glib/gstdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* Build a temp file and wrap it in a GIOChannel for reading. */
static GIOChannel *
open_read_channel (char const *path)
{
	GIOChannel *channel = g_io_channel_new_file (path, "r", NULL);
	if (channel != NULL)
		g_io_channel_set_encoding (channel, NULL, NULL);
	return channel;
}

/* Build a GIOChannel for writing to @path. */
static GIOChannel *
open_write_channel (char const *path)
{
	GIOChannel *channel = g_io_channel_new_file (path, "w", NULL);
	if (channel != NULL)
		g_io_channel_set_encoding (channel, NULL, NULL);
	return channel;
}

/*
 * Writing through GsfOutputIOChannel to a temp file must produce the
 * written content on disk.  The wrapper takes ownership of the channel
 * (it does not ref it), so the channel is freed when the output is
 * unreffed.
 */
static void
test_output_roundtrip (void)
{
	char *path = g_strdup_printf ("/tmp/gsf-iochan-%d", (int)getpid ());
	GIOChannel *channel = open_write_channel (path);
	GsfOutput *out;
	GsfInput *in;
	guint8 buf[16];
	guint8 const *r;

	check (channel != NULL, "write channel created");
	if (channel == NULL) {
		g_free (path);
		return;
	}

	out = gsf_output_iochannel_new (channel);
	check (out != NULL, "output wrapper created");
	if (out != NULL) {
		check (gsf_output_write (out, 5, (guint8 const *)"hello"),
		       "write succeeds");
		check (gsf_output_tell (out) == 5, "position is 5");
		check (gsf_output_close (out), "close succeeds");
		g_object_unref (out);
	}
	/* The output wrapper owns the channel; it is freed by now.  */

	/* verify the content on disk */
	in = gsf_input_stdio_new (path, NULL);
	check (in != NULL, "file readable after close");
	if (in != NULL) {
		check (gsf_input_size (in) == 5, "file has 5 bytes");
		r = gsf_input_read (in, 5, buf);
		check (r != NULL && memcmp (r, "hello", 5) == 0,
		       "content matches");
		g_object_unref (in);
	}
	g_unlink (path);
	g_free (path);
}

/*
 * Multiple writes accumulate in the file.
 */
static void
test_output_multiple_writes (void)
{
	char *path = g_strdup_printf ("/tmp/gsf-iochan-multi-%d", (int)getpid ());
	GIOChannel *channel = open_write_channel (path);
	GsfOutput *out;

	check (channel != NULL, "write channel created");
	if (channel == NULL) {
		g_free (path);
		return;
	}

	out = gsf_output_iochannel_new (channel);
	if (out != NULL) {
		check (gsf_output_write (out, 5, (guint8 const *)"first"),
		       "first write");
		check (gsf_output_write (out, 1, (guint8 const *)" "),
		       "second write");
		check (gsf_output_write (out, 6, (guint8 const *)"second"),
		       "third write");
		check (gsf_output_tell (out) == 12, "position is 12");
		check (gsf_output_close (out), "close succeeds");
		g_object_unref (out);
	}
	/* the channel is freed by the output wrapper */

	{
		GsfInput *in = gsf_input_stdio_new (path, NULL);
		if (in != NULL) {
			guint8 buf[16];
			guint8 const *r;
			check (gsf_input_size (in) == 12, "file has 12 bytes");
			r = gsf_input_read (in, 12, buf);
			check (r != NULL && memcmp (r, "first second", 12) == 0,
			       "content matches");
			g_object_unref (in);
		}
	}
	g_unlink (path);
	g_free (path);
}

/*
 * GsfOutputIOChannel on a regular file CAN seek (the GIOChannel reports
 * is_seekable for regular files).
 */
static void
test_output_seek_fails (void)
{
	char *path = g_strdup_printf ("/tmp/gsf-iochan-seek-%d", (int)getpid ());
	GIOChannel *channel = open_write_channel (path);
	GsfOutput *out;

	check (channel != NULL, "write channel created");
	if (channel == NULL) {
		g_free (path);
		return;
	}

	out = gsf_output_iochannel_new (channel);
	check (out != NULL, "output wrapper created");
	if (out != NULL) {
		check (gsf_output_seek (out, 0, G_SEEK_SET),
		       "seek to 0 succeeds on a regular file");
		check (gsf_output_seek (out, 10, G_SEEK_SET),
		       "seek to 10 succeeds on a regular file");
		gsf_output_close (out);
		g_object_unref (out);
	}
	/* the channel is freed by the output wrapper */
	g_unlink (path);
	g_free (path);
}

/*
 * gsf_input_memory_new_from_iochannel reads a whole file into a memory
 * input, and the content matches.
 */
static void
test_input_from_file (void)
{
	char *path = g_strdup_printf ("/tmp/gsf-iochan-read-%d", (int)getpid ());
	GsfInput *mem;
	guint8 buf[16];
	guint8 const *r;

	/* write a file with GsfOutput first */
	{
		GsfOutput *out = gsf_output_stdio_new (path, NULL);
		check (out != NULL, "output created for setup");
		if (out != NULL) {
			gsf_output_write (out, 11, (guint8 const *)"hello world");
			gsf_output_close (out);
			g_object_unref (out);
		}
	}

	{
		GIOChannel *channel = open_read_channel (path);
		check (channel != NULL, "read channel created");
		if (channel == NULL) {
			g_unlink (path);
			g_free (path);
			return;
		}
		mem = gsf_input_memory_new_from_iochannel (channel, NULL);
		g_io_channel_shutdown (channel, TRUE, NULL);
		g_io_channel_unref (channel);
	}

	check (mem != NULL, "memory input created from iochannel");
	if (mem != NULL) {
		check (gsf_input_size (mem) == 11, "size is 11");
		r = gsf_input_read (mem, 11, buf);
		check (r != NULL && memcmp (r, "hello world", 11) == 0,
		       "content matches");
		g_object_unref (mem);
	}
	g_unlink (path);
	g_free (path);
}

/*
 * An empty GIOChannel yields an empty memory input: the G_IO_STATUS_EOF
 * returned by g_io_channel_read_to_end is accepted as a successful (0-byte)
 * read, and the caller gets a valid 0-byte GsfInputMemory.
 */
static void
test_input_empty (void)
{
	char *path = g_strdup_printf ("/tmp/gsf-iochan-empty-%d", (int)getpid ());
	GIOChannel *channel;
	GsfInput *mem;
	GError *err = NULL;

	/* create an empty file */
	{
		GsfOutput *out = gsf_output_stdio_new (path, NULL);
		if (out != NULL) {
			gsf_output_close (out);
			g_object_unref (out);
		}
	}

	channel = open_read_channel (path);
	check (channel != NULL, "read channel created");
	if (channel == NULL) {
		g_unlink (path);
		g_free (path);
		return;
	}
	mem = gsf_input_memory_new_from_iochannel (channel, &err);
	check (mem != NULL, "empty channel returns a valid input");
	check (mem != NULL && gsf_input_size (mem) == 0,
	       "the empty input has size 0");
	if (err != NULL)
		g_error_free (err);
	g_io_channel_shutdown (channel, TRUE, NULL);
	g_io_channel_unref (channel);
	g_unlink (path);
	g_free (path);
}

/*
 * A pipe as GIOChannel: write to the write end, then read from the read
 * end after the write end is closed (so the reader sees EOF).
 */
static void
test_pipe_roundtrip (void)
{
	int fds[2];
	pid_t pid;

	check (pipe (fds) == 0, "pipe created");
	if (fds[0] < 0)
		return;

	pid = fork ();
	if (pid < 0) {
		close (fds[0]);
		close (fds[1]);
		return;
	}

	if (pid == 0) {
		/* Child: write through GsfOutputIOChannel, then exit
		 * (which closes the write end).  The output wrapper
		 * takes ownership of the channel.  */
		GIOChannel *chan = g_io_channel_unix_new (fds[1]);
		GsfOutput *out;
		g_io_channel_set_encoding (chan, NULL, NULL);
		g_io_channel_set_close_on_unref (chan, TRUE);
		close (fds[0]);
		out = gsf_output_iochannel_new (chan);
		if (out != NULL) {
			gsf_output_write (out, 5, (guint8 const *)"hello");
			gsf_output_close (out);
			g_object_unref (out);
		}
		_exit (0);
	}

	/* Parent: close the write end, then read from the read end.  */
	close (fds[1]);
	{
		GIOChannel *chan = g_io_channel_unix_new (fds[0]);
		GsfInput *mem;
		g_io_channel_set_encoding (chan, NULL, NULL);
		g_io_channel_set_close_on_unref (chan, TRUE);
		mem = gsf_input_memory_new_from_iochannel (chan, NULL);
		check (mem != NULL, "memory input from the read end");
		if (mem != NULL) {
			guint8 buf[8];
			guint8 const *r = gsf_input_read (mem, 5, buf);
			check (gsf_input_size (mem) == 5,
			       "the read end has 5 bytes");
			check (r != NULL && memcmp (r, "hello", 5) == 0,
			       "pipe content matches");
			g_object_unref (mem);
		}
		/* gsf_input_memory_new_from_iochannel does not take
		 * ownership of the channel; we still hold our ref.  */
		g_io_channel_unref (chan);
	}
	/* Reap the child.  */
	{
		int status;
		waitpid (pid, &status, 0);
	}
}

/*
 * Closing the output flushes the channel; when that fails (here /dev/full
 * gives ENOSPC) gsf_output_close used to ignore the result of
 * g_io_channel_shutdown and report success although buffered data was
 * lost.  The failure must be reported and recorded as the output's error.
 */
static void
test_close_flush_failure_is_reported (void)
{
	GIOChannel *chan;
	GsfOutput *out;
	gboolean closed;

	if (!g_file_test ("/dev/full", G_FILE_TEST_EXISTS)) {
		g_print ("skipped: /dev/full not available\n");
		return;
	}
	chan = g_io_channel_new_file ("/dev/full", "w", NULL);
	if (chan == NULL) {
		g_print ("skipped: cannot open /dev/full\n");
		return;
	}
	g_io_channel_set_encoding (chan, NULL, NULL);

	out = gsf_output_iochannel_new (chan);
	check (out != NULL, "output created");
	if (out == NULL) {
		g_io_channel_unref (chan);
		return;
	}

	/* Small enough to stay in the channel's buffer.  */
	check (gsf_output_write (out, 5, (guint8 const *)"hello"),
	       "buffered write succeeds");
	closed = gsf_output_close (out);
	check (!closed, "close reports the failed flush");
	check (gsf_output_error (out) != NULL,
	       "close failure is recorded as the output's error");
	g_object_unref (out);
}

/*
 * A failed write must leave an error on the output (the base class
 * documents that a FALSE return has assigned one), so disk-full
 * conditions can be diagnosed with gsf_output_error().  A write larger
 * than the channel buffer reaches /dev/full immediately and fails with
 * ENOSPC.
 */
static void
test_write_failure_sets_error (void)
{
	enum { BIG = 1 << 20 };
	GIOChannel *chan;
	GsfOutput *out;
	guint8 *buf;

	if (!g_file_test ("/dev/full", G_FILE_TEST_EXISTS)) {
		g_print ("skipped: /dev/full not available\n");
		return;
	}
	chan = g_io_channel_new_file ("/dev/full", "w", NULL);
	if (chan == NULL) {
		g_print ("skipped: cannot open /dev/full\n");
		return;
	}
	g_io_channel_set_encoding (chan, NULL, NULL);

	out = gsf_output_iochannel_new (chan);
	check (out != NULL, "output created");
	if (out == NULL) {
		g_io_channel_unref (chan);
		return;
	}

	buf = g_malloc0 (BIG);
	check (!gsf_output_write (out, BIG, buf), "large write to /dev/full fails");
	check (gsf_output_error (out) != NULL,
	       "write failure is recorded as the output's error");
	g_free (buf);

	/* The close will fail too; that is not what is tested here.  */
	gsf_output_close (out);
	g_object_unref (out);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_output_roundtrip ();
	test_output_multiple_writes ();
	test_output_seek_fails ();
	test_input_from_file ();
	test_input_empty ();
	test_pipe_roundtrip ();
	test_close_flush_failure_is_reported ();
	test_write_failure_sets_error ();

	gsf_shutdown ();
	return regress_exit_status ();
}
