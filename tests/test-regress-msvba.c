/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-msvba.c: regression tests for gsf-infile-msvba.c
 */

#include <gsf/gsf.h>
#include <gsf/gsf-infile-msvba.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-outfile.h>
#include <gsf/gsf-outfile-msole.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-output-memory.h>

#include "libgsf-regress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GsfInput *
bytes_input (guint8 const *data, gsf_off_t len)
{
	return gsf_input_memory_new_clone (data, len);
}

static gboolean
expect (guint8 const *got, int got_len, char const *want, int want_len)
{
	return got != NULL && got_len == want_len &&
		memcmp (got, want, want_len) == 0;
}

/*
 * gsf_vba_inflate() must seek to its `offset` parameter before checking
 * the 0x01 signature.  Previously it read at the input's current position
 * (0) and only used `offset` for loop bounds, so a module whose
 * compressed container starts at a non-zero SOURCECODEOFFSET was never
 * decompressed.  The container here has three bytes of prefix junk before
 * the real signature.
 */
static void
test_offset_parameter (void)
{
	static guint8 const c[] = {
		0xAA, 0xBB, 0xCC,	/* 3 bytes of prefix junk */
		0x01,			/* signature */
		0x08, 0xB0,		/* chunk header: 0xB008 */
		0x00, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	};
	GsfInput *in = bytes_input (c, sizeof c);
	int size = 0;
	guint8 *res = gsf_vba_inflate (in, 3, &size, FALSE);

	check (expect (res, size, "ABCDEFGH", 8),
	       "gsf_vba_inflate honours the offset parameter");
	if (res != NULL && size != 8)
		printf ("     (got %d bytes)\n", size);
	g_free (res);
	g_object_unref (in);
}

/*
 * After each chunk the stream position must land on the next chunk.
 * The G_SEEK_CUR seek added the absolute `offset` again (the input was
 * already positioned there), so the parse position doubled and later
 * chunks were lost.  A single trailing byte makes the failure
 * deterministic: with the correct position the 2-byte header read at EOF
 * fails cleanly (16 bytes of output), while the doubled seek reached EOF
 * right after the first chunk (8 bytes).
 */
static void
test_two_chunk_seek (void)
{
	static guint8 const c[] = {
		0x01,			/* signature */
		0x08, 0xB0, 0x00, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
		0x08, 0xB0, 0x00, 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		0x00,			/* trailing byte */
	};
	GsfInput *in = bytes_input (c, sizeof c);
	int size = 0;
	guint8 *res = gsf_vba_inflate (in, 0, &size, FALSE);

	check (expect (res, size, "ABCDEFGHIJKLMNOP", 16),
	       "two-chunk container decompresses completely");
	if (res != NULL && size != 16)
		printf ("     (got %d bytes)\n", size);
	g_free (res);
	g_object_unref (in);
}

/*
 * A raw (uncompressed) chunk, whose header signature is 0b011 (0x3xxx),
 * must be copied verbatim.  Previously it fell into the else-branch,
 * which sliced a fixed 4094 bytes and fed them to the decompressor as if
 * they were compressed, producing garbage.
 */
static void
test_raw_chunk (void)
{
	static guint8 payload[4096];
	guint8 *c = g_new (guint8, 1 + 2 + 4096);
	unsigned i;
	int size = 0;
	guint8 *res;

	for (i = 0; i < sizeof payload; i++)
		payload[i] = (guint8)('a' + (i % 26));
	c[0] = 0x01;
	c[1] = 0xFF; c[2] = 0x3F;	/* header 0x3FFF: raw chunk */
	memcpy (c + 3, payload, sizeof payload);

	res = gsf_vba_inflate (bytes_input (c, 1 + 2 + 4096), 0, &size, FALSE);
	check (expect (res, size, (char const *)payload, sizeof payload),
	       "raw chunk copied verbatim");
	if (res != NULL && size != (int)sizeof payload)
		printf ("     (got %d bytes)\n", size);
	g_free (res);
	g_free (c);
}

/*
 * Drive vba_dir_read() through gsf_infile_msvba_new() with a hand-made "dir"
 * stream: an (uncompressed) container holding one raw 4096-byte chunk that
 * starts with the records given in @records.
 */
static GsfInfile *
make_vba_source (guint8 const *records, size_t nrec, GsfOutput **keep_mem)
{
	guint8 *stream = g_new0 (guint8, 1 + 2 + 4096);
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *out = gsf_outfile_msole_new (mem);
	GsfOutput *child;
	GsfInput *in;
	GsfInfile *res;

	stream[0] = 0x01;			/* container signature */
	stream[1] = 0xFF; stream[2] = 0x3F;	/* raw chunk */
	memcpy (stream + 3, records, nrec);	/* rest of the chunk is zero */

	child = gsf_outfile_new_child (out, "dir", FALSE);
	gsf_output_write (child, 1 + 2 + 4096, stream);
	gsf_output_close (child);
	g_object_unref (child);
	gsf_output_close (GSF_OUTPUT (out));
	g_object_unref (out);
	g_free (stream);

	in = gsf_input_memory_new (gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem)),
				   gsf_output_size (mem), FALSE);
	res = gsf_infile_msole_new (in, NULL);
	g_object_unref (in);
	*keep_mem = mem;	/* the input borrows the memory output's bytes */
	return res;
}

static gboolean
vba_parses (guint8 const *records, size_t nrec)
{
	GsfOutput *mem = NULL;
	GsfInfile *src = make_vba_source (records, nrec, &mem);
	GError *err = NULL;
	GsfInfile *vba;
	gboolean ok;

	g_assert (src != NULL);
	vba = gsf_infile_msvba_new (src, &err);
	ok = (vba != NULL);
	if (vba)
		g_object_unref (vba);
	g_clear_error (&err);
	g_object_unref (src);
	g_object_unref (mem);
	return ok;
}

/*
 * Record lengths come straight from the stream.  The bounds check used
 * "ptr + len > end", which wraps around on 32-bit systems for lengths near
 * 2^32 and let the parser run off the buffer.  64-bit builds cannot wrap, so
 * this test cannot tell old from new there; it pins the intended behaviour
 * (valid input parses, over-long or truncated records are rejected cleanly)
 * on every platform.
 */
static void
test_dir_record_bounds (void)
{
	/* tag, len(4), data */
	static guint8 const valid[] = {
		0x01,0x00, 0x04,0x00,0x00,0x00, 1,0,0,0,		/* tag 1 */
		0x04,0x00, 0x04,0x00,0x00,0x00, 'P','r','o','j',	/* name */
		0x09,0x00, 0x04,0x00,0x00,0x00, 9,4,0,0, 0xAA,0xBB,	/* +2 extra */
		0x10,0x00, 0x00,0x00,0x00,0x00,			/* terminator */
	};
	static guint8 const huge_len[] = {
		0x04,0x00, 0xF0,0xFF,0xFF,0xFF, 'x',
	};
	static guint8 const huge_len_max[] = {
		0x04,0x00, 0xFF,0xFF,0xFF,0xFF, 'x',
	};
	/* tag 9 whose length leaves no room for its two extra bytes:
	 * 4096 - 6 = 4090 bytes follow the header; claim 4089. */
	static guint8 const nine_no_room[] = {
		0x09,0x00, 0xF9,0x0F,0x00,0x00,
	};

	inhibit_g_log_messages ();
	check (vba_parses (valid, sizeof valid), "valid dir records parse");
	check (!vba_parses (huge_len, sizeof huge_len),
	       "length 0xFFFFFFF0 is rejected");
	check (!vba_parses (huge_len_max, sizeof huge_len_max),
	       "length 0xFFFFFFFF is rejected");
	check (!vba_parses (nine_no_room, sizeof nine_no_room),
	       "tag 9 without room for its extra bytes is rejected");
	restore_g_log_messages ();
}

/*
 * gsf_infile_msvba_new() returns an object that is documented (and typed) as
 * a GsfInfile, but its class installed none of the GsfInput/GsfInfile
 * virtual methods, so ordinary calls such as gsf_infile_num_children() or
 * gsf_input_dup() jumped through a NULL function pointer.
 */
static void
test_msvba_is_a_usable_infile (void)
{
	static guint8 const records[] = {
		0x10,0x00, 0x00,0x00,0x00,0x00,		/* terminator only */
	};
	GsfOutput *mem = NULL;
	GsfInfile *src = make_vba_source (records, sizeof records, &mem);
	GsfInfile *vba;
	GsfInput *dup;
	GError *err = NULL;

	g_assert (src != NULL);
	inhibit_g_log_messages ();
	vba = gsf_infile_msvba_new (src, NULL);
	restore_g_log_messages ();
	check (vba != NULL, "VBA project opens");
	if (vba != NULL) {
		check (gsf_infile_num_children (vba) == -1,
		       "num_children reports it is not a storage");
		check (gsf_infile_name_by_index (vba, 0) == NULL,
		       "name_by_index finds nothing");
		check (gsf_infile_child_by_index (vba, 0) == NULL,
		       "child_by_index finds nothing");
		check (gsf_infile_child_by_name (vba, "x") == NULL,
		       "child_by_name finds nothing");
		dup = gsf_input_dup (GSF_INPUT (vba), &err);
		check (dup == NULL && err != NULL,
		       "dup fails cleanly with an error");
		if (dup)
			g_object_unref (dup);
		g_clear_error (&err);
		g_object_unref (vba);
	}
	g_object_unref (src);
	g_object_unref (mem);
}

static int critical_count;

static void
count_criticals (G_GNUC_UNUSED char const *d, GLogLevelFlags l,
		 G_GNUC_UNUSED char const *m, G_GNUC_UNUSED gpointer u)
{
	if (l & G_LOG_LEVEL_CRITICAL)
		critical_count++;
}

/* Inflate @n bytes of @data; returns the output size and counts criticals. */
static int
inflate_bytes (guint8 const *data, size_t n, guint8 **out)
{
	GsfInput *in = gsf_input_memory_new (data, n, FALSE);
	int size = -1;

	*out = gsf_vba_inflate (in, 0, &size, FALSE);
	g_object_unref (in);
	return size;
}

/*
 * The final chunk's bounds check must agree with the size of the section it
 * creates: a chunk header's size field N means N+1 data bytes follow.  The
 * check once required only N, so a last chunk that was exactly one byte
 * short passed it and then failed gsf_input_proxy_new_section's
 * precondition (a GLib critical).  Truncated chunks must be dropped
 * quietly, earlier chunks kept, and an exactly fitting chunk accepted.
 */
static void
test_final_chunk_boundary (void)
{
	/* 0x01 signature; chunk header 0xB008 = compressed, size field 8, so
	 * 9 data bytes: one flag byte (8 literals) and the literals. */
	static guint8 const one[] = {
		0x01,
		0x08, 0xB0, 0x00, 'A','B','C','D','E','F','G','H',
	};
	static guint8 const two[] = {
		0x01,
		0x08, 0xB0, 0x00, 'A','B','C','D','E','F','G','H',
		0x08, 0xB0, 0x00, '1','2','3','4','5','6','7','8',
	};
	guint8 *out;
	int size;

	g_log_set_default_handler (count_criticals, NULL);

	critical_count = 0;
	size = inflate_bytes (one, sizeof one, &out);
	check (size == 8 && out != NULL && memcmp (out, "ABCDEFGH", 8) == 0,
	       "a final chunk that fits exactly is decoded (size %d)", size);
	check (critical_count == 0, "no critical for an exactly fitting chunk");
	g_free (out);

	critical_count = 0;
	size = inflate_bytes (one, sizeof one - 1, &out);
	check (size == 0, "a lone chunk one byte short yields nothing (size %d)", size);
	check (critical_count == 0,
	       "no critical for a chunk one byte short (got %d)", critical_count);
	g_free (out);

	critical_count = 0;
	size = inflate_bytes (two, sizeof two - 1, &out);
	check (size == 8 && out != NULL && memcmp (out, "ABCDEFGH", 8) == 0,
	       "the chunk before a truncated one is kept (size %d)", size);
	check (critical_count == 0,
	       "no critical when the second chunk is one byte short (got %d)",
	       critical_count);
	g_free (out);

	g_log_set_default_handler (g_log_default_handler, NULL);
}

/*
 * gsf_msole_inflate only appended a completed 4096-byte window when more
 * data followed it, so a stream that decompresses to exactly 4096 bytes
 * (a full VBA chunk, the normal case for all but the last chunk of a
 * large module) came back empty.  Check streams that end exactly on the
 * window boundary, once with literals and once with a final copy token.
 */
static void
test_inflate_exact_window (void)
{
	enum { WIN = 4096 };
	GByteArray *src = g_byte_array_new ();
	guint8 expected[WIN];
	GsfInput *in;
	GByteArray *res;
	guint8 b;
	int i, n;

	/* 4096 literals: 512 flag bytes (all literal), 8 literals each. */
	for (n = 0; n < WIN; ) {
		b = 0;
		g_byte_array_append (src, &b, 1);
		for (i = 0; i < 8; i++, n++) {
			expected[n] = (guint8)(n % 251);
			g_byte_array_append (src, &expected[n], 1);
		}
	}
	in = gsf_input_memory_new (src->data, src->len, FALSE);
	res = gsf_msole_inflate (in, 0);
	check (res != NULL && res->len == WIN && memcmp (res->data, expected, WIN) == 0,
	       "4096 literals inflate to the full window (got %u bytes)",
	       res ? res->len : 0);
	if (res)
		g_byte_array_free (res, TRUE);
	g_object_unref (in);
	g_byte_array_free (src, TRUE);

	/* 4087 literals, then a copy of 9 bytes at distance 0 (repeat the
	 * previous byte) as the 8th and last element of its flag byte: the
	 * copy token completes the window and nothing follows it. */
	src = g_byte_array_new ();
	for (n = 0; n < 4080; ) {
		b = 0;
		g_byte_array_append (src, &b, 1);
		for (i = 0; i < 8; i++, n++) {
			expected[n] = (guint8)(n % 251);
			g_byte_array_append (src, &expected[n], 1);
		}
	}
	b = 0x80;
	g_byte_array_append (src, &b, 1);
	for (i = 0; i < 7; i++, n++) {
		expected[n] = (guint8)(n % 251);
		g_byte_array_append (src, &expected[n], 1);
	}
	b = 6;					/* token 6: len 9, distance 0 */
	g_byte_array_append (src, &b, 1);
	b = 0;
	g_byte_array_append (src, &b, 1);
	for (i = 0; i < 9; i++, n++)
		expected[n] = expected[n - 1];
	g_assert (n == WIN);

	in = gsf_input_memory_new (src->data, src->len, FALSE);
	res = gsf_msole_inflate (in, 0);
	check (res != NULL && res->len == WIN && memcmp (res->data, expected, WIN) == 0,
	       "window ending in a copy token inflates fully (got %u bytes)",
	       res ? res->len : 0);
	if (res)
		g_byte_array_free (res, TRUE);
	g_object_unref (in);
	g_byte_array_free (src, TRUE);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_offset_parameter ();
	test_two_chunk_seek ();
	test_raw_chunk ();
	test_final_chunk_boundary ();
	test_inflate_exact_window ();
	test_dir_record_bounds ();
	test_msvba_is_a_usable_infile ();

	gsf_shutdown ();
	return regress_exit_status ();
}
