/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-blob.c: regression tests for gsf-structured-blob.c
 */

#include <gsf/gsf.h>
#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf-outfile-zip.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-structured-blob.h>

#include <signal.h>
#include "libgsf-regress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
on_alarm (int sig G_GNUC_UNUSED)
{
	/* The child_by_name loop never advances; abort instead of spinning
	 * forever.  stderr is unbuffered, so the message survives the
	 * _exit(). */
	fprintf (stderr, "FAIL: gsf_infile_child_by_name did not terminate\n");
	_exit (1);
}

/*
 * A blob read from a plain input carries its content as its own stream.
 * gsf_structured_blob_read() never set the blob's size, so every read
 * through the GsfInput API was rejected and dup could not work either.
 * (A zip infile reports no bytes of its own, so a plain input is used for
 * the content checks.)
 */
static void
test_content_stream (void)
{
	GsfInput *plain = gsf_input_memory_new_clone
		((guint8 const *)"hello world", 11);
	GsfStructuredBlob *blob = gsf_structured_blob_read (plain);

	check (blob != NULL, "blob read");
	if (blob != NULL) {
		guint8 buf[11];
		guint8 const *r;
		GsfInput *dup;

		check (gsf_input_size (GSF_INPUT (blob)) == 11,
		       "blob size set from its content");
		r = gsf_input_read (GSF_INPUT (blob), 11, buf);
		check (r != NULL && memcmp (r, "hello world", 11) == 0,
		       "blob content readable");
		dup = gsf_input_dup (GSF_INPUT (blob), NULL);
		check (dup != NULL, "blob dup works");
		if (dup != NULL) {
			check (gsf_input_size (dup) == 11, "blob dup size");
			g_object_unref (dup);
		}
		g_object_unref (blob);
	}
	g_object_unref (plain);
}

/*
 * Child lookup on a blob read from a GsfInfile.  The index bounds checks in
 * blob_name_by_index()/blob_child_by_index() were inverted: valid indices
 * were rejected with a critical (and invalid indices would pass the check
 * and index out of bounds).  blob_child_by_name()'s search loop never
 * incremented its index, so looking up a child that is not the first one
 * spun forever -- guarded here by a SIGALRM that aborts the process if the
 * lookup does not finish.
 */
static void
test_child_lookup (void)
{
	GsfInput *zipin = regress_make_zip_input ();
	GsfInfile *zip = gsf_infile_zip_new (zipin, NULL);
	GsfStructuredBlob *blob;
	GsfInput *child;

	check (zip != NULL, "zip opened");
	if (zip == NULL) {
		g_object_unref (zipin);
		return;
	}

	blob = gsf_structured_blob_read (GSF_INPUT (zip));
	check (blob != NULL, "blob read");
	if (blob == NULL) {
		g_object_unref (zip);
		g_object_unref (zipin);
		return;
	}

	check (NULL != gsf_infile_name_by_index (GSF_INFILE (blob), 0),
	       "name_by_index(0) on a valid index");
	child = gsf_infile_child_by_index (GSF_INFILE (blob), 0);
	check (child != NULL, "child_by_index(0) on a valid index");
	if (child != NULL)
		g_object_unref (child);

	fflush (stdout);
	signal (SIGALRM, on_alarm);
	alarm (10);
	child = gsf_infile_child_by_name (GSF_INFILE (blob), "bbb");
	alarm (0);
	check (child != NULL, "child_by_name(bbb) found");
	if (child != NULL)
		g_object_unref (child);

	/* a name that does not exist must return NULL (and terminate) */
	child = gsf_infile_child_by_name (GSF_INFILE (blob), "zzz");
	check (child == NULL, "child_by_name of a missing name fails cleanly");
	if (child != NULL)
		g_object_unref (child);

	g_object_unref (blob);
	g_object_unref (zip);
	g_object_unref (zipin);
}

/*
 * An out-of-range index returns NULL silently, matching the convention
 * of the other GsfInfile implementations (zip, tar, stdio, msole).
 * Before the fix the check was inverted (valid indices were rejected
 * and invalid indices passed through, indexing out of bounds), and it
 * used g_return_val_if_fail which emitted a GLib critical instead of
 * returning NULL quietly.
 */
static void
test_child_index_bounds (void)
{
	GsfInput *zipin = regress_make_zip_input ();
	GsfInfile *zip = gsf_infile_zip_new (zipin, NULL);
	GsfStructuredBlob *blob;
	int n;

	check (zip != NULL, "zip opened");
	if (zip == NULL) {
		g_object_unref (zipin);
		return;
	}

	blob = gsf_structured_blob_read (GSF_INPUT (zip));
	check (blob != NULL, "blob read");
	if (blob == NULL) {
		g_object_unref (zip);
		g_object_unref (zipin);
		return;
	}

	n = gsf_infile_num_children (GSF_INFILE (blob));
	check (n == 2, "the blob has two children");

	check (NULL == gsf_infile_name_by_index (GSF_INFILE (blob), -1),
	       "name_by_index(-1) rejected");
	check (NULL == gsf_infile_name_by_index (GSF_INFILE (blob), n),
	       "name_by_index(len) rejected");
	check (NULL == gsf_infile_child_by_index (GSF_INFILE (blob), -1),
	       "child_by_index(-1) rejected");
	check (NULL == gsf_infile_child_by_index (GSF_INFILE (blob), n),
	       "child_by_index(len) rejected");

	g_object_unref (blob);
	g_object_unref (zip);
	g_object_unref (zipin);
}

/*
 * Seeking within a blob's content must reposition reads; seeking outside
 * the content must fail.  blob_seek() always returned an error before the
 * fix, which also made gsf_input_dup() unable to restore the position.
 */
static void
test_seek (void)
{
	GsfInput *plain = gsf_input_memory_new_clone
		((guint8 const *)"hello world", 11);
	GsfStructuredBlob *blob = gsf_structured_blob_read (plain);

	check (blob != NULL, "blob read");
	if (blob != NULL) {
		guint8 buf[6];
		guint8 const *r;

		/* gsf_input_seek returns TRUE on error. */
		check (!gsf_input_seek (GSF_INPUT (blob), 6, G_SEEK_SET),
		       "seek inside the content");
		check (gsf_input_tell (GSF_INPUT (blob)) == 6,
		       "position is 6");
		r = gsf_input_read (GSF_INPUT (blob), 5, buf);
		check (r != NULL && memcmp (r, "world", 5) == 0,
		       "read continues at the seek position");
		check (gsf_input_seek (GSF_INPUT (blob), 12, G_SEEK_SET),
		       "seek past the end fails");
		g_object_unref (blob);
	}
	g_object_unref (plain);
}

/*
 * A blob with no content and no children reports size 0 and no children,
 * and every lookup fails cleanly (no hang, no crash).
 */
static void
test_empty_blob (void)
{
	GsfInput *plain = gsf_input_memory_new_clone ((guint8 const *)"", 0);
	GsfStructuredBlob *blob = gsf_structured_blob_read (plain);
	GsfInput *child;

	check (blob != NULL, "empty blob read");
	if (blob != NULL) {
		check (gsf_input_size (GSF_INPUT (blob)) == 0,
		       "empty blob has size 0");
		check (gsf_infile_num_children (GSF_INFILE (blob)) == -1,
		       "childless blob reports -1 children");
		child = gsf_infile_child_by_name (GSF_INFILE (blob), "anything");
		check (child == NULL, "child lookup on a childless blob fails");
		if (child != NULL)
			g_object_unref (child);
		g_object_unref (blob);
	}
	g_object_unref (plain);
}

/*
 * gsf_structured_blob_write() must write a blob -- its content and, for
 * an infile-sourced blob, its whole child tree -- into an outfile so that
 * it can be read back.
 */
static void
test_blob_write_roundtrip (void)
{
	/* content-only blob with a name */
	{
		GsfInput *in = gsf_input_memory_new_clone
			((guint8 const *)"payload-data", 12);
		GsfStructuredBlob *blob;
		GsfOutput *out;
		GsfOutfile *zout;
		guint8 const *bytes;
		GsfInput *in2;

		gsf_input_set_name (in, "payload");
		blob = gsf_structured_blob_read (in);
		check (blob != NULL, "payload blob read");

		out = gsf_output_memory_new ();
		zout = gsf_outfile_zip_new (out, NULL);
		check (zout != NULL, "zip outfile created");
		if (zout != NULL && blob != NULL) {
			check (gsf_structured_blob_write (blob, zout),
			       "blob written");
		}
		gsf_output_close (GSF_OUTPUT (zout));
		g_object_unref (zout);
		if (blob != NULL)
			g_object_unref (blob);
		g_object_unref (in);

		bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (out));
		in2 = gsf_input_memory_new_clone (bytes, gsf_output_size (out));
		g_object_unref (out);
		{
			GsfInfile *zin = (in2 != NULL)
				? gsf_infile_zip_new (in2, NULL) : NULL;
			GsfInput *child = (zin != NULL)
				? gsf_infile_child_by_name (zin, "payload") : NULL;
			guint8 buf[12];
			guint8 const *r = (child != NULL)
				? gsf_input_read (child, 12, buf) : NULL;
			check (child != NULL, "written blob readable");
			check (r != NULL && memcmp (r, "payload-data", 12) == 0,
			       "written blob content matches");
			if (child != NULL)
				g_object_unref (child);
			if (zin != NULL)
				g_object_unref (zin);
		}
		g_object_unref (in2);
	}

	/* a blob carrying a child tree */
	{
		GsfInput *zipin = regress_make_zip_input ();
		GsfInfile *zip = gsf_infile_zip_new (zipin, NULL);
		GsfStructuredBlob *blob;
		GsfOutput *out;
		GsfOutfile *zout;
		guint8 const *bytes;
		GsfInput *in2;

		check (zip != NULL, "zip opened");
		if (zip == NULL) {
			g_object_unref (zipin);
			return;
		}
		gsf_input_set_name (GSF_INPUT (zip), "tree");
		blob = gsf_structured_blob_read (GSF_INPUT (zip));
		check (blob != NULL, "tree blob read");

		out = gsf_output_memory_new ();
		zout = gsf_outfile_zip_new (out, NULL);
		if (zout != NULL && blob != NULL)
			check (gsf_structured_blob_write (blob, zout),
			       "tree blob written");
		gsf_output_close (GSF_OUTPUT (zout));
		g_object_unref (zout);
		if (blob != NULL)
			g_object_unref (blob);
		g_object_unref (zip);
		g_object_unref (zipin);

		bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (out));
		in2 = gsf_input_memory_new_clone (bytes, gsf_output_size (out));
		g_object_unref (out);
		{
			GsfInfile *zin = (in2 != NULL)
				? gsf_infile_zip_new (in2, NULL) : NULL;
			GsfInput *tree = (zin != NULL)
				? gsf_infile_child_by_name (zin, "tree") : NULL;
			GsfInput *leaf = (tree != NULL)
				? gsf_infile_child_by_name (GSF_INFILE (tree), "aaa")
				: NULL;
			guint8 buf[5];
			guint8 const *r = (leaf != NULL)
				? gsf_input_read (leaf, 5, buf) : NULL;
			check (tree != NULL, "tree child written");
			check (leaf != NULL && gsf_input_size (leaf) == 5,
			       "leaf child written");
			check (r != NULL && memcmp (r, "hello", 5) == 0,
			       "leaf content matches");
			if (leaf != NULL)
				g_object_unref (leaf);
			if (tree != NULL)
				g_object_unref (tree);
			if (zin != NULL)
				g_object_unref (zin);
		}
		g_object_unref (in2);
	}
}

/*
 * A blob tree with a grandchild (depth > 2) must survive the write and
 * read-back cycle: the tree is a/b/c where each level has content.
 */
static void
test_blob_deep_tree (void)
{
	/* Build a zip with a nested path a/b/c.  */
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *zip = gsf_outfile_zip_new (mem, NULL);
	GsfOutput *child;
	GsfInput *in;
	GsfStructuredBlob *blob;
	GsfOutfile *zout;
	guint8 const *bytes;
	GsfInput *in2;
	GsfInfile *zin;
	GsfInput *a, *b, *c;

	check (zip != NULL, "zip outfile created");
	if (zip == NULL) {
		g_object_unref (mem);
		return;
	}
	child = gsf_outfile_new_child (zip, "a", TRUE);
	gsf_output_close (child);
	g_object_unref (child);
	child = gsf_outfile_new_child (zip, "a/b", TRUE);
	gsf_output_close (child);
	g_object_unref (child);
	child = gsf_outfile_new_child (zip, "a/b/c", FALSE);
	gsf_output_write (child, 3, (guint8 const *)"abc");
	gsf_output_close (child);
	g_object_unref (child);
	gsf_output_close (GSF_OUTPUT (zip));
	g_object_unref (zip);

	{
		guint8 const *raw = gsf_output_memory_get_bytes
			(GSF_OUTPUT_MEMORY (mem));
		in = gsf_input_memory_new_clone (raw, gsf_output_size (mem));
	}
	g_object_unref (mem);

	{
		GsfInfile *zipin = gsf_infile_zip_new (in, NULL);
		check (zipin != NULL, "zip infile created");
		if (zipin == NULL) {
			g_object_unref (in);
			return;
		}
		gsf_input_set_name (GSF_INPUT (zipin), "deep");
		blob = gsf_structured_blob_read (GSF_INPUT (zipin));
		check (blob != NULL, "deep tree blob read");
		g_object_unref (zipin);
	}

	/* Write the blob tree to a new zip archive.  */
	mem = gsf_output_memory_new ();
	zout = gsf_outfile_zip_new (mem, NULL);
	if (zout != NULL && blob != NULL)
		check (gsf_structured_blob_write (blob, zout),
		       "deep tree blob written");
	gsf_output_close (GSF_OUTPUT (zout));
	g_object_unref (zout);
	if (blob != NULL)
		g_object_unref (blob);
	g_object_unref (in);

	bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
	in2 = gsf_input_memory_new_clone (bytes, gsf_output_size (mem));
	g_object_unref (mem);

	zin = (in2 != NULL) ? gsf_infile_zip_new (in2, NULL) : NULL;
	check (zin != NULL, "zip infile for the written tree");
	a = (zin != NULL) ? gsf_infile_child_by_name (zin, "deep") : NULL;
	check (a != NULL, "top-level 'deep' found");
	if (a != NULL) {
		check (GSF_IS_INFILE (a), "'deep' is a directory");
		b = GSF_IS_INFILE (a)
			? gsf_infile_child_by_name (GSF_INFILE (a), "a") : NULL;
		check (b != NULL, "second level 'a' found");
		if (b != NULL) {
			check (GSF_IS_INFILE (b), "'a' is a directory");
			c = GSF_IS_INFILE (b)
				? gsf_infile_child_by_name (GSF_INFILE (b), "b") : NULL;
			check (c != NULL, "third level 'b' found");
			if (c != NULL) {
				check (GSF_IS_INFILE (c), "'b' is a directory");
				{
					GsfInput *leaf = gsf_infile_child_by_name
						(GSF_INFILE (c), "c");
					check (leaf != NULL, "leaf 'c' found");
					if (leaf != NULL) {
						guint8 buf[3];
						guint8 const *r = gsf_input_read (leaf, 3, buf);
						check (gsf_input_size (leaf) == 3,
						       "leaf size is 3");
						check (r != NULL &&
						       memcmp (r, "abc", 3) == 0,
						       "leaf content matches");
						g_object_unref (leaf);
					}
				}
				g_object_unref (c);
			}
			g_object_unref (b);
		}
		g_object_unref (a);
	}
	if (zin != NULL)
		g_object_unref (zin);
	g_object_unref (in2);
}

/*
 * A blob read from a tar infile (a different container format) produces
 * the same tree structure as from a zip.
 */
static void
test_blob_from_tar (void)
{
	GsfInput *tarin = regress_make_tar_input ();
	GsfInfile *tar = gsf_infile_tar_new (tarin, NULL);
	GsfStructuredBlob *blob;
	int n;

	check (tar != NULL, "tar opened");
	if (tar == NULL) {
		g_object_unref (tarin);
		return;
	}

	gsf_input_set_name (GSF_INPUT (tar), "from-tar");
	blob = gsf_structured_blob_read (GSF_INPUT (tar));
	check (blob != NULL, "blob from tar read");

	n = (blob != NULL) ? gsf_infile_num_children (GSF_INFILE (blob)) : -1;
	check (n == 2, "the blob from tar has two children");
	check (n >= 1 &&
	       0 == strcmp (gsf_infile_name_by_index (GSF_INFILE (blob), 0),
			    "aaa"),
	       "first child is 'aaa'");
	check (n >= 2 &&
	       0 == strcmp (gsf_infile_name_by_index (GSF_INFILE (blob), 1),
			    "bbb"),
	       "second child is 'bbb'");

	if (blob != NULL)
		g_object_unref (blob);
	g_object_unref (tar);
	g_object_unref (tarin);
}

/*
 * Seek END positions the read at the end of the blob content, so a
 * subsequent read fails (nothing left).
 */
static void
test_seek_end (void)
{
	GsfInput *plain = gsf_input_memory_new_clone
		((guint8 const *)"hello world", 11);
	GsfStructuredBlob *blob = gsf_structured_blob_read (plain);
	guint8 buf[4];

	check (blob != NULL, "blob read");
	if (blob != NULL) {
		guint8 const *r;
		check (!gsf_input_seek (GSF_INPUT (blob), 0, G_SEEK_END),
		       "seek to end succeeds");
		check (gsf_input_tell (GSF_INPUT (blob)) == 11,
		       "position is at the end");
		r = gsf_input_read (GSF_INPUT (blob), 1, buf);
		check (r == NULL, "read at end returns NULL");
		g_object_unref (blob);
	}
	g_object_unref (plain);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_content_stream ();
	test_child_lookup ();
	test_child_index_bounds ();
	test_seek ();
	test_empty_blob ();
	test_blob_write_roundtrip ();
	test_blob_deep_tree ();
	test_blob_from_tar ();
	test_seek_end ();

	gsf_shutdown ();
	return regress_exit_status ();
}
