/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-outmem-printf.c: exercise for gsf-output-memory.c
 * (meant to be run under valgrind; see t8050-valgrind-regress.pl)
 */

#include <gsf/gsf.h>
#include <gsf/gsf-output-memory.h>

/*
 * gsf_output_memory_vprintf() computed mem->capacity - cur_offset after a
 * seek past EOF; the size_t subtraction underflowed and g_vsnprintf wrote
 * out of bounds before the "insufficient space" check ran.  This program
 * deliberately exercises that path; on a fixed libgsf it produces no
 * memory errors.
 */
static void
test_vprintf_past_eof (void)
{
	GsfOutput *mem = gsf_output_memory_new ();

	gsf_output_write (mem, 5, (guint8 const *)"hello");
	gsf_output_seek (mem, 100000, G_SEEK_SET);	/* past the end */
	gsf_output_printf (mem, "%s", "world");		/* out-of-bounds write */

	gsf_output_close (mem);
	g_object_unref (mem);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_vprintf_past_eof ();

	gsf_shutdown ();
	return 0;
}
