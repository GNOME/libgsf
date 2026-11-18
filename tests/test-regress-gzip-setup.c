/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-gzip-setup.c: GsfOutputGZip must not ignore a failed setup
 */

#include <gsf/gsf.h>
#include <gsf/gsf-output-gzip.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-input-memory.h>

#include "libgsf-regress.h"

#include <string.h>

/* A sink whose first N writes fail. */
typedef struct {
	GsfOutput base;
	int fail_writes;
	int nwrites;
} FailOut;
typedef struct { GsfOutputClass base; } FailOutClass;

static gboolean
fo_write (GsfOutput *o, G_GNUC_UNUSED size_t n, G_GNUC_UNUSED guint8 const *d)
{
	FailOut *f = (FailOut *)o;
	f->nwrites++;
	if (f->fail_writes > 0) {
		f->fail_writes--;
		return FALSE;
	}
	return TRUE;
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
		t = g_type_register_static (GSF_OUTPUT_TYPE, "RegressFailOut", &i, 0);
	}
	return t;
}

/*
 * gsf_output_gzip_write() called setup but ignored its failure.  A failed
 * header write then left the zlib stream initialised but "not set up", so
 * the next write ran deflateInit2 a second time on the same stream
 * (leaking ~400KB of zlib state, visible under valgrind) and reported
 * success for data that never reached a valid gzip stream.
 */
static void
test_failed_header (void)
{
	FailOut *sink = g_object_new (fo_get_type (), NULL);
	GsfOutput *gz;
	gboolean w1, w2, closed;

	sink->fail_writes = 1;		/* the gzip header write fails */
	gz = gsf_output_gzip_new ((GsfOutput *)sink, NULL);
	check (gz != NULL, "gzip output created");

	inhibit_g_log_messages ();
	w1 = gsf_output_write (gz, 4, (guint8 const *)"abcd");
	w2 = gsf_output_write (gz, 4, (guint8 const *)"efgh");
	closed = gsf_output_close (gz);
	restore_g_log_messages ();

	check (!w1, "first write reports the failed header");
	check (!w2, "later writes keep failing");
	check (!closed, "close reports failure");
	check (gsf_output_error (gz) != NULL, "an error is recorded");
	check (sink->nwrites == 1, "the header is not re-sent (%d sink writes)",
	       sink->nwrites);

	g_object_unref (gz);
	g_object_unref (sink);
}

/* Control: a healthy stream still works end to end. */
static void
test_healthy_roundtrip (void)
{
	GsfOutput *sink = gsf_output_memory_new ();
	GsfOutput *gz = gsf_output_gzip_new (sink, NULL);
	GsfInput *raw, *plain;
	guint8 const *bytes;
	gsf_off_t n;

	check (gsf_output_write (gz, 5, (guint8 const *)"hello"), "write ok");
	check (gsf_output_close (gz), "close ok");
	n = gsf_output_size (sink);
	bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (sink));
	check (n > 10 && bytes[0] == 0x1f && bytes[1] == 0x8b, "gzip header present");

	raw = gsf_input_memory_new (bytes, n, FALSE);
	plain = gsf_input_uncompress (raw);
	check (gsf_input_size (plain) == 5, "uncompressed size is 5");
	{
		guint8 buf[5];
		guint8 const *r = gsf_input_read (plain, 5, buf);
		check (r != NULL && memcmp (r, "hello", 5) == 0, "content survives");
	}
	g_object_unref (plain);
	g_object_unref (gz);
	g_object_unref (sink);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();
	test_healthy_roundtrip ();
	test_failed_header ();
	gsf_shutdown ();
	return regress_exit_status ();
}
