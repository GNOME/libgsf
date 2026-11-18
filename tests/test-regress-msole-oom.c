/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-msole-oom.c: GsfInfileMSOle must not leak when it runs out of
 * memory while opening a file.
 *
 * This needs to make one specific allocation fail, which is done by
 * overriding malloc() in this program (glibc only).  It is a separate binary
 * so that the override cannot affect any other test, and so that it is not
 * accidentally run under valgrind, which replaces malloc itself.
 */

#include <gsf/gsf.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-input-memory.h>

#include "libgsf-regress.h"

#include <string.h>

#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2,33)
#define CAN_INJECT 1
#endif
#endif

#ifdef CAN_INJECT
#include <malloc.h>

extern void *__libc_malloc (size_t);

static size_t fail_size;
static int injected;

void *
malloc (size_t n)
{
	if (fail_size != 0 && n == fail_size) {
		injected++;
		return NULL;
	}
	return __libc_malloc (n);
}

/*
 * With 4096-byte sectors and one BAT block, the BAT array is a 4096-byte
 * g_new0 and the next allocation, the "metabat" scratch buffer, is
 * g_try_new (guint32, 4096) = 16384 bytes.  When that one fails, the code
 * freed the info struct with g_free(), leaking the BAT array (4KB per
 * failed open).
 */
static void
test_oom_while_opening (void)
{
	enum { SZ = 512 + 3 * 4096, N = 2000 };
	static guint8 const sig[] = { 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };
	guint8 *buf = g_malloc0 (SZ);
	GsfInput *in;
	GError *err = NULL;
	GsfInfile *ole;
	size_t before, after;
	int i, failed = 0, msg_ok = 0;

	memcpy (buf, sig, sizeof sig);
	GSF_LE_SET_GUINT16 (buf + 0x1e, 12);		/* 4096-byte sectors */
	GSF_LE_SET_GUINT16 (buf + 0x20, 6);
	GSF_LE_SET_GUINT32 (buf + 0x2c, 1);		/* one BAT block */
	GSF_LE_SET_GUINT32 (buf + 0x30, 0);
	GSF_LE_SET_GUINT32 (buf + 0x38, 4096);
	GSF_LE_SET_GUINT32 (buf + 0x3c, 0xfffffffe);
	GSF_LE_SET_GUINT32 (buf + 0x44, 0xfffffffe);
	GSF_LE_SET_GUINT32 (buf + 0x4c, 0);		/* the BAT is block 0 */
	in = gsf_input_memory_new (buf, SZ, FALSE);

	fail_size = 4 * 4096;

	/* One attempt first, so one-time allocations are not counted. */
	ole = gsf_infile_msole_new (in, &err);
	if (ole)
		g_object_unref (ole);
	g_clear_error (&err);

	before = mallinfo2 ().uordblks;
	for (i = 0; i < N; i++) {
		ole = gsf_infile_msole_new (in, &err);
		if (ole == NULL) {
			failed++;
			if (err && strstr (err->message, "memory"))
				msg_ok++;
		} else
			g_object_unref (ole);
		g_clear_error (&err);
	}
	after = mallinfo2 ().uordblks;
	fail_size = 0;

	check (injected > N, "the metabat allocation failure was injected (%d times)",
	       injected);
	check (failed == N, "every injected failure makes the open fail (%d of %d)",
	       failed, N);
	check (msg_ok == N, "each failure reports insufficient memory");
	/* The leak was 4KB per failed open (8MB here); allow generous noise. */
	check (after - before < (size_t)64 * 1024,
	       "no memory leaked over %d failed opens (heap grew %ld bytes)",
	       N, (long)(after - before));

	g_object_unref (in);
	g_free (buf);
}
#endif

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();
#ifdef CAN_INJECT
	test_oom_while_opening ();
#else
	g_print ("skipping: needs glibc 2.33+ (malloc override and mallinfo2)\n");
#endif
	gsf_shutdown ();
	return regress_exit_status ();
}
