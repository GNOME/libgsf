/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-tar-badhdr.c: regression test for gsf-infile-tar.c
 *
 * A tar header with more than one invalid numeric field (here both size
 * and mtime) used to make tar_init_info overwrite the first GError with
 * the second, leaking the first.  The leak is only visible under
 * valgrind (t8050-valgrind-regress.pl runs this program there); without
 * valgrind the test just checks that opening such an archive fails
 * cleanly with an error.
 */

#include <gsf/gsf.h>
#include <gsf/gsf-infile-tar.h>
#include <gsf/gsf-input-memory.h>

#include "libgsf-regress.h"

#include <string.h>

#define TAR_SIZE_OFFSET  124
#define TAR_MTIME_OFFSET 136

static void
test_two_bad_fields (void)
{
	GByteArray *b = g_byte_array_new ();
	guint8 zeros[1024];
	GsfInput *src;
	GsfInfile *tar;
	GError *err = NULL;

	regress_tar_add (b, "aaa", "hello");
	memset (zeros, 0, sizeof zeros);
	g_byte_array_append (b, zeros, sizeof zeros);

	/* Make both octal fields invalid (non-octal digits, NUL terminated).
	 * The reader does not verify the header checksum. */
	memset (b->data + TAR_SIZE_OFFSET, 'z', 11);
	b->data[TAR_SIZE_OFFSET + 11] = 0;
	memset (b->data + TAR_MTIME_OFFSET, 'z', 11);
	b->data[TAR_MTIME_OFFSET + 11] = 0;

	src = gsf_input_memory_new_clone (b->data, b->len);
	g_byte_array_free (b, TRUE);

	tar = gsf_infile_tar_new (src, &err);
	check (tar == NULL, "archive with bad size and mtime is rejected");
	check (err != NULL, "a GError is reported for the bad archive");
	if (err != NULL)
		g_error_free (err);
	if (tar != NULL)
		g_object_unref (tar);
	g_object_unref (src);
}

static void
test_bad_mtime_only (void)
{
	GByteArray *b = g_byte_array_new ();
	guint8 zeros[1024];
	GsfInput *src;
	GsfInfile *tar;
	GError *err = NULL;

	regress_tar_add (b, "aaa", "hello");
	memset (zeros, 0, sizeof zeros);
	g_byte_array_append (b, zeros, sizeof zeros);

	memset (b->data + TAR_MTIME_OFFSET, 'z', 11);
	b->data[TAR_MTIME_OFFSET + 11] = 0;

	src = gsf_input_memory_new_clone (b->data, b->len);
	g_byte_array_free (b, TRUE);

	tar = gsf_infile_tar_new (src, &err);
	check (tar == NULL, "archive with bad mtime is rejected");
	check (err != NULL, "a GError is reported for the bad mtime");
	if (err != NULL)
		g_error_free (err);
	if (tar != NULL)
		g_object_unref (tar);
	g_object_unref (src);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_two_bad_fields ();
	test_bad_mtime_only ();

	gsf_shutdown ();
	return regress_exit_status ();
}
