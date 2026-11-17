/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-structured-blob.c: GsfStructuredBlob reading
 */

#include <gsf/gsf.h>
#include <gsf/gsf-structured-blob.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-outfile-msole.h>
#include <gsf/gsf-output-memory.h>

#include "libgsf-regress.h"

#include <string.h>

/* An input that claims to have data but fails every read (I/O error). */
typedef struct { GsfInput base; } BadIn;
typedef struct { GsfInputClass base; } BadInClass;

static guint8 const *
bi_read (G_GNUC_UNUSED GsfInput *i, G_GNUC_UNUSED size_t n,
	 G_GNUC_UNUSED guint8 *buf)
{
	return NULL;
}

static gboolean
bi_seek (G_GNUC_UNUSED GsfInput *i, G_GNUC_UNUSED gsf_off_t o,
	 G_GNUC_UNUSED GSeekType w)
{
	return TRUE;
}

static GsfInput *
bi_dup (G_GNUC_UNUSED GsfInput *i, G_GNUC_UNUSED GError **e)
{
	return NULL;
}

static void
bi_class_init (gpointer k, G_GNUC_UNUSED gpointer d)
{
	GsfInputClass *c = k;
	c->Read = bi_read;
	c->Seek = bi_seek;
	c->Dup = bi_dup;
}

static GType
bi_get_type (void)
{
	static GType t;
	if (!t) {
		GTypeInfo ti = { sizeof (BadInClass), NULL, NULL, bi_class_init,
				 NULL, NULL, sizeof (BadIn), 0, NULL, NULL };
		t = g_type_register_static (GSF_INPUT_TYPE, "RegressBadIn", &ti, 0);
	}
	return t;
}

static void
quiet (G_GNUC_UNUSED char const *d, G_GNUC_UNUSED GLogLevelFlags l,
       G_GNUC_UNUSED char const *m, G_GNUC_UNUSED gpointer u)
{
}

/*
 * gsf_structured_blob_read() ignored a failing gsf_input_read() and wrapped
 * the still-uninitialised buffer, so the blob's content was whatever the heap
 * held (and gsf_structured_blob_write would dump it into a file).
 */
static void
test_failed_read (void)
{
	GsfInput *in = g_object_new (bi_get_type (), NULL);
	GsfStructuredBlob *blob;

	gsf_input_set_size (in, 4096);
	gsf_input_set_name (in, "bad");

	g_log_set_default_handler (quiet, NULL);
	blob = gsf_structured_blob_read (in);
	g_log_set_default_handler (g_log_default_handler, NULL);

	check (blob == NULL, "a failed read does not produce a blob");
	if (blob)
		g_object_unref (blob);
	g_object_unref (in);
}

/* Control: a healthy input is captured byte for byte. */
static void
test_healthy_read (void)
{
	static guint8 const data[] = "0123456789abcdef";
	GsfInput *in = gsf_input_memory_new (data, sizeof data, FALSE);
	GsfStructuredBlob *blob = gsf_structured_blob_read (in);

	check (blob != NULL, "healthy input yields a blob");
	if (blob) {
		guint8 const *p;

		check (gsf_input_size (GSF_INPUT (blob)) == (gsf_off_t) sizeof data,
		       "blob size matches");
		p = gsf_input_read (GSF_INPUT (blob), sizeof data, NULL);
		check (p != NULL && memcmp (p, data, sizeof data) == 0,
		       "blob content matches");
		g_object_unref (blob);
	}
	g_object_unref (in);
}

/* A directory with one child whose data cannot be read. */
typedef struct { GsfInfile base; } FakeDir;
typedef struct { GsfInfileClass base; } FakeDirClass;

static int
fd_num_children (G_GNUC_UNUSED GsfInfile *i)
{
	return 1;
}

static char const *
fd_name_by_index (G_GNUC_UNUSED GsfInfile *i, int k)
{
	return k == 0 ? "kid" : NULL;
}

static GsfInput *
fd_child_by_index (G_GNUC_UNUSED GsfInfile *i, int k, G_GNUC_UNUSED GError **e)
{
	GsfInput *c;
	if (k != 0)
		return NULL;
	c = g_object_new (bi_get_type (), NULL);
	gsf_input_set_size (c, 64);
	gsf_input_set_name (c, "kid");
	return c;
}

static GsfInput *
fd_child_by_name (G_GNUC_UNUSED GsfInfile *i, G_GNUC_UNUSED char const *n,
		  G_GNUC_UNUSED GError **e)
{
	return NULL;
}

static void
fd_class_init (gpointer k, G_GNUC_UNUSED gpointer d)
{
	GsfInputClass *c = k;
	GsfInfileClass *f = k;
	c->Read = bi_read;
	c->Seek = bi_seek;
	c->Dup = bi_dup;
	f->num_children = fd_num_children;
	f->name_by_index = fd_name_by_index;
	f->child_by_index = fd_child_by_index;
	f->child_by_name = fd_child_by_name;
}

static GType
fd_get_type (void)
{
	static GType t;
	if (!t) {
		GTypeInfo ti = { sizeof (FakeDirClass), NULL, NULL, fd_class_init,
				 NULL, NULL, sizeof (FakeDir), 0, NULL, NULL };
		t = g_type_register_static (GSF_INFILE_TYPE, "RegressFakeDir", &ti, 0);
	}
	return t;
}

static gboolean container_finalized;

static void
on_finalize (G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GObject *o)
{
	container_finalized = TRUE;
}

/*
 * When writing a child failed, gsf_structured_blob_write() returned without
 * closing or unreffing the output it had created for the parent.  A live
 * child output holds a reference on its container, so the whole container
 * leaked.  A child that cannot be read leaves a NULL slot, which is a cheap
 * way to make the recursive write fail.
 */
static void
test_failed_child_write_releases_output (void)
{
	GsfInput *root = g_object_new (fd_get_type (), NULL);
	GsfStructuredBlob *blob;
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *of = gsf_outfile_msole_new (mem);
	gboolean ok;

	gsf_input_set_size (root, 0);
	gsf_input_set_name (root, "root");

	g_log_set_default_handler (quiet, NULL);
	blob = gsf_structured_blob_read (root);
	check (blob != NULL, "tree with an unreadable child still reads");
	if (blob) {
		container_finalized = FALSE;
		g_object_weak_ref (G_OBJECT (of), on_finalize, NULL);
		ok = gsf_structured_blob_write (blob, of);
		check (!ok, "write reports the failed child");
		gsf_output_close (GSF_OUTPUT (of));
		g_object_unref (of);
		g_object_unref (blob);
		check (container_finalized,
		       "container is released after a failed child write");
	} else {
		g_object_unref (of);
	}
	g_log_set_default_handler (g_log_default_handler, NULL);

	g_object_unref (mem);
	g_object_unref (root);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();
	test_healthy_read ();
	test_failed_read ();
	test_failed_child_write_releases_output ();
	gsf_shutdown ();
	return regress_exit_status ();
}
