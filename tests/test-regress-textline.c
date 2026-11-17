/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-textline.c: regression tests for gsf-input-textline.c
 */

#include <gsf/gsf.h>
#include <gsf/gsf-input-textline.h>
#include <gsf/gsf-input-memory.h>

#include "libgsf-regress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Wrap @text in a memory input and a GsfInputTextline that owns it.
 * NOTE: the line getters return (transfer none) pointers to ONE internal
 * buffer that is overwritten by every call, so each line must be consumed
 * before the next one is read.
 */
static GsfInput *
make_textline (char const *text)
{
	GsfInput *in = gsf_input_memory_new_clone
		((guint8 const *)text, strlen (text));
	GsfInput *tl = gsf_input_textline_new (in);

	g_object_unref (in);		/* the textline holds its own reference */
	return tl;
}

/*
 * Lines are returned in order, and NULL signals the end of the stream.
 * gsf_input_textline_ascii_gets is a thin alias for the utf8 variant and
 * must behave identically; it is exercised here as well.
 */
static void
test_sequential_lines (void)
{
	GsfInput *tl = make_textline ("line one\nline two\nline three\n");
	unsigned char *line;

	line = gsf_input_textline_ascii_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "line one") == 0,
	       "first line read correctly (ascii alias)");
	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "line two") == 0,
	       "second line read correctly");
	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "line three") == 0,
	       "third line read correctly");
	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line == NULL, "eof reported after the last line");

	g_object_unref (tl);
}

/*
 * A final line without a terminating newline must still be returned, and
 * the following call must report eof.
 */
static void
test_unterminated_last_line (void)
{
	GsfInput *tl = make_textline ("a\nb");
	guint8 *line;

	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "a") == 0,
	       "terminated line read correctly");
	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "b") == 0,
	       "unterminated last line read correctly");
	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line == NULL, "eof reported after the unterminated line");

	g_object_unref (tl);
}

/*
 * CRLF pairs, lone CR and empty lines are all line terminators.  The
 * sequence "a\r\nb\rc\n\r\n" must yield "a", "b", "c" and one empty line.
 */
static void
test_crlf_and_empty_lines (void)
{
	GsfInput *tl = make_textline ("a\r\nb\rc\n\r\n");
	guint8 *line;

	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "a") == 0, "CRLF line read correctly");
	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "b") == 0, "lone CR line read correctly");
	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "c") == 0, "LF line read correctly");
	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "") == 0,
	       "empty line read correctly");
	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line == NULL, "eof reported after the empty line");

	g_object_unref (tl);
}

/*
 * An empty input yields NULL immediately.
 */
static void
test_empty_file (void)
{
	GsfInput *tl = make_textline ("");
	guint8 *line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));

	check (line == NULL, "empty input reports eof immediately");

	g_object_unref (tl);
}

/*
 * Lines longer than the internal chunk size (512 bytes) must be
 * accumulated across reads, and the lines that follow must not be
 * affected.
 */
static void
test_long_line (void)
{
	GString *text = g_string_new (NULL);
	unsigned i;
	GsfInput *tl;
	guint8 *line;
	gboolean long_ok;

	for (i = 0; i < 1000; i++)
		g_string_append_c (text, (gchar)('a' + (i % 26)));
	g_string_append (text, "\ntail\n");

	tl = make_textline (text->str);

	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	long_ok = (line != NULL && strlen ((char *)line) == 1000);
	if (long_ok) {
		for (i = 0; i < 1000; i++)
			if (line[i] != (guint8)('a' + (i % 26))) {
				long_ok = FALSE;
				break;
			}
	}
	check (long_ok, "long line accumulated correctly");

	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "tail") == 0,
	       "line after the long line read correctly");

	g_object_unref (tl);
	g_string_free (text, TRUE);
}

/*
 * Seeking must reset the logical position, so the stream can be read
 * again from the beginning.
 */
static void
test_seek (void)
{
	GsfInput *tl = make_textline ("line one\nline two\n");
	guint8 *line;

	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "line one") == 0,
	       "first line read correctly");
	/* gsf_input_seek returns TRUE on error, FALSE on success. */
	check (!gsf_input_seek (tl, 0, G_SEEK_SET), "seek to the beginning");
	check (gsf_input_tell (tl) == 0, "position reset by the seek");
	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "line one") == 0,
	       "line read again after the seek");
	line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	check (line != NULL && strcmp ((char *)line, "line two") == 0,
	       "reading continues after the seek");

	g_object_unref (tl);
}

/*
 * Duplication must produce a same-named, same-sized textline.  (The
 * duplicate currently shares the underlying source cursor, so full
 * independence of the two streams is not asserted here.)
 */
static void
test_dup (void)
{
	GsfInput *tl = make_textline ("line one\nline two\n");
	GsfInput *dup = gsf_input_dup (tl, NULL);

	check (dup != NULL, "dup works");
	check (dup != NULL && gsf_input_size (dup) == gsf_input_size (tl),
	       "duplicate has the same size");
	check (dup != NULL && 0 == strcmp (gsf_input_name (dup) ? gsf_input_name (dup) : "",
					   gsf_input_name (tl) ? gsf_input_name (tl) : ""),
	       "duplicate has the same name");

	if (dup != NULL)
		g_object_unref (dup);
	g_object_unref (tl);
}

/*
 * The wrapper reports the source's size and name.
 */
static void
test_wrapper_metadata (void)
{
	GsfInput *in = gsf_input_memory_new_clone
		((guint8 const *)"some content\n", 13);
	GsfInput *tl;

	gsf_input_set_name (in, "named.txt");
	tl = gsf_input_textline_new (in);
	g_object_unref (in);		/* the textline holds its own reference */

	check (gsf_input_size (tl) == 13, "size matches the source");
	check (gsf_input_name (tl) != NULL &&
	       0 == strcmp (gsf_input_name (tl), "named.txt"),
	       "name propagated from the source");

	g_object_unref (tl);
}

/*
 * A block read after a line read must continue at the logical position.
 * The Read vfunc read from the source's already-advanced position instead
 * of the input's, and dropped the buffered remainder, so mixing
 * gsf_input_textline_utf8_gets() with gsf_input_read() yielded shifted
 * data (here: the read failed outright).
 */
static void
test_block_read_after_line_read (void)
{
	GsfInput *tl = make_textline ("line one\nline two\nline three\n");
	guint8 *line = gsf_input_textline_utf8_gets (GSF_INPUT_TEXTLINE (tl));
	guint8 buf[16];
	guint8 const *r;

	check (line != NULL && strcmp ((char *)line, "line one") == 0,
	       "first line read correctly");
	/* (transfer none): the buffer belongs to the textline object. */

	r = gsf_input_read (tl, 9, buf);
	check (r != NULL, "block read after line read succeeds");
	check (r != NULL && memcmp (r, "line two\n", 9) == 0,
	       "block read continues at the logical position");

	g_object_unref (tl);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_sequential_lines ();
	test_unterminated_last_line ();
	test_crlf_and_empty_lines ();
	test_empty_file ();
	test_long_line ();
	test_seek ();
	test_dup ();
	test_wrapper_metadata ();
	test_block_read_after_line_read ();

	gsf_shutdown ();
	return regress_exit_status ();
}
