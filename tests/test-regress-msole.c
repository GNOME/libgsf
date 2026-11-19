/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-msole.c: regression tests for gsf-outfile-msole.c
 */

#include <gsf/gsf.h>
#include <gsf/gsf-outfile-msole.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-input-memory.h>

#include "libgsf-regress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef G_OS_UNIX
#include <sys/resource.h>
#endif

static void
fill (char *buf, char c, size_t n)
{
	memset (buf, c, n);
	buf[n] = '\0';
}

static GsfInput *
ole_to_input (GsfOutput *mem)
{
	guint8 const *bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
	return gsf_input_memory_new_clone (bytes, gsf_output_size (mem));
}

/*
 * Sanity check for the test harness itself: children created and written
 * in the same order must round-trip exactly.  Every other check in this
 * file relies on this working.
 */
static void
test_roundtrip_in_order (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *root = gsf_outfile_msole_new (mem);
	GsfOutput *c1 = gsf_outfile_new_child (root, "d1", FALSE);
	GsfOutput *c2 = gsf_outfile_new_child (root, "d2", FALSE);
	char *a = g_new (char, 5001), *b = g_new (char, 5001);
	GsfInput *in;
	GsfInfile *ole;
	GsfInput *r1, *r2;

	fill (a, 'A', 5000);
	fill (b, 'B', 5000);
	gsf_output_write (c1, 5000, (guint8 const *)a);
	gsf_output_close (c1);
	g_object_unref (c1);
	gsf_output_write (c2, 5000, (guint8 const *)b);
	gsf_output_close (c2);
	g_object_unref (c2);
	gsf_output_close (GSF_OUTPUT (root));
	g_object_unref (root);

	in = ole_to_input (mem);
	ole = gsf_infile_msole_new (in, NULL);
	check (ole != NULL, "in-order OLE file reopens");
	r1 = (ole != NULL) ? gsf_infile_child_by_name (ole, "d1") : NULL;
	r2 = (ole != NULL) ? gsf_infile_child_by_name (ole, "d2") : NULL;
	check (r1 != NULL && gsf_input_size (r1) == 5000,
	       "in-order: d1 has the right size");
	{
		guint8 const *r = (r1 != NULL) ? gsf_input_read (r1, 5000, NULL) : NULL;
		check (r != NULL && memcmp (r, a, 5000) == 0,
		       "in-order: d1 content matches");
	}
	{
		guint8 const *r = (r2 != NULL) ? gsf_input_read (r2, 5000, NULL) : NULL;
		check (r != NULL && memcmp (r, b, 5000) == 0,
		       "in-order: d2 content matches");
	}
	if (r1 != NULL) g_object_unref (r1);
	if (r2 != NULL) g_object_unref (r2);
	g_object_unref (ole);
	g_object_unref (in);
	g_object_unref (mem);
	g_free (a);
	g_free (b);
}

/*
 * The OLE2 small/big block boundary: streams under OLE_DEFAULT_THRESHOLD
 * (4096 bytes) live in the mini stream, streams at or above it use the
 * regular FAT.  A member at exactly the cutoff, one byte below it, and
 * one byte above it must all round-trip correctly.  Also: a member that
 * starts small and grows past the threshold (a second write pushes it
 * over) must survive the SMALL_BLOCK -> BIG_BLOCK conversion.
 */
static void
test_small_big_boundary (void)
{
	/* Sizes: threshold-1 (small block), threshold (converts),
	 * threshold+1 (big block). */
	static size_t const sizes[] = { 4095, 4096, 4097 };
	unsigned si;

	for (si = 0; si < G_N_ELEMENTS (sizes); si++) {
		size_t n = sizes[si];
		guint8 *pattern = g_new (guint8, n);
		GsfOutput *mem = gsf_output_memory_new ();
		GsfOutfile *root = gsf_outfile_msole_new (mem);
		GsfOutput *child = gsf_outfile_new_child (root, "boundary", FALSE);
		GsfInput *in;
		GsfInfile *ole;
		GsfInput *c;
		unsigned i;
		gboolean ok = TRUE;

		regress_fill_pattern (pattern, n);
		gsf_output_write (child, n, pattern);
		gsf_output_close (child);
		g_object_unref (child);
		gsf_output_close (GSF_OUTPUT (root));
		g_object_unref (root);

		{
			guint8 const *bytes = gsf_output_memory_get_bytes
				(GSF_OUTPUT_MEMORY (mem));
			in = gsf_input_memory_new_clone
				(bytes, gsf_output_size (mem));
		}
		g_object_unref (mem);

		ole = gsf_infile_msole_new (in, NULL);
		check (ole != NULL, "OLE file with %zu-byte member reopens", n);
		c = (ole != NULL)
			? gsf_infile_child_by_name (ole, "boundary") : NULL;
		check (c != NULL, "%zu-byte member found", n);
		if (c != NULL) {
			check (gsf_input_size (c) == (gsf_off_t)n,
			       "%zu-byte member has the right size", n);
			for (i = 0; ok && i < n; i += 512) {
				size_t chunk = (n - i > 512) ? 512 : (n - i);
				guint8 buf[512];
				guint8 const *r = gsf_input_read (c, chunk, buf);
				if (r == NULL || memcmp (r, pattern + i, chunk) != 0)
					ok = FALSE;
			}
			check (ok, "%zu-byte member content matches", n);
			g_object_unref (c);
		}
		g_object_unref (ole);
		g_object_unref (in);
		g_free (pattern);
	}

	/* A member that starts small (one write below the threshold) and
	 * grows past it on a second write must survive the conversion. */
	{
		size_t n = 4096;
		guint8 *pattern = g_new (guint8, n);
		GsfOutput *mem = gsf_output_memory_new ();
		GsfOutfile *root = gsf_outfile_msole_new (mem);
		GsfOutput *child = gsf_outfile_new_child (root, "growing", FALSE);
		GsfInput *in;
		GsfInfile *ole;
		GsfInput *c;
		unsigned i;
		gboolean ok = TRUE;

		regress_fill_pattern (pattern, n);
		/* First write: 4095 bytes (stays small block).  */
		gsf_output_write (child, n - 1, pattern);
		/* Second write: pushes it to 4096, triggering the
		 * SMALL_BLOCK -> BIG_BLOCK conversion.  */
		gsf_output_write (child, 1, pattern + n - 1);
		gsf_output_close (child);
		g_object_unref (child);
		gsf_output_close (GSF_OUTPUT (root));
		g_object_unref (root);

		{
			guint8 const *bytes = gsf_output_memory_get_bytes
				(GSF_OUTPUT_MEMORY (mem));
			in = gsf_input_memory_new_clone
				(bytes, gsf_output_size (mem));
		}
		g_object_unref (mem);

		ole = gsf_infile_msole_new (in, NULL);
		check (ole != NULL, "growing OLE file reopens");
		c = (ole != NULL)
			? gsf_infile_child_by_name (ole, "growing") : NULL;
		check (c != NULL, "growing member found");
		if (c != NULL) {
			check (gsf_input_size (c) == (gsf_off_t)n,
			       "growing member has the right size");
			for (i = 0; ok && i < n; i += 512) {
				size_t chunk = (n - i > 512) ? 512 : (n - i);
				guint8 buf[512];
				guint8 const *r = gsf_input_read (c, chunk, buf);
				if (r == NULL || memcmp (r, pattern + i, chunk) != 0)
					ok = FALSE;
			}
			check (ok, "growing member content matches after conversion");
			g_object_unref (c);
		}
		g_object_unref (ole);
		g_object_unref (in);
		g_free (pattern);
	}
}

/*
 * A member whose name is at least 32 UTF-16 units must produce a valid
 * directory entry.  The writer compared the CHARACTER count against the
 * 64-byte DIRENT_MAX_NAME_SIZE field, so names of 32-63 characters got an
 * illegal DIRENT_NAME_LEN and the UTF-16 name bytes spilled over the
 * CLSID/timestamp fields; libgsf's own reader then rejected or lost the
 * entry.  The CFB name field holds at most 31 characters, so the writer
 * must truncate instead.
 */
static void
test_long_name_truncation (void)
{
	char const *longname = "abcdefghijklmnopqrstuvwxyz0123456789";
	char truncated[32];
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *root = gsf_outfile_msole_new (mem);
	GsfOutput *child = gsf_outfile_new_child (root, longname, FALSE);
	GsfInput *in;
	GsfInfile *ole;

	memcpy (truncated, longname, 31);
	truncated[31] = '\0';

	check (child != NULL, "long-named child created");
	if (child != NULL) {
		gsf_output_write (child, 4, (guint8 const *)"data");
		gsf_output_close (child);
		g_object_unref (child);
	}
	gsf_output_close (GSF_OUTPUT (root));
	g_object_unref (root);

	in = ole_to_input (mem);
	ole = gsf_infile_msole_new (in, NULL);
	check (ole != NULL, "OLE file with long member name reopens");
	if (ole != NULL) {
		GsfInput *c = gsf_infile_child_by_name (ole, truncated);
		check (c != NULL,
		       "long name truncated to the legal 31 characters");
		if (c != NULL) {
			guint8 buf[4];
			guint8 const *r;
			check (gsf_input_size (c) == 4, "long-named member size");
			r = gsf_input_read (c, 4, buf);
			check (r != NULL && memcmp (r, "data", 4) == 0,
			       "long-named member content");
			g_object_unref (c);
		}
		g_object_unref (ole);
	}
	g_object_unref (in);
	g_object_unref (mem);
}

/*
 * printf data must be accounted in the child's size, so it survives the
 * round trip.  This currently works: the base class updates the child's
 * size after the Vprintf vfunc returns -- a guard, documenting correct
 * behaviour.
 */
static void
test_printf_roundtrip (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *root = gsf_outfile_msole_new (mem);
	GsfOutput *child = gsf_outfile_new_child (root, "printf", FALSE);
	GsfInput *in;
	GsfInfile *ole;
	guint8 *expect = g_new (guint8, 5003);
	GsfInput *c;

	memset (expect, 'A', 5000);
	memcpy (expect + 5000, "BBB", 3);

	gsf_output_write (child, 5000, expect);
	gsf_output_printf (child, "%s", "BBB");
	gsf_output_close (child);
	g_object_unref (child);
	gsf_output_close (GSF_OUTPUT (root));
	g_object_unref (root);

	in = ole_to_input (mem);
	ole = gsf_infile_msole_new (in, NULL);
	c = (ole != NULL) ? gsf_infile_child_by_name (ole, "printf") : NULL;
	check (c != NULL, "printf member found");
	if (c != NULL) {
		guint8 const *r;
		check (gsf_input_size (c) == 5003,
		       "printf data included in member size");
		r = gsf_input_read (c, 5003, NULL);
		check (r != NULL && memcmp (r, expect, 5003) == 0,
		       "printf data survives the round trip");
		g_object_unref (c);
	}
	g_object_unref (ole);
	g_object_unref (in);
	g_object_unref (mem);
	g_free (expect);
}

/*
 * Children created in one order but written in another must still
 * round-trip.  The BAT chains are appended sequentially, so they must be
 * emitted in sector (write) order; the writer emitted them in creation
 * order, which scrambled the allocation table when the two orders
 * differed.
 */
static void
test_roundtrip_out_of_order (void)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *root = gsf_outfile_msole_new (mem);
	GsfOutput *c1 = gsf_outfile_new_child (root, "c1", FALSE);
	GsfOutput *c2 = gsf_outfile_new_child (root, "c2", FALSE);
	char *a = g_new (char, 5001), *b = g_new (char, 5001);
	GsfInput *in;
	GsfInfile *ole;
	GsfInput *r1, *r2;

	fill (a, 'A', 5000);
	fill (b, 'B', 5000);

	/* write in the opposite order of creation */
	gsf_output_write (c2, 5000, (guint8 const *)b);
	gsf_output_close (c2);
	g_object_unref (c2);
	gsf_output_write (c1, 5000, (guint8 const *)a);
	gsf_output_close (c1);
	g_object_unref (c1);
	gsf_output_close (GSF_OUTPUT (root));
	g_object_unref (root);

	in = ole_to_input (mem);
	ole = gsf_infile_msole_new (in, NULL);
	check (ole != NULL, "out-of-order OLE file reopens");
	r1 = (ole != NULL) ? gsf_infile_child_by_name (ole, "c1") : NULL;
	r2 = (ole != NULL) ? gsf_infile_child_by_name (ole, "c2") : NULL;
	{
		guint8 const *r = (r1 != NULL) ? gsf_input_read (r1, 5000, NULL) : NULL;
		check (r != NULL && memcmp (r, a, 5000) == 0,
		       "out-of-order: c1 content matches");
	}
	{
		guint8 const *r = (r2 != NULL) ? gsf_input_read (r2, 5000, NULL) : NULL;
		check (r != NULL && memcmp (r, b, 5000) == 0,
		       "out-of-order: c2 content matches");
	}
	if (r1 != NULL) g_object_unref (r1);
	if (r2 != NULL) g_object_unref (r2);
	g_object_unref (ole);
	g_object_unref (in);
	g_object_unref (mem);
	g_free (a);
	g_free (b);
}

/*
 * The directory-entry "seen before" bitmap was sized from the directory's
 * FAT chain length times the sector size.  A 2MB file with 1MB sectors and
 * a 262144-entry chain made libgsf try to allocate 4GB.  The size is now
 * bounded by what fits in the input.  The address space is capped so that
 * an unfixed library fails here instead of thrashing the machine.
 */
static void
test_dirent_bitmap_is_bounded (void)
{
	enum { SHIFT = 20, SECTOR = 1 << SHIFT, NENT = SECTOR / 4 };
	static guint8 const sig[] =
		{ 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };
	guint8 *buf = g_malloc0 (2 * SECTOR);
	guint8 *bat = buf + SECTOR;
	GsfInput *in;
	GsfInfile *ole;
	GError *err = NULL;
	int i;

#ifdef G_OS_UNIX
	struct rlimit lim;
	if (getrlimit (RLIMIT_AS, &lim) == 0) {
		lim.rlim_cur = (rlim_t)1 << 30;
		setrlimit (RLIMIT_AS, &lim);
	}
#endif

	memcpy (buf, sig, sizeof sig);
	GSF_LE_SET_GUINT16 (buf + 0x1e, SHIFT);
	GSF_LE_SET_GUINT16 (buf + 0x20, 6);
	GSF_LE_SET_GUINT32 (buf + 0x2c, 1);		/* one BAT block */
	GSF_LE_SET_GUINT32 (buf + 0x30, 0);		/* directory at block 0 */
	GSF_LE_SET_GUINT32 (buf + 0x38, 4096);
	GSF_LE_SET_GUINT32 (buf + 0x3c, 0xfffffffe);
	GSF_LE_SET_GUINT32 (buf + 0x44, 0xfffffffe);
	GSF_LE_SET_GUINT32 (buf + 0x4c, 0);		/* the BAT is block 0 */
	for (i = 0; i < NENT; i++)
		GSF_LE_SET_GUINT32 (bat + 4 * i,
				    i + 1 < NENT ? (guint32)(i + 1) : 0xfffffffeu);

	in = gsf_input_memory_new (buf, 2 * SECTOR, FALSE);
	ole = gsf_infile_msole_new (in, &err);
	/* The directory is garbage; all that matters is a clean failure. */
	check (ole == NULL, "garbage directory is rejected");
	if (ole)
		g_object_unref (ole);
	g_clear_error (&err);
	g_object_unref (in);
	g_free (buf);
}

/*
 * A stream's block count is only computed when it is closed.  A big stream
 * left open when the root was closed was written with an empty block chain
 * (while close() reported success), producing an invalid file.
 */
static void
test_close_root_with_open_streams (void)
{
	enum { BIG = 20000 };
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *root = gsf_outfile_msole_new (mem);
	GsfOutput *small, *dir, *inner, *big;
	guint8 *payload = g_malloc (BIG);
	GsfInput *in;
	GsfInfile *ole;
	GsfInput *s;
	int i;

	for (i = 0; i < BIG; i++)
		payload[i] = (guint8)(i * 7 + 3);

	small = gsf_outfile_new_child (root, "small", FALSE);
	gsf_output_write (small, 5, (guint8 const *)"small");
	dir = gsf_outfile_new_child (root, "dir", TRUE);
	inner = gsf_outfile_new_child (GSF_OUTFILE (dir), "inner", FALSE);
	gsf_output_write (inner, 5, (guint8 const *)"inner");
	big = gsf_outfile_new_child (root, "big", FALSE);
	gsf_output_write (big, BIG, payload);

	/* None of the children is closed. */
	check (gsf_output_close (GSF_OUTPUT (root)), "root closes");
	g_object_unref (big);
	g_object_unref (inner);
	g_object_unref (dir);
	g_object_unref (small);
	g_object_unref (root);

	in = gsf_input_memory_new (gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem)),
				   gsf_output_size (mem), FALSE);
	ole = gsf_infile_msole_new (in, NULL);
	check (ole != NULL, "file opens");
	if (ole) {
		guint8 const *d;

		s = gsf_infile_child_by_name (ole, "big");
		check (s != NULL && gsf_input_size (s) == BIG, "big stream has its size");
		d = s ? gsf_input_read (s, BIG, NULL) : NULL;
		check (d != NULL && memcmp (d, payload, BIG) == 0,
		       "big stream content survives");
		if (s)
			g_object_unref (s);

		s = gsf_infile_child_by_name (ole, "small");
		d = s ? gsf_input_read (s, 5, NULL) : NULL;
		check (d != NULL && memcmp (d, "small", 5) == 0, "small stream content survives");
		if (s)
			g_object_unref (s);

		s = gsf_infile_child_by_name (ole, "dir");
		check (s != NULL && GSF_IS_INFILE (s), "directory exists");
		if (s) {
			GsfInput *t = gsf_infile_child_by_name (GSF_INFILE (s), "inner");
			d = t ? gsf_input_read (t, 5, NULL) : NULL;
			check (d != NULL && memcmp (d, "inner", 5) == 0,
			       "stream inside the open directory survives");
			if (t)
				g_object_unref (t);
			g_object_unref (s);
		}
		g_object_unref (ole);
	}
	g_object_unref (in);
	g_object_unref (mem);
	g_free (payload);
}

/*
 * If loading the small-block allocation table (SBAT) fails part-way,
 * ole_info_get_sb_file used to ignore the failure: the unfilled tail of
 * the table stayed zero and small-block streams opened "successfully".
 * Corrupt one SBAT entry (to a value beyond the table, but not a magic
 * marker) that lies past the chain of the stream we read, and make sure
 * the failure is reported.  An undamaged file must still work.
 */
static void
test_corrupt_sbat (void)
{
	enum { LEN = 100, SECTOR = 512 };
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *root = gsf_outfile_msole_new (mem);
	GsfOutput *o = gsf_outfile_new_child (root, "a", FALSE);
	guint8 payload[LEN];
	guint8 *bytes;
	gsf_off_t size;
	guint32 sbat_start;
	int pass;

	regress_fill_pattern (payload, LEN);
	gsf_output_write (o, LEN, payload);
	gsf_output_close (o);
	g_object_unref (o);
	check (gsf_output_close (GSF_OUTPUT (root)), "root closes");
	g_object_unref (root);

	size = gsf_output_size (mem);
	bytes = g_memdup2 (gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem)), size);
	g_object_unref (mem);

	sbat_start = GSF_LE_GET_GUINT32 (bytes + 0x3c);
	check ((gsf_off_t)(sbat_start + 2) * SECTOR <= size,
	       "SBAT sector lies inside the file");

	for (pass = 0; pass < 2; pass++) {
		GsfInput *in;
		GsfInfile *ole;
		GsfInput *s;

		/* pass 0: undamaged; pass 1: entry 5 of the SBAT is bad */
		if (pass == 1)
			GSF_LE_SET_GUINT32 (bytes + (sbat_start + 1) * SECTOR + 5 * 4,
					    0x00000800);

		in = gsf_input_memory_new (bytes, size, FALSE);
		ole = gsf_infile_msole_new (in, NULL);
		check (ole != NULL, "pass %d: file opens", pass);
		if (ole != NULL) {
			inhibit_g_log_messages ();
			s = gsf_infile_child_by_name (ole, "a");
			restore_g_log_messages ();
			if (pass == 0) {
				guint8 const *d = s ? gsf_input_read (s, LEN, NULL) : NULL;
				check (d != NULL && memcmp (d, payload, LEN) == 0,
				       "undamaged small stream reads correctly");
			} else {
				check (s == NULL,
				       "small stream is refused when the SBAT is damaged");
			}
			if (s != NULL)
				g_object_unref (s);
			g_object_unref (ole);
		}
		g_object_unref (in);
	}
	g_free (bytes);
}

/* A sink whose writes all fail (seeking and closing succeed). */
typedef struct { GsfOutput base; } FailSink;
typedef struct { GsfOutputClass base; } FailSinkClass;

static gboolean
fs_write (G_GNUC_UNUSED GsfOutput *o, G_GNUC_UNUSED size_t n,
	  G_GNUC_UNUSED guint8 const *d)
{
	return FALSE;
}

static gboolean
fs_seek (G_GNUC_UNUSED GsfOutput *o, G_GNUC_UNUSED gsf_off_t off,
	 G_GNUC_UNUSED GSeekType w)
{
	return TRUE;
}

static gboolean
fs_close (G_GNUC_UNUSED GsfOutput *o)
{
	return TRUE;
}

static void
fs_class_init (gpointer k, G_GNUC_UNUSED gpointer d)
{
	GsfOutputClass *c = k;
	c->Write = fs_write;
	c->Seek = fs_seek;
	c->Close = fs_close;
}

static GType
fs_get_type (void)
{
	static GType t;
	if (!t) {
		GTypeInfo i = { sizeof (FailSinkClass), NULL, NULL, fs_class_init,
				NULL, NULL, sizeof (FailSink), 0, NULL, NULL };
		t = g_type_register_static (GSF_OUTPUT_TYPE, "RegressMSOleFailSink", &i, 0);
	}
	return t;
}

/*
 * None of the writes to the sink in gsf-outfile-msole.c were checked, so
 * a failing sink (disk full, I/O error) still produced "successful"
 * writes and a "successful" close of a corrupt file.  Check both the data
 * path (a stream growing past the small-block threshold is flushed to the
 * sink) and the directory/table path at close.
 */
static void
test_sink_write_failures_are_reported (void)
{
	GsfOutput *sink = g_object_new (fs_get_type (), NULL);
	GsfOutfile *root = gsf_outfile_msole_new (sink);
	GsfOutput *big = gsf_outfile_new_child (root, "big", FALSE);
	GsfOutput *small = gsf_outfile_new_child (root, "small", FALSE);
	guint8 *data = g_malloc0 (8192);

	check (gsf_output_write (small, 5, (guint8 const *)"hello"),
	       "small stream write is buffered, so it succeeds");
	check (!gsf_output_write (big, 8192, data),
	       "write that has to reach a failing sink reports failure");
	g_free (data);

	gsf_output_close (big);
	g_object_unref (big);
	gsf_output_close (small);
	g_object_unref (small);
	check (!gsf_output_close (GSF_OUTPUT (root)),
	       "closing onto a failing sink reports failure");
	g_object_unref (root);
	g_object_unref (sink);
}

static void
test_directory_write_failure_is_reported (void)
{
	GsfOutput *sink = g_object_new (fs_get_type (), NULL);
	GsfOutfile *root = gsf_outfile_msole_new (sink);
	GsfOutput *small = gsf_outfile_new_child (root, "small", FALSE);

	gsf_output_write (small, 5, (guint8 const *)"hello");
	gsf_output_close (small);
	g_object_unref (small);
	check (!gsf_output_close (GSF_OUTPUT (root)),
	       "close reports a failure to write the directory and tables");
	g_object_unref (root);
	g_object_unref (sink);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_roundtrip_in_order ();
	test_long_name_truncation ();
	test_printf_roundtrip ();
	test_roundtrip_out_of_order ();
	test_small_big_boundary ();
	test_dirent_bitmap_is_bounded ();
	test_close_root_with_open_streams ();
	test_corrupt_sbat ();
	test_sink_write_failures_are_reported ();
	test_directory_write_failure_is_reported ();

	gsf_shutdown ();
	return regress_exit_status ();
}
