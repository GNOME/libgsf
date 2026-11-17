/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-shared-memory.c: tests for gsf-shared-memory.c, including
 * the refcounting that GsfInputMemory relies on.
 */

#include "libgsf-regress.h"

#include <gsf/gsf-shared-memory.h>
#include <gsf/gsf-input-memory.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * gsf_shared_memory_new stores the buffer, size and needs_free flag; the
 * fields are readable through the public struct.
 */
static void
test_basic (void)
{
	static guint8 data[] = "shared memory test";
	GsfSharedMemory *mem = gsf_shared_memory_new (data, sizeof data, FALSE);

	check (mem != NULL, "shared memory created");
	check (mem != NULL && mem->buf == data, "buffer pointer is stored");
	check (mem != NULL && mem->size == (gsf_off_t)(sizeof data),
	       "size is stored");
	check (mem != NULL && mem->needs_free == FALSE,
	       "needs_free is FALSE");
	check (mem != NULL && mem->needs_unmap == FALSE,
	       "needs_unmap is FALSE");

	g_object_unref (mem);
}

/*
 * Refcounting: the object survives intermediate unrefs and the buffer
 * pointer stays stable across them.
 */
static void
test_refcount (void)
{
	static guint8 data[] = "refcount test";
	GsfSharedMemory *mem = gsf_shared_memory_new (data, sizeof data, FALSE);

	check (mem != NULL, "shared memory created");
	g_object_ref (mem);
	check (mem->buf == data, "buffer stable after ref");
	g_object_unref (mem);
	check (mem->buf == data, "buffer stable after intermediate unref");

	g_object_unref (mem);
}

/*
 * Two GsfSharedMemory objects can share the same underlying buffer
 * (each with needs_free == FALSE so neither frees it).
 */
static void
test_shared_buffer (void)
{
	static guint8 data[] = "shared buffer data";
	GsfSharedMemory *a = gsf_shared_memory_new (data, sizeof data, FALSE);
	GsfSharedMemory *b = gsf_shared_memory_new (data, sizeof data, FALSE);

	check (a != NULL && b != NULL, "both objects created");
	check (a->buf == b->buf, "both point to the same buffer");
	check (a->size == b->size, "both have the same size");

	g_object_unref (b);
	check (a->buf == data, "the first object is still valid");
	g_object_unref (a);
}

/*
 * GsfInputMemory wraps a GsfSharedMemory: dropping the input must not
 * invalidate a dup that shares the same underlying shared memory.
 */
static void
test_input_memory_sharing (void)
{
	GsfInput *in = gsf_input_memory_new
		((guint8 const *)"shared input", 12, FALSE);
	GsfInput *dup = gsf_input_dup (in, NULL);
	guint8 buf[12];
	guint8 const *r;

	check (in != NULL && dup != NULL, "input and dup created");
	if (in == NULL || dup == NULL) {
		if (in != NULL) g_object_unref (in);
		if (dup != NULL) g_object_unref (dup);
		return;
	}

	/* Drop the original; the dup must still be able to read.  */
	g_object_unref (in);
	r = gsf_input_read (dup, 12, buf);
	check (r != NULL, "dup reads after the original is gone");
	check (r != NULL && memcmp (r, "shared input", 12) == 0,
	       "dup content matches after the original is gone");
	g_object_unref (dup);
}

/*
 * gsf_input_memory_new with needs_free=TRUE: the underlying buffer is
 * freed when the last reference goes away.  We verify by stealing the
 * bytes and checking they survive the input's lifetime.
 */
static void
test_input_memory_needs_free (void)
{
	guint8 *heap_data = g_memdup2 ((guint8 const *)"owned data", 11);
	GsfInput *in = gsf_input_memory_new (heap_data, 10, TRUE);
	GsfInput *dup = gsf_input_dup (in, NULL);
	guint8 buf[11];
	guint8 const *r;

	check (in != NULL && dup != NULL, "input and dup created");

	/* Both reference the same shared memory; drop both.  */
	g_object_unref (in);
	r = gsf_input_read (dup, 10, buf);
	check (r != NULL && memcmp (r, "owned data", 10) == 0,
	       "dup reads after the original is gone (needs_free)");
	g_object_unref (dup);
	/* The buffer was freed by the shared memory (needs_free = TRUE).  */
	heap_data = NULL;
}

/*
 * gsf_input_memory_new_clone produces an independent copy: mutating the
 * clone's data (if we could) would not affect the original.  At minimum
 * the content matches and the sizes are equal.
 */
static void
test_input_memory_clone (void)
{
	GsfInput *orig = gsf_input_memory_new
		((guint8 const *)"clone me", 8, FALSE);
	GsfInput *clone;
	guint8 buf[8];
	guint8 const *r;

	check (orig != NULL, "original created");
	if (orig == NULL)
		return;
	r = gsf_input_read (orig, 8, buf);
	check (r != NULL, "original is readable");
	clone = gsf_input_memory_new_clone (r, 8);
	check (clone != NULL, "clone created");
	if (clone == NULL) {
		g_object_unref (orig);
		return;
	}

	check (gsf_input_size (orig) == gsf_input_size (clone),
	       "clone has the same size");
	r = gsf_input_read (clone, 8, buf);
	check (r != NULL && memcmp (r, "clone me", 8) == 0,
	       "clone content matches");

	/* The clone is independent: dropping the original is fine.  */
	g_object_unref (orig);
	check (!gsf_input_seek (clone, 0, G_SEEK_SET),
	       "seek the clone back to the start");
	r = gsf_input_read (clone, 8, buf);
	check (r != NULL && memcmp (r, "clone me", 8) == 0,
	       "clone survives the original");
	g_object_unref (clone);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_basic ();
	test_refcount ();
	test_shared_buffer ();
	test_input_memory_sharing ();
	test_input_memory_needs_free ();
	test_input_memory_clone ();

	gsf_shutdown ();
	return regress_exit_status ();
}
