/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-xmlout-uaf.c: exercise for gsf-libxml.c / gsf-output.c
 * (meant to be run under valgrind; see t8050-valgrind-regress.pl)
 */

#include <gsf/gsf.h>
#include <gsf/gsf-libxml.h>
#include <gsf/gsf-output-memory.h>

/*
 * GsfXMLOut's sink lifetime: gsf_xml_out_set_output() stored the sink
 * without taking a reference, and gsf_output_wrap() only installed a weak
 * reference whose user data is the raw sink pointer.  When the sink dies
 * first, every subsequent write goes through freed memory, and the
 * weak-notify callback writes into the freed object when the wrapper is
 * destroyed.  This program deliberately exercises that path; on a fixed
 * libgsf it produces no memory errors.
 */
static void
test_xmlout_sink_lifetime (void)
{
	GsfOutput *sink = gsf_output_memory_new ();
	GsfXMLOut *xout = gsf_xml_out_new (sink);

	g_object_unref (sink);		/* xout holds no reference */

	gsf_xml_out_start_element (xout, "test");
	gsf_xml_out_add_cstr (xout, "attr", "value");	/* use-after-free */
	gsf_xml_out_end_element (xout);

	g_object_unref (xout);		/* weak-notify writes into freed sink */
}

/*
 * gsf_output_wrap() is public API and the wrapper need not hold a reference
 * on the wrapee.  If the wrapee dies first, the wrapper's weak-ref notify
 * must not write into it.
 */
static void
test_wrapee_dies_first (void)
{
	GsfOutput *sink = gsf_output_memory_new ();
	GObject *wrapper = g_object_new (G_TYPE_OBJECT, NULL);

	g_assert (gsf_output_wrap (wrapper, sink));
	g_object_unref (sink);
	g_object_unref (wrapper);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_xmlout_sink_lifetime ();
	test_wrapee_dies_first ();

	gsf_shutdown ();
	return 0;
}
