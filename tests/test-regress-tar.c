/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-tar.c: regression tests for gsf-infile-tar.c
 */

#include <gsf/gsf.h>
#include <gsf/gsf-infile-tar.h>
#include <gsf/gsf-input-memory.h>

#include <glib/gstdio.h>

#include "libgsf-regress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Add a GNU longname entry for @realname followed by the file entry that
 * carries @content: the 'L' (typeflag) entry's name is the magic marker
 * and its DATA holds the real name.
 */
/* Build a crafted archive with a plain member, a nested member and a
 * long-named member. */
static GsfInput *
make_test_tar (char *longname_buf, size_t longname_buf_size)
{
	GByteArray *b = g_byte_array_new ();
	guint8 zeros[1024];
	GsfInput *input;
	char *longname = g_new (char, 120);
	unsigned i;

	memset (longname, 0, 120);
	for (i = 0; i < 119; i++)
		longname[i] = (char)('A' + (i % 26));
	longname[119] = '\0';

	regress_tar_add (b, "aaa", "hello");
	regress_tar_add (b, "dir/file", "nested");
	regress_tar_add_longname (b, longname, "long content");
	memset (zeros, 0, sizeof zeros);
	g_byte_array_append (b, zeros, sizeof zeros);

	if (longname_buf != NULL && longname_buf_size >= 120)
		memcpy (longname_buf, longname, 120);
	g_free (longname);

	input = gsf_input_memory_new_clone (b->data, b->len);
	g_byte_array_free (b, TRUE);
	return input;
}

static GsfInput *
make_flat_tar (void)
{
	GByteArray *b = g_byte_array_new ();
	guint8 zeros[1024];
	GsfInput *input;

	regress_tar_add (b, "aaa", "hello");
	regress_tar_add (b, "bbb", "world");
	memset (zeros, 0, sizeof zeros);
	g_byte_array_append (b, zeros, sizeof zeros);

	input = gsf_input_memory_new_clone (b->data, b->len);
	g_byte_array_free (b, TRUE);
	return input;
}

/*
 * gsf_input_dup() on a tar infile must work.  gsf_infile_tar_dup() built
 * the complete duplicate (children, names and referenced objects) and then
 * returned NULL, so every dup failed and leaked the whole object graph.
 */
static void
test_tar_dup (void)
{
	GsfInput *input = make_flat_tar ();
	GsfInfile *tar = gsf_infile_tar_new (input, NULL);
	GsfInput *dup;
	GError *err = NULL;

	check (tar != NULL, "tar opened");
	if (tar == NULL) {
		if (err != NULL)
			g_printerr ("tar error: %s\n", err->message);
		g_clear_error (&err);
		g_object_unref (input);
		return;
	}

	dup = gsf_input_dup (GSF_INPUT (tar), &err);
	check (dup != NULL, "gsf_input_dup of tar infile");
	if (dup != NULL) {
		check (gsf_input_size (dup) == gsf_input_size (GSF_INPUT (tar)),
		       "duplicate has the same size");
		g_object_unref (dup);
	}
	if (err != NULL) {
		g_printerr ("dup error: %s\n", err->message);
		g_error_free (err);
	}

	g_object_unref (tar);
	g_object_unref (input);
}

/*
 * Listing a tar archive: the top-level children appear in file order, and
 * members under a path land in a directory child.
 */
static void
test_basic_listing (void)
{
	char longname[120];
	GsfInput *input = make_test_tar (longname, sizeof longname);
	GsfInfile *tar = gsf_infile_tar_new (input, NULL);

	check (tar != NULL, "tar opened");
	if (tar == NULL) {
		g_object_unref (input);
		return;
	}

	check (gsf_infile_num_children (tar) == 3,
	       "three top-level children");
	check (0 == strcmp (gsf_infile_name_by_index (tar, 0), "aaa"),
	       "first child is 'aaa'");
	check (0 == strcmp (gsf_infile_name_by_index (tar, 1), "dir"),
	       "second child is 'dir'");
	check (0 == strcmp (gsf_infile_name_by_index (tar, 2), longname),
	       "the long name survived GNU longname entries");

	g_object_unref (tar);
	g_object_unref (input);
}

/*
 * Member content: flat members, members under a directory (found via
 * child_by_vname), and long-named members must all read back with the
 * right size and content.  Seeking within a member repositions reads.
 */
static void
test_child_content (void)
{
	char longname[120];
	GsfInput *input = make_test_tar (longname, sizeof longname);
	GsfInfile *tar = gsf_infile_tar_new (input, NULL);
	GsfInput *child;
	GError *err = NULL;

	check (tar != NULL, "tar opened");
	if (tar == NULL) {
		g_clear_error (&err);
		g_object_unref (input);
		return;
	}

	/* flat member */
	child = gsf_infile_child_by_name (tar, "aaa");
	check (child != NULL, "flat member found");
	if (child != NULL) {
		guint8 buf[5];
		guint8 const *r;

		check (gsf_input_size (child) == 5, "flat member size is 5");
		r = gsf_input_read (child, 2, buf);
		check (r != NULL && memcmp (r, "he", 2) == 0, "partial read ok");
		check (!gsf_input_seek (child, 0, G_SEEK_SET),
		       "seek back to the start");
		r = gsf_input_read (child, 5, buf);
		check (r != NULL && memcmp (r, "hello", 5) == 0,
		       "member content matches");
		g_object_unref (child);
	}

	/* nested member */
	child = gsf_infile_child_by_vname (tar, "dir", "file", NULL);
	check (child != NULL, "nested member found");
	if (child != NULL) {
		guint8 buf[6];
		guint8 const *r;
		check (gsf_input_size (child) == 6, "nested member size is 6");
		r = gsf_input_read (child, 6, buf);
		check (r != NULL && memcmp (r, "nested", 6) == 0,
		       "nested member content matches");
		g_object_unref (child);
	}

	/* long-named member */
	child = gsf_infile_child_by_name (tar, longname);
	check (child != NULL, "long-named member found");
	if (child != NULL) {
		guint8 buf[12];
		guint8 const *r;
		check (gsf_input_size (child) == 12, "long-named member size is 12");
		r = gsf_input_read (child, 12, buf);
		check (r != NULL && memcmp (r, "long content", 12) == 0,
		       "long-named member content matches");
		g_object_unref (child);
	}

	g_object_unref (tar);
	g_object_unref (input);
}

/*
 * The reader must accept archives created by the real GNU tar command,
 * not only hand-crafted ones.  A small file, a nested file and a long-
 * named file are written to a temp directory, archived with `tar`, and
 * then verified through libgsf.
 */
static void
test_real_tar (void)
{
	char *dir = g_strdup_printf ("/tmp/gsf-regress-tar-%d", (int)getpid ());
	char *archive = g_strdup_printf ("%s.tar", dir);
	char *small = g_strdup_printf ("%s/small.txt", dir);
	char *sub = g_strdup_printf ("%s/sub", dir);
	char *nested = g_strdup_printf ("%s/sub/nested.txt", dir);
	char *longname_path = g_strdup_printf
		("%s/%s", dir,
		 "L012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789");
	char *cmd;
	GsfInput *input;
	GsfInfile *tar;
	GsfInput *child;
	GError *err = NULL;

	check (g_mkdir (dir, 0777) == 0, "temp directory created");
	check (g_mkdir (sub, 0777) == 0, "sub directory created");
	{
		FILE *f = fopen (small, "wb");
		check (f != NULL, "small file created");
		if (f != NULL) {
			fwrite ("tar small", 1, 9, f);
			fclose (f);
		}
		f = fopen (nested, "wb");
		check (f != NULL, "nested file created");
		if (f != NULL) {
			fwrite ("tar nested", 1, 10, f);
			fclose (f);
		}
		f = fopen (longname_path, "wb");
		check (f != NULL, "long-named file created");
		if (f != NULL) {
			fwrite ("tar long", 1, 8, f);
			fclose (f);
		}
	}
	cmd = g_strdup_printf
		("tar --format=gnu -C %s -cf %s small.txt sub/nested.txt %s >/dev/null 2>&1",
		 dir, archive,
		 "L012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789");
	check (system (cmd) == 0, "tar command succeeded");
	g_free (cmd);

	input = gsf_input_stdio_new (archive, &err);
	check (input != NULL, "the archive opens as an input");
	if (err != NULL) {
		g_printerr ("archive error: %s\n", err->message);
		g_error_free (err);
	}
	tar = (input != NULL) ? gsf_infile_tar_new (input, NULL) : NULL;
	check (tar != NULL, "the real tar archive parses");
	if (tar != NULL) {
		guint8 buf[16];
		guint8 const *r;

		check (gsf_infile_num_children (tar) >= 2,
		       "the archive has the expected top-level children");

		child = gsf_infile_child_by_name (tar, "small.txt");
		check (child != NULL, "small.txt found");
		if (child != NULL) {
			check (gsf_input_size (child) == 9, "small.txt size");
			r = gsf_input_read (child, 9, buf);
			check (r != NULL && memcmp (r, "tar small", 9) == 0,
			       "small.txt content matches");
			g_object_unref (child);
		}

		child = gsf_infile_child_by_vname
			(tar, "sub", "nested.txt", NULL);
		check (child != NULL, "sub/nested.txt found");
		if (child != NULL) {
			check (gsf_input_size (child) == 10, "nested.txt size");
			r = gsf_input_read (child, 10, buf);
			check (r != NULL && memcmp (r, "tar nested", 10) == 0,
			       "nested.txt content matches");
			g_object_unref (child);
		}

		child = gsf_infile_child_by_name
			(tar,
			 "L012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789");
		check (child != NULL, "the long-named file is found");
		if (child != NULL) {
			check (gsf_input_size (child) == 8, "long-named file size");
			r = gsf_input_read (child, 8, buf);
			check (r != NULL && memcmp (r, "tar long", 8) == 0,
			       "long-named file content matches");
			g_object_unref (child);
		}

		g_object_unref (tar);
	}
	if (input != NULL)
		g_object_unref (input);

	g_unlink (small);
	g_unlink (nested);
	g_unlink (longname_path);
	g_rmdir (sub);
	g_rmdir (dir);
	g_unlink (archive);
	g_free (small);
	g_free (sub);
	g_free (nested);
	g_free (longname_path);
	g_free (archive);
	g_free (dir);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_tar_dup ();
	test_basic_listing ();
	test_child_content ();
	test_real_tar ();

	gsf_shutdown ();
	return regress_exit_status ();
}
