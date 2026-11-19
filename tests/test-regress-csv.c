/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-csv.c: regression tests for gsf-output-csv.c
 *
 * The quoting code used to decode fields as UTF-8 without validating them
 * or stopping at the field's end.  Fields are allocated at exactly their
 * length here, so an overread is also visible under valgrind (this
 * program is run there from t8050-valgrind-regress.pl).
 */

#include <gsf/gsf.h>
#include <gsf/gsf-output-csv.h>
#include <gsf/gsf-output-memory.h>

#include "libgsf-regress.h"

#include <string.h>

/* Write one field (exactly @len bytes, no terminator) and return what the
 * CSV output produced as a NUL terminated string, with its length. */
static char *
csv_field (GsfOutputCsvQuotingMode mode, char const *bytes, size_t len,
	   gsf_off_t *out_len)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutputCsv *csv = g_object_new (GSF_OUTPUT_CSV_TYPE,
					  "sink", mem,
					  NULL);
	char *field = g_memdup2 (bytes, len);
	char *res;

	/* Setting the triggers selects AUTO mode, so set the mode last. */
	g_object_set (csv, "quoting-triggers", ",\"", NULL);
	g_object_set (csv, "quoting-mode", mode, NULL);

	gsf_output_csv_write_field (csv, field, len);
	gsf_output_close (GSF_OUTPUT (csv));

	*out_len = gsf_output_size (mem);
	res = g_strndup ((char const *)gsf_output_memory_get_bytes
			 (GSF_OUTPUT_MEMORY (mem)), *out_len);
	g_free (field);
	g_object_unref (csv);
	g_object_unref (mem);
	return res;
}

static void
test_latin1_lead_byte_at_end (void)
{
	/* 0xE9 is a 3-byte UTF-8 lead; it ends the field, so decoding it
	 * must not look at the bytes after the field. */
	gsf_off_t n;
	char *r = csv_field (GSF_OUTPUT_CSV_QUOTING_MODE_ALWAYS, "caf\xe9", 4, &n);
	check (n == 6 && memcmp (r, "\"caf\xe9\"", 6) == 0,
	       "ALWAYS: Latin-1 byte at the end is kept (%d bytes)", (int)n);
	g_free (r);

	r = csv_field (GSF_OUTPUT_CSV_QUOTING_MODE_AUTO, "caf\xe9", 4, &n);
	check (n == 4 && memcmp (r, "caf\xe9", 4) == 0,
	       "AUTO: Latin-1 byte at the end is kept unquoted (%d bytes)", (int)n);
	g_free (r);
}

static void
test_invalid_bytes_preserved (void)
{
	gsf_off_t n;
	char *r = csv_field (GSF_OUTPUT_CSV_QUOTING_MODE_AUTO, "a\xffz,\"q", 6, &n);
	check (n == 9 && memcmp (r, "\"a\xffz,\"\"q\"", 9) == 0,
	       "AUTO: invalid byte kept and quotes doubled (%d bytes)", (int)n);
	g_free (r);

	/* A truncated multi-byte sequence in the middle must not swallow
	 * the following bytes. */
	r = csv_field (GSF_OUTPUT_CSV_QUOTING_MODE_ALWAYS, "a\xe9z", 3, &n);
	check (n == 5 && memcmp (r, "\"a\xe9z\"", 5) == 0,
	       "ALWAYS: truncated sequence does not swallow its neighbours (%d bytes)",
	       (int)n);
	g_free (r);
}

static void
test_valid_utf8_unchanged (void)
{
	gsf_off_t n;
	char *r = csv_field (GSF_OUTPUT_CSV_QUOTING_MODE_AUTO,
			     "\xc3\xa9,\xe2\x82\xac\"x", 8, &n);
	check (n == 11 && memcmp (r, "\"\xc3\xa9,\xe2\x82\xac\"\"x\"", 11) == 0,
	       "valid UTF-8 is quoted and preserved (%d bytes)", (int)n);
	g_free (r);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_latin1_lead_byte_at_end ();
	test_invalid_bytes_preserved ();
	test_valid_utf8_unchanged ();

	gsf_shutdown ();
	return regress_exit_status ();
}
