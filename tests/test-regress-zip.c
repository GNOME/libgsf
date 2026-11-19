/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-zip.c: zip-focused regression tests
 */

#include "libgsf-regress.h"

#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf-outfile-zip.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-input-memory.h>

#include <stdio.h>
#include <stdlib.h>

/*
 * Basic member round trip: a normal member, a zero-byte member and a
 * member with a non-ASCII (UTF-8) name must all survive the archive
 * round trip with their sizes and contents intact.
 */
static void
test_roundtrip_members (void)
{
	static guint8 const content[] = "member content";
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *zip = gsf_outfile_zip_new (mem, NULL);
	GsfOutput *child;
	GsfInput *in;
	GsfInfile *zin;
	GsfInput *c;

	check (zip != NULL, "zip outfile created");
	if (zip == NULL) {
		g_object_unref (mem);
		return;
	}

	child = gsf_outfile_new_child (zip, "one", FALSE);
	gsf_output_write (child, sizeof content - 1, content);
	gsf_output_close (child);
	g_object_unref (child);

	child = gsf_outfile_new_child (zip, "empty", FALSE);
	gsf_output_close (child);
	g_object_unref (child);

	child = gsf_outfile_new_child (zip, "na\xC3\xAFve.txt", FALSE);
	gsf_output_write (child, 4, (guint8 const *)"data");
	gsf_output_close (child);
	g_object_unref (child);

	gsf_output_close (GSF_OUTPUT (zip));
	g_object_unref (zip);

	{
		guint8 const *bytes =
			gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
		in = gsf_input_memory_new_clone (bytes, gsf_output_size (mem));
	}
	g_object_unref (mem);

	zin = gsf_infile_zip_new (in, NULL);
	check (zin != NULL, "zip infile created");
	if (zin == NULL) {
		g_object_unref (in);
		return;
	}
	check (gsf_infile_num_children (zin) == 3, "three members");

	c = gsf_infile_child_by_name (zin, "one");
	check (c != NULL, "member 'one' found");
	if (c != NULL) {
		guint8 buf[16];
		guint8 const *r;
		check (gsf_input_size (c) == sizeof content - 1,
		       "member 'one' has the right size");
		r = gsf_input_read (c, sizeof content - 1, buf);
		check (r != NULL && memcmp (r, content, sizeof content - 1) == 0,
		       "member 'one' content matches");
		g_object_unref (c);
	}

	c = gsf_infile_child_by_name (zin, "empty");
	check (c != NULL, "member 'empty' found");
	if (c != NULL) {
		guint8 buf[1];
		check (gsf_input_size (c) == 0, "member 'empty' has size 0");
		check (gsf_input_read (c, 1, buf) == NULL,
		       "reading an empty member fails");
		g_object_unref (c);
	}

	c = gsf_infile_child_by_name (zin, "na\xC3\xAFve.txt");
	check (c != NULL, "the UTF-8 named member is found");
	if (c != NULL) {
		guint8 buf[4];
		guint8 const *r;
		check (gsf_input_size (c) == 4, "the UTF-8 named member size");
		r = gsf_input_read (c, 4, buf);
		check (r != NULL && memcmp (r, "data", 4) == 0,
		       "the UTF-8 named member content matches");
		g_object_unref (c);
	}

	g_object_unref (zin);
	g_object_unref (in);
}

/*
 * A member much larger than the internal compression buffer (32 KB) must
 * round trip exactly.
 */
static void
test_large_member (void)
{
	guint8 *pattern = g_new (guint8, 100000);
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *zip = gsf_outfile_zip_new (mem, NULL);
	GsfOutput *child;
	GsfInput *in, *c;
	guint8 const *r;

	regress_fill_pattern (pattern, 100000);

	check (zip != NULL, "zip outfile created");
	if (zip == NULL) {
		g_free (pattern);
		g_object_unref (mem);
		return;
	}
	child = gsf_outfile_new_child (zip, "large", FALSE);
	gsf_output_write (child, 100000, pattern);
	gsf_output_close (child);
	g_object_unref (child);
	gsf_output_close (GSF_OUTPUT (zip));
	g_object_unref (zip);

	{
		guint8 const *bytes =
			gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
		in = gsf_input_memory_new_clone (bytes, gsf_output_size (mem));
	}
	g_object_unref (mem);

	{
		GsfInfile *zin = (in != NULL)
			? gsf_infile_zip_new (in, NULL) : NULL;
		c = (zin != NULL)
			? gsf_infile_child_by_name (zin, "large") : NULL;
		check (c != NULL, "the large member is found");
		if (c != NULL) {
			unsigned i;
			gboolean ok = gsf_input_size (c) == 100000;
			for (i = 0; ok && i < 100000; i += 8192) {
				size_t chunk = (100000 - i > 8192) ? 8192 : (100000 - i);
				r = gsf_input_read (c, chunk, NULL);
				if (r == NULL || memcmp (r, pattern + i, chunk) != 0)
					ok = FALSE;
			}
			check (ok, "the large member round-trips");
			g_object_unref (c);
		}
		if (zin != NULL)
			g_object_unref (zin);
	}
	g_object_unref (in);
	g_free (pattern);
}

/*
 * Seeking within a member repositions reads, exactly as for any input.
 */
static void
test_member_seek (void)
{
	GsfInput *in = regress_make_zip_input ();
	GsfInfile *zin = (in != NULL) ? gsf_infile_zip_new (in, NULL) : NULL;
	GsfInput *c = (zin != NULL)
		? gsf_infile_child_by_name (zin, "bbb") : NULL;
	guint8 buf[8];
	guint8 const *r;

	check (c != NULL, "member found");
	if (c == NULL) {
		if (zin != NULL)
			g_object_unref (zin);
		g_object_unref (in);
		return;
	}
	r = gsf_input_read (c, 2, buf);
	check (r != NULL && memcmp (r, "wo", 2) == 0, "initial read");
	check (!gsf_input_seek (c, 0, G_SEEK_SET), "seek to the start");
	r = gsf_input_read (c, 5, buf);
	check (r != NULL && memcmp (r, "world", 5) == 0, "read after seek");
	check (!gsf_input_seek (c, 4, G_SEEK_SET), "seek near the end");
	r = gsf_input_read (c, 1, buf);
	check (r != NULL && buf[0] == 'd', "single byte at the offset");

	g_object_unref (c);
	g_object_unref (zin);
	g_object_unref (in);
}

/*
 * A directory member round-trips as a directory with no children, and
 * lives alongside regular members in the same archive.
 */
static void
test_directory_member (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *zip = gsf_outfile_zip_new (mem, NULL);
	GsfOutput *child;
	GsfInput *in, *dir, *c;
	GsfInfile *zin;

	check (zip != NULL, "zip outfile created");
	if (zip == NULL) {
		g_object_unref (mem);
		return;
	}
	child = gsf_outfile_new_child (zip, "docs", TRUE);
	gsf_output_close (child);
	g_object_unref (child);
	child = gsf_outfile_new_child (zip, "docs/readme", FALSE);
	gsf_output_write (child, 2, (guint8 const *)"hi");
	gsf_output_close (child);
	g_object_unref (child);
	gsf_output_close (GSF_OUTPUT (zip));
	g_object_unref (zip);

	{
		guint8 const *bytes =
			gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
		in = gsf_input_memory_new_clone (bytes, gsf_output_size (mem));
	}
	g_object_unref (mem);

	zin = (in != NULL) ? gsf_infile_zip_new (in, NULL) : NULL;
	check (zin != NULL, "zip infile created");
	dir = (zin != NULL) ? gsf_infile_child_by_name (zin, "docs") : NULL;
	check (dir != NULL, "the directory member is found");
	if (dir != NULL) {
		check (GSF_IS_INFILE (dir), "the directory member is an infile");
		check (GSF_IS_INFILE (dir) &&
		       gsf_infile_num_children (GSF_INFILE (dir)) == 1,
		       "the directory has the nested member");
		g_object_unref (dir);
	}
	c = (zin != NULL)
		? gsf_infile_child_by_vname (zin, "docs", "readme", NULL)
		: NULL;
	check (c != NULL, "the nested member is reachable");
	if (c != NULL)
		g_object_unref (c);
	if (zin != NULL)
		g_object_unref (zin);
	g_object_unref (in);
}

/*
 * The deflate-level property is honoured for every legal value, and
 * maximum compression produces no larger an archive than level 0 for a
 * highly compressible payload.
 */
static void
test_deflate_levels (void)
{
	static int const levels[] = { -1, 0, 1, 9 };
	gsf_off_t sizes[G_N_ELEMENTS (levels)];
	GsfOutput *mems[G_N_ELEMENTS (levels)];
	unsigned i;

	for (i = 0; i < G_N_ELEMENTS (levels); i++) {
		GsfOutfile *zip;
		GsfOutput *child;
		GsfInput *in;
		GsfInput *c;
		guint8 buf[16];
		guint8 const *r;
		char name[16];

		mems[i] = gsf_output_memory_new ();
		zip = gsf_outfile_zip_new (mems[i], NULL);
		check (zip != NULL, "zip outfile created");
		if (zip == NULL)
			continue;
		sprintf (name, "m%u", i);
		child = gsf_outfile_new_child_full (zip, name, FALSE,
						    "deflate-level",
						    levels[i], NULL);
		check (child != NULL, "child created with deflate-level %d",
		       levels[i]);
		if (child != NULL) {
			gsf_output_write (child, 5,
					  (guint8 const *)"AAAAA");
			gsf_output_close (child);
			g_object_unref (child);
		}
		gsf_output_close (GSF_OUTPUT (zip));
		g_object_unref (zip);

		{
			guint8 const *bytes = gsf_output_memory_get_bytes
				(GSF_OUTPUT_MEMORY (mems[i]));
			in = gsf_input_memory_new_clone
				(bytes, gsf_output_size (mems[i]));
		}
		sizes[i] = gsf_output_size (mems[i]);

		{
			GsfInfile *zin = (in != NULL)
				? gsf_infile_zip_new (in, NULL) : NULL;
			c = (zin != NULL)
				? gsf_infile_child_by_name (zin, name) : NULL;
			check (c != NULL, "member with deflate-level %d found",
			       levels[i]);
			if (c != NULL) {
				check (gsf_input_size (c) == 5, "content size is 5");
				r = gsf_input_read (c, 5, buf);
				check (r != NULL && memcmp (r, "AAAAA", 5) == 0,
				       "content matches at level %d", levels[i]);
				g_object_unref (c);
			}
			if (zin != NULL)
				g_object_unref (zin);
		}
		g_object_unref (in);
	}

	check (sizes[2] >= sizes[3],
	       "level 1 compresses no better than level 9");
	check (sizes[1] >= sizes[3],
	       "level 0 compresses no better than level 9");

	for (i = 0; i < G_N_ELEMENTS (levels); i++)
		g_object_unref (mems[i]);
}

/*
 * A member's modification time survives the round trip at the minute
 * granularity of the DOS timestamp in the zip directory.
 */
static void
test_modtime_roundtrip (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *zip = gsf_outfile_zip_new (mem, NULL);
	GsfOutput *child;
	GDateTime *dt = g_date_time_new_utc (2015, 6, 1, 10, 30, 0);
	GsfInput *in, *c;
	GDateTime *got;

	check (zip != NULL, "zip outfile created");
	if (zip == NULL) {
		g_date_time_unref (dt);
		g_object_unref (mem);
		return;
	}
	child = gsf_outfile_new_child (zip, "dated", FALSE);
	gsf_output_set_modtime (child, dt);
	gsf_output_close (child);
	g_object_unref (child);
	gsf_output_close (GSF_OUTPUT (zip));
	g_object_unref (zip);

	{
		guint8 const *bytes =
			gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
		in = gsf_input_memory_new_clone (bytes, gsf_output_size (mem));
	}
	g_object_unref (mem);

	{
		GsfInfile *zin = (in != NULL)
			? gsf_infile_zip_new (in, NULL) : NULL;
		c = (zin != NULL)
			? gsf_infile_child_by_name (zin, "dated") : NULL;
		check (c != NULL, "the dated member is found");
		if (c != NULL) {
			got = gsf_input_get_modtime (c);
			check (got != NULL, "the member has a modtime");
			check (got != NULL &&
			       g_date_time_get_year (got) == 2015 &&
			       g_date_time_get_month (got) == 6 &&
			       g_date_time_get_day_of_month (got) == 1 &&
			       g_date_time_get_hour (got) == 10 &&
			       g_date_time_get_minute (got) == 30,
			       "the modtime survives the round trip");
			g_object_unref (c);
		}
		if (zin != NULL)
			g_object_unref (zin);
	}
	g_date_time_unref (dt);
	g_object_unref (in);
}

/*
 * Duplicate member names are preserved: the archive keeps both members,
 * child_by_index reaches both, and child_by_name returns the first.
 */
static void
test_duplicate_names (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *zip = gsf_outfile_zip_new (mem, NULL);
	GsfOutput *child;
	GsfInput *in, *c;
	GsfInfile *zin;
	unsigned i;

	check (zip != NULL, "zip outfile created");
	if (zip == NULL) {
		g_object_unref (mem);
		return;
	}
	for (i = 0; i < 2; i++) {
		child = gsf_outfile_new_child (zip, "dup", FALSE);
		gsf_output_write (child, 1, (guint8 const *)(i ? "2" : "1"));
		gsf_output_close (child);
		g_object_unref (child);
	}
	gsf_output_close (GSF_OUTPUT (zip));
	g_object_unref (zip);

	{
		guint8 const *bytes =
			gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
		in = gsf_input_memory_new_clone (bytes, gsf_output_size (mem));
	}
	g_object_unref (mem);

	zin = (in != NULL) ? gsf_infile_zip_new (in, NULL) : NULL;
	check (zin != NULL, "zip infile created");
	if (zin == NULL) {
		g_object_unref (in);
		return;
	}
	check (gsf_infile_num_children (zin) == 2,
	       "both duplicate members survive");

	for (i = 0; i < 2; i++) {
		c = gsf_infile_child_by_index (zin, (int)i);
		check (c != NULL, "child %u by index", i);
		if (c != NULL) {
			check (0 == strcmp (gsf_input_name (c), "dup"),
			       "child %u is named 'dup'", i);
			check (gsf_input_size (c) == 1, "child %u has size 1", i);
			g_object_unref (c);
		}
	}

	c = gsf_infile_child_by_name (zin, "dup");
	check (c != NULL, "child_by_name finds one of them");
	if (c != NULL) {
		guint8 buf[1];
		check (gsf_input_read (c, 1, buf) != NULL && buf[0] == '1',
		       "child_by_name returns the first duplicate");
		g_object_unref (c);
	}

	g_object_unref (zin);
	g_object_unref (in);
}

/*
 * The end-of-central-directory record may be followed by a comment of up to
 * 65535 bytes, so it can start 65557 bytes before EOF.  The backward scan
 * used to give up too early and rejected such archives.
 */
static void
test_max_comment (void)
{
	static guint const comment_lens[] = { 0, 1000, 65514, 65515, 65535 };
	guint k;

	for (k = 0; k < G_N_ELEMENTS (comment_lens); k++) {
		GsfInput *base = regress_make_zip_input ();
		gsf_off_t n = gsf_input_size (base);
		guint clen = comment_lens[k];
		guint8 *buf = g_malloc0 (n + clen);
		GsfInput *in;
		GsfInfile *zip;

		g_assert (gsf_input_read (base, n, buf) != NULL);
		/* comment length is the last 2 bytes of the 22-byte EOCD */
		buf[n - 2] = clen & 0xff;
		buf[n - 1] = clen >> 8;
		memset (buf + n, 'x', clen);

		in = gsf_input_memory_new_clone (buf, n + clen);
		zip = gsf_infile_zip_new (in, NULL);
		check (zip != NULL, "zip with a %u byte comment opens", clen);
		if (zip) {
			check (gsf_infile_num_children (zip) == 2,
			       "members found with a %u byte comment", clen);
			g_object_unref (zip);
		}
		g_object_unref (in);
		g_object_unref (base);
		g_free (buf);
	}
}

/*
 * The "compression-level" getter read the method from the member's zip
 * directory entry, which does not exist until the member is written (and
 * never does for the root), so it reported 0 (stored) for an outfile that
 * was in fact configured for deflate.  It must report the configured
 * method.
 */
static void
test_compression_method_getter (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *root = gsf_outfile_zip_new (mem, NULL);
	GsfOutput *stored_mem = gsf_output_memory_new ();
	GsfOutfile *stored_root = g_object_new (GSF_OUTFILE_ZIP_TYPE,
						"sink", stored_mem,
						"compression-level", GSF_ZIP_STORED,
						NULL);
	GsfOutput *child;
	int m = -1;

	g_object_get (root, "compression-level", &m, NULL);
	check (m == GSF_ZIP_DEFLATED, "default root reports deflate (got %d)", m);

	child = gsf_outfile_new_child (root, "member", FALSE);
	m = -1;
	g_object_get (child, "compression-level", &m, NULL);
	check (m == GSF_ZIP_DEFLATED,
	       "member that has not been written yet reports deflate (got %d)", m);

	m = -1;
	g_object_get (stored_root, "compression-level", &m, NULL);
	check (m == GSF_ZIP_STORED, "stored root reports stored (got %d)", m);

	gsf_output_close (child);
	g_object_unref (child);
	gsf_output_close (GSF_OUTPUT (root));
	g_object_unref (root);
	gsf_output_close (GSF_OUTPUT (stored_root));
	g_object_unref (stored_root);
	g_object_unref (mem);
	g_object_unref (stored_mem);
}

/* A sink whose writes all fail (seeking and closing succeed). */
typedef struct { GsfOutput base; } FailOut;
typedef struct { GsfOutputClass base; } FailOutClass;

static gboolean
fo_write (G_GNUC_UNUSED GsfOutput *o, G_GNUC_UNUSED size_t n,
	  G_GNUC_UNUSED guint8 const *d)
{
	return FALSE;
}

static gboolean
fo_seek (G_GNUC_UNUSED GsfOutput *o, G_GNUC_UNUSED gsf_off_t off,
	 G_GNUC_UNUSED GSeekType w)
{
	return TRUE;
}

static gboolean
fo_close (G_GNUC_UNUSED GsfOutput *o)
{
	return TRUE;
}

static void
fo_class_init (gpointer k, G_GNUC_UNUSED gpointer d)
{
	GsfOutputClass *c = k;
	c->Write = fo_write;
	c->Seek = fo_seek;
	c->Close = fo_close;
}

static GType
fo_get_type (void)
{
	static GType t;
	if (!t) {
		GTypeInfo i = { sizeof (FailOutClass), NULL, NULL, fo_class_init,
				NULL, NULL, sizeof (FailOut), 0, NULL, NULL };
		t = g_type_register_static (GSF_OUTPUT_TYPE, "RegressZipFailOut", &i, 0);
	}
	return t;
}

/*
 * zip_init_write ignored the result of writing the local file header.  If
 * the sink rejected it, the member was nevertheless marked as being
 * written, and the data that followed went to the sink with no header
 * (and the later size patch overwrote the start of that data).  The first
 * write to the member must fail instead.
 */
static void
test_header_write_failure (void)
{
	GsfOutput *sink = g_object_new (fo_get_type (), NULL);
	GsfOutfile *root = gsf_outfile_zip_new (sink, NULL);
	GsfOutput *child = gsf_outfile_new_child (root, "member", FALSE);
	gboolean ok;

	inhibit_g_log_messages ();
	ok = gsf_output_write (child, 5, (guint8 const *)"hello");
	check (!ok, "first write fails when the local header cannot be written");

	/* Tearing down after the failure must not crash or leak.  */
	gsf_output_close (child);
	g_object_unref (child);
	gsf_output_close (GSF_OUTPUT (root));
	g_object_unref (root);
	g_object_unref (sink);
	restore_g_log_messages ();
}

/*
 * A GsfInfileZip created without a source (reachable through g_object_new)
 * never gets a virtual directory.  Several entry points dereferenced the
 * missing vdir and crashed; they must refuse politely instead.  Reaching
 * the end of this function without crashing is the main check.
 */
static void
test_zip_infile_without_source (void)
{
	GsfInfile *z;
	GsfInput *in;
	int level = -1;

	inhibit_g_log_messages ();
	z = g_object_new (GSF_INFILE_ZIP_TYPE, NULL);
	check (z != NULL, "zip infile without source can be created");
	if (z == NULL) {
		restore_g_log_messages ();
		return;
	}

	check (gsf_infile_child_by_index (z, 0) == NULL, "child_by_index fails cleanly");
	check (gsf_infile_name_by_index (z, 0) == NULL, "name_by_index fails cleanly");
	check (gsf_infile_child_by_name (z, "x") == NULL, "child_by_name fails cleanly");
	check (gsf_infile_num_children (z) < 0, "num_children reports an error");
	in = gsf_input_dup (GSF_INPUT (z), NULL);
	check (in == NULL, "dup fails cleanly");
	if (in != NULL)
		g_object_unref (in);
	g_object_get (z, "compression-level", &level, NULL);
	check (level == 0, "compression-level getter copes (got %d)", level);
	check (gsf_input_read (GSF_INPUT (z), 1, NULL) == NULL,
	       "read fails cleanly");
	restore_g_log_messages ();

	g_object_unref (z);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_roundtrip_members ();
	test_large_member ();
	test_member_seek ();
	test_directory_member ();
	test_deflate_levels ();
	test_modtime_roundtrip ();
	test_duplicate_names ();
	test_max_comment ();
	test_compression_method_getter ();
	test_header_write_failure ();
	test_zip_infile_without_source ();

	gsf_shutdown ();
	return regress_exit_status ();
}
