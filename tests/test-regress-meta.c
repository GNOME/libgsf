/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-meta.c: regression tests for metadata handling
 */

#include <gsf/gsf.h>
#include <gsf/gsf-libxml.h>
#include <gsf/gsf-doc-meta-data.h>
#include <gsf/gsf-meta-names.h>
#include <gsf/gsf-docprop-vector.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-opendoc-utils.h>
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
set_int_prop (GsfDocMetaData *md, char const *name, int value)
{
	GValue *val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_INT);
	g_value_set_int (val, value);
	gsf_doc_meta_data_insert (md, g_strdup (name), val);
}

/*
 * The MSOLE LCID table: 0x044C is Malayalam ("ml") and "gd_GB" maps to
 * Scottish Gaelic (0x0491).  The table duplicated 0x044C for "az" and
 * 0x100C (French, Switzerland) for "gd_GB", so lookups returned the wrong
 * language.
 */
static void
test_lcid_table (void)
{
	check (0 == strcmp (gsf_msole_language_for_lid (0x044C), "ml"),
	       "lid 0x044C maps to Malayalam (ml)");
	check (gsf_msole_lid_for_language ("gd_GB") == 0x0491,
	       "gd_GB maps to 0x0491");
}

/*
 * No ODF namespace URI may contain a space: the OO_NS_DATASTYLE entry read
 * "...:xmlns:data style:1.0" and could therefore never match a conforming
 * document, since namespace matching is by URI string comparison.
 */
static void
test_namespace_uris (void)
{
	GsfXMLInNS const *ns = gsf_odf_get_ns ();
	int i;
	gboolean bad = FALSE;

	for (i = 0; ns != NULL && ns[i].uri != NULL; i++) {
		if (strchr (ns[i].uri, ' ') != NULL) {
			printf ("     (bad uri: '%s')\n", ns[i].uri);
			bad = TRUE;
		}
	}
	check (!bad, "no space in any ODF namespace uri");
}

/*
 * gsf_doc_meta_data_read_from_odf() must report an error for malformed
 * input.  The parse result of gsf_xml_in_doc_parse() was ignored and the
 * GError was never set, so the function always reported success.
 */
static void
test_odf_parse_error (void)
{
	GsfDocMetaData *md = gsf_doc_meta_data_new ();
	GsfInput *in = gsf_input_memory_new_clone
		((guint8 const *)"this is not xml at all", 22);
	GError *err = gsf_doc_meta_data_read_from_odf (md, in);

	check (err != NULL,
	       "read_from_odf reports an error for invalid input");
	if (err != NULL)
		g_error_free (err);
	g_object_unref (in);
	g_object_unref (md);
}

/*
 * A user-defined string vector must round-trip through the Unicode
 * (codepage 1200) MSOLE property set writer.  The vector's type marker
 * stayed byte-counted VT_LPSTR while its elements were written with
 * UTF-16 character counts, so the values came back garbled.  Note that
 * gsf_doc_meta_data_insert() takes ownership of the GValue pointer
 * itself, so the value must be heap-allocated.
 */
static void
test_vector_roundtrip (void)
{
	GsfDocMetaData *md = gsf_doc_meta_data_new ();
	GsfDocPropVector *vec = gsf_docprop_vector_new ();
	GValue sv = G_VALUE_INIT;
	GValue *vv = g_new0 (GValue, 1);
	GsfOutput *out = gsf_output_memory_new ();
	GsfInput *in;
	GsfDocMetaData *md2;
	GsfDocProp *prop;
	GValue const *val;
	GsfDocPropVector *vec2;
	char *rendered;

	set_int_prop (md, GSF_META_NAME_CODEPAGE, 1200);

	g_value_init (&sv, G_TYPE_STRING);
	g_value_set_string (&sv, "alpha");
	gsf_docprop_vector_append (vec, &sv);
	g_value_set_string (&sv, "beta");
	gsf_docprop_vector_append (vec, &sv);
	g_value_unset (&sv);

	g_value_init (vv, GSF_DOCPROP_VECTOR_TYPE);
	g_value_take_object (vv, vec);
	gsf_doc_meta_data_insert (md, g_strdup ("MyVector"), vv);

	if (!gsf_doc_meta_data_write_to_msole (md, out, TRUE)) {
		check (FALSE, "write_to_msole succeeded");
	} else {
		guint8 const *bytes =
			gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (out));
		in = gsf_input_memory_new_clone (bytes, gsf_output_size (out));
		md2 = gsf_doc_meta_data_new ();
		gsf_doc_meta_data_read_from_msole (md2, in);
		prop = gsf_doc_meta_data_lookup (md2, "MyVector");
		check (prop != NULL, "vector property found on re-read");
		if (prop != NULL) {
			val = gsf_doc_prop_get_val (prop);
			check (val != NULL &&
			       G_VALUE_HOLDS (val, GSF_DOCPROP_VECTOR_TYPE),
			       "vector property still holds a vector");
			if (val != NULL &&
			    G_VALUE_HOLDS (val, GSF_DOCPROP_VECTOR_TYPE)) {
				vec2 = g_value_get_object ((GValue *)val);
				rendered = gsf_docprop_vector_as_string (vec2);
				check (rendered != NULL &&
				       strstr (rendered, "alpha") != NULL &&
				       strstr (rendered, "beta") != NULL,
				       "vector elements survive the round trip");
				if (rendered != NULL)
					printf ("     (got \"%s\")\n", rendered);
				g_free (rendered);
			}
		}
		g_object_unref (md2);
		g_object_unref (in);
	}
	g_object_unref (out);
	g_object_unref (md);
}

/*
 * The MS OLE property-set writer kept a 16-byte-per-property table in
 * g_alloca(), sized by the number of user-defined properties.  A document
 * with enough properties (the reader accepts hundreds of thousands) made
 * saving it overflow the stack.  Cap the stack well below that table's size
 * so an unfixed library crashes here rather than only on huge inputs.
 */
static void
test_many_user_props_write (void)
{
	enum { NPROPS = 100000 };		/* 1.6MB of table */
	GsfDocMetaData *meta = gsf_doc_meta_data_new ();
	GsfOutput *out = gsf_output_memory_new ();
	gboolean ok;
	int i;

#ifdef G_OS_UNIX
	struct rlimit lim;
	if (getrlimit (RLIMIT_STACK, &lim) == 0) {
		rlim_t want = (rlim_t)1 << 20;	/* 1MB */
		if (lim.rlim_max == RLIM_INFINITY || lim.rlim_max >= want) {
			lim.rlim_cur = want;
			setrlimit (RLIMIT_STACK, &lim);
		}
	}
#endif

	for (i = 0; i < NPROPS; i++) {
		GValue *v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_INT);
		g_value_set_int (v, i);
		gsf_doc_meta_data_insert (meta, g_strdup_printf ("prop%d", i), v);
	}

	ok = gsf_doc_meta_data_write_to_msole (meta, out, TRUE);
	check (ok, "writing %d user properties succeeds", NPROPS);
	check (gsf_output_size (out) > NPROPS * 8, "output has plausible size");

	g_object_unref (out);
	g_object_unref (meta);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	/* Keep output visible even when a check aborts the process. */
	setvbuf (stdout, NULL, _IONBF, 0);

	gsf_init ();

	test_lcid_table ();
	test_namespace_uris ();
	test_odf_parse_error ();
	test_vector_roundtrip ();
	test_many_user_props_write ();

	gsf_shutdown ();
	return regress_exit_status ();
}
