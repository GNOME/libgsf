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

/*
 * A GSF_TIMESTAMP GValue that is initialized but holds no boxed value
 * made the VT_FILETIME writer dereference a NULL pointer (ts->timet).  A
 * property with no value must not crash the writer or take down the
 * properties around it.
 */
static void
test_unset_timestamp_write (void)
{
	GsfDocMetaData *md = gsf_doc_meta_data_new ();
	GsfDocMetaData *md2;
	GsfOutput *out = gsf_output_memory_new ();
	GValue *tv = g_new0 (GValue, 1);
	GsfInput *in;
	GsfDocProp *prop;
	gboolean ok;

	set_int_prop (md, GSF_META_NAME_CODEPAGE, 1200);
	set_int_prop (md, "Keep", 42);
	g_value_init (tv, GSF_TIMESTAMP_TYPE);	/* no value set */
	gsf_doc_meta_data_insert (md, g_strdup ("Unset"), tv);

	ok = gsf_doc_meta_data_write_to_msole (md, out, TRUE);
	check (ok, "writing a property set with an unset timestamp succeeds");
	if (ok) {
		in = gsf_input_memory_new_clone
			(gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (out)),
			 gsf_output_size (out));
		md2 = gsf_doc_meta_data_new ();
		check (gsf_doc_meta_data_read_from_msole (md2, in) == NULL,
		       "the result can be read back");
		prop = gsf_doc_meta_data_lookup (md2, "Keep");
		check (prop != NULL &&
		       G_VALUE_HOLDS_INT (gsf_doc_prop_get_val (prop)) &&
		       g_value_get_int (gsf_doc_prop_get_val (prop)) == 42,
		       "the neighbouring property survives");
		check (gsf_doc_meta_data_lookup (md2, "Unset") == NULL,
		       "the valueless property is not written");
		g_object_unref (md2);
		g_object_unref (in);
	}
	g_object_unref (out);
	g_object_unref (md);
}

/*
 * The Macintosh pseudo language id 0xFFF (what gsf_msole_codepage_to_lid
 * returns for the Mac charset) maps to the pseudo codepage 0xFFF.  The
 * iconv helpers only knew the real Mac codepages (10000, 0x8000), so they
 * asked for the nonexistent "CP4095" and could never open a converter.
 */
static void
test_mac_codepage_iconv (void)
{
	int cp = gsf_msole_lid_to_codepage (gsf_msole_codepage_to_lid (77));
	GIConv cd;

	check (cp == 0x0FFF, "Mac charset maps to the 0xFFF pseudo codepage");

	inhibit_g_log_messages ();
	cd = gsf_msole_iconv_open_for_import (cp);
	restore_g_log_messages ();
	check (cd != (GIConv)-1, "an iconv converter opens for the Mac pseudo codepage");
	if (cd != (GIConv)-1) {
		/* 0x8A is a-umlaut in MacRoman. */
		char *res = g_convert_with_iconv ("\x8a", 1, cd, NULL, NULL, NULL);
		check (res != NULL && strcmp (res, "\xc3\xa4") == 0,
		       "0x8A converts to a-umlaut");
		g_free (res);
		g_iconv_close (cd);
	}
}

/*
 * A multi-valued property (a GsfDocPropVector, as produced by the OLE2
 * reader for e.g. a VT_VECTOR|VT_LPSTR Title) stored under a standard ODF
 * name was written as an empty element: the generic GValue writer has no
 * case for the vector's object type, so all its values were lost on ODF
 * export.
 */
static void
test_odf_vector_under_standard_name (void)
{
	GsfDocMetaData *md = gsf_doc_meta_data_new ();
	GsfDocPropVector *vec = gsf_docprop_vector_new ();
	GValue sv = G_VALUE_INIT;
	GValue *vv = g_new0 (GValue, 1);
	GsfOutput *mem = gsf_output_memory_new ();
	GsfXMLOut *xout;
	char *text;
	gsf_off_t size;

	g_value_init (&sv, G_TYPE_STRING);
	g_value_set_string (&sv, "alpha");
	gsf_docprop_vector_append (vec, &sv);
	g_value_set_string (&sv, "beta");
	gsf_docprop_vector_append (vec, &sv);
	g_value_unset (&sv);
	g_value_init (vv, GSF_DOCPROP_VECTOR_TYPE);
	g_value_take_object (vv, vec);
	gsf_doc_meta_data_insert (md, g_strdup (GSF_META_NAME_TITLE), vv);

	xout = gsf_xml_out_new (mem);
	check (gsf_doc_meta_data_write_to_odf (md, xout), "ODF write succeeds");
	g_object_unref (xout);

	size = gsf_output_size (mem);
	text = g_strndup ((char const *)gsf_output_memory_get_bytes
			  (GSF_OUTPUT_MEMORY (mem)), size);
	check (strstr (text, "alpha") != NULL, "first vector value is exported");
	check (strstr (text, "beta") != NULL, "second vector value is exported");
	g_free (text);

	g_object_unref (mem);
	g_object_unref (md);
}

/*
 * When the ODF reader merged the elements of a vector (written as
 * meta:user-defined entries named GSF_DOCPROP_VECTOR:NNNN:name), it
 * appended to the vector's GArray directly.  GsfDocPropVector keeps a
 * second, public (deprecated) GValueArray view that only
 * gsf_docprop_vector_append updates, so after a read the two views
 * disagreed: gsf_value_get_docprop_varray showed only the first element.
 */
static void
test_odf_vector_views_agree (void)
{
	GsfDocMetaData *md = gsf_doc_meta_data_new ();
	GsfDocMetaData *md2 = gsf_doc_meta_data_new ();
	GsfDocPropVector *vec = gsf_docprop_vector_new ();
	GValue sv = G_VALUE_INIT;
	GValue *vv = g_new0 (GValue, 1);
	GsfOutput *mem = gsf_output_memory_new ();
	GsfXMLOut *xout;
	GsfInput *in;
	GsfDocProp *prop;
	char const *words[] = { "a", "b", "c" };
	guint i;

	g_value_init (&sv, G_TYPE_STRING);
	for (i = 0; i < G_N_ELEMENTS (words); i++) {
		g_value_set_string (&sv, words[i]);
		gsf_docprop_vector_append (vec, &sv);
	}
	g_value_unset (&sv);
	g_value_init (vv, GSF_DOCPROP_VECTOR_TYPE);
	g_value_take_object (vv, vec);
	gsf_doc_meta_data_insert (md, g_strdup ("MyVec"), vv);

	xout = gsf_xml_out_new (mem);
	check (gsf_doc_meta_data_write_to_odf (md, xout), "ODF write succeeds");
	g_object_unref (xout);

	in = gsf_input_memory_new_clone
		(gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem)),
		 gsf_output_size (mem));
	check (gsf_doc_meta_data_read_from_odf (md2, in) == NULL,
	       "ODF read succeeds");
	prop = gsf_doc_meta_data_lookup (md2, "MyVec");
	check (prop != NULL, "vector property is read back");
	if (prop != NULL) {
		GValue const *v = gsf_doc_prop_get_val (prop);
		GArray *ga = gsf_value_get_docprop_array (v);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		GValueArray *va = gsf_value_get_docprop_varray (v);
G_GNUC_END_IGNORE_DEPRECATIONS
		check (ga != NULL && ga->len == G_N_ELEMENTS (words),
		       "array view has all %d elements", (int)G_N_ELEMENTS (words));
		check (va != NULL && ga != NULL && va->n_values == ga->len,
		       "legacy GValueArray view agrees (%u vs %u)",
		       va ? va->n_values : 0, ga ? ga->len : 0);
	}

	g_object_unref (in);
	g_object_unref (mem);
	g_object_unref (md2);
	g_object_unref (md);
}

/*
 * A document that ends inside <meta:user-defined> made the reader abort
 * with the property name (duplicated by the start handler) still pending;
 * only the end handler freed it, so it leaked.  The error must still be
 * reported; the leak is caught by running this program under valgrind
 * (t8050-valgrind-regress.pl).
 */
static void
test_odf_truncated_user_defined (void)
{
	static char const xml[] =
		"<?xml version=\"1.0\"?>"
		"<office:document-meta"
		" xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\""
		" xmlns:meta=\"urn:oasis:names:tc:opendocument:xmlns:meta:1.0\""
		" office:version=\"1.2\"><office:meta>"
		"<meta:user-defined meta:name=\"pending\""
		" meta:value-type=\"string\">abc";
	GsfDocMetaData *md = gsf_doc_meta_data_new ();
	GsfInput *in = gsf_input_memory_new ((guint8 const *)xml,
					     sizeof xml - 1, FALSE);
	GError *err;

	inhibit_g_log_messages ();
	err = gsf_doc_meta_data_read_from_odf (md, in);
	restore_g_log_messages ();
	check (err != NULL, "truncated document is reported as an error");
	if (err != NULL)
		g_error_free (err);
	check (gsf_doc_meta_data_lookup (md, "pending") == NULL,
	       "the unfinished property is not recorded");

	g_object_unref (in);
	g_object_unref (md);
}

/* A sink that accepts a limited number of bytes, then fails every write
 * (seeking and closing succeed). */
typedef struct { GsfOutput base; gsf_off_t budget; } MetaFailSink;
typedef struct { GsfOutputClass base; } MetaFailSinkClass;

static gboolean
mfs_write (GsfOutput *o, size_t n, G_GNUC_UNUSED guint8 const *d)
{
	MetaFailSink *f = (MetaFailSink *)o;
	if ((gsf_off_t)n > f->budget)
		return FALSE;
	f->budget -= n;
	return TRUE;
}

static gboolean
mfs_seek (G_GNUC_UNUSED GsfOutput *o, G_GNUC_UNUSED gsf_off_t off,
	  G_GNUC_UNUSED GSeekType w)
{
	return TRUE;
}

static gboolean
mfs_close (G_GNUC_UNUSED GsfOutput *o)
{
	return TRUE;
}

static void
mfs_class_init (gpointer k, G_GNUC_UNUSED gpointer d)
{
	GsfOutputClass *c = k;
	c->Write = mfs_write;
	c->Seek = mfs_seek;
	c->Close = mfs_close;
}

static GType
mfs_get_type (void)
{
	static GType t;
	if (!t) {
		GTypeInfo i = { sizeof (MetaFailSinkClass), NULL, NULL, mfs_class_init,
				NULL, NULL, sizeof (MetaFailSink), 0, NULL, NULL };
		t = g_type_register_static (GSF_OUTPUT_TYPE, "RegressMetaFailSink", &i, 0);
	}
	return t;
}

/*
 * The property-set writer ignored failures of the writes to its sink, so
 * saving metadata to a full or broken output could report success after
 * a failure part-way through a section (and, when the first write of a
 * property failed, the next property recorded the same offset).  Let the
 * sink accept only the first N bytes for every N shorter than the whole
 * output: each of them must make the write fail.
 */
static void
test_msole_write_to_failing_sink (void)
{
	GsfDocMetaData *md = gsf_doc_meta_data_new ();
	GsfOutput *mem = gsf_output_memory_new ();
	gsf_off_t total, budget;
	int missed = 0, tried = 0;

	set_int_prop (md, GSF_META_NAME_CODEPAGE, 1200);
	set_int_prop (md, "Keep", 42);
	set_int_prop (md, "Other", 43);
	check (gsf_doc_meta_data_write_to_msole (md, mem, TRUE),
	       "reference write succeeds");
	total = gsf_output_size (mem);
	g_object_unref (mem);

	for (budget = 0; budget < total; budget++) {
		GsfOutput *sink = g_object_new (mfs_get_type (), NULL);
		((MetaFailSink *)sink)->budget = budget;
		tried++;
		if (gsf_doc_meta_data_write_to_msole (md, sink, TRUE)) {
			if (missed++ < 5)
				g_print ("  budget %ld of %ld: failure not reported\n",
					 (long)budget, (long)total);
		}
		g_object_unref (sink);
	}
	check (tried > 0 && missed == 0,
	       "every short sink is detected (%d of %d missed)", missed, tried);

	g_object_unref (md);
}

/*
 * A property whose type cannot be exported (here a 64-bit integer) was
 * announced as "ignored" but its type word was already written and its
 * offset recorded, so the next property got the same offset and the
 * reader rejected the whole section as corrupt.  Such a property must
 * simply be left out.
 */
static void
test_msole_unsupported_type_is_skipped (void)
{
	GsfDocMetaData *md = gsf_doc_meta_data_new ();
	GsfDocMetaData *md2 = gsf_doc_meta_data_new ();
	GsfOutput *out = gsf_output_memory_new ();
	GValue *odd = g_new0 (GValue, 1);
	GsfInput *in;
	GsfDocProp *prop;
	GError *err;
	gboolean ok;

	set_int_prop (md, GSF_META_NAME_CODEPAGE, 1200);
	g_value_init (odd, G_TYPE_INT64);
	g_value_set_int64 (odd, 7);
	gsf_doc_meta_data_insert (md, g_strdup ("Odd"), odd);
	set_int_prop (md, "Keep", 42);

	inhibit_g_log_messages ();
	ok = gsf_doc_meta_data_write_to_msole (md, out, TRUE);
	restore_g_log_messages ();
	check (ok, "writing a property set with an unsupported type succeeds");

	in = gsf_input_memory_new_clone
		(gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (out)),
		 gsf_output_size (out));
	inhibit_g_log_messages ();
	err = gsf_doc_meta_data_read_from_msole (md2, in);
	restore_g_log_messages ();
	check (err == NULL, "the result can be read back");
	if (err != NULL)
		g_error_free (err);
	prop = gsf_doc_meta_data_lookup (md2, "Keep");
	check (prop != NULL &&
	       G_VALUE_HOLDS_INT (gsf_doc_prop_get_val (prop)) &&
	       g_value_get_int (gsf_doc_prop_get_val (prop)) == 42,
	       "the neighbouring property survives");
	check (gsf_doc_meta_data_lookup (md2, "Odd") == NULL,
	       "the unsupported property is not written");

	g_object_unref (in);
	g_object_unref (md2);
	g_object_unref (out);
	g_object_unref (md);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	/* Keep output visible even when a check aborts the process. */
	setvbuf (stdout, NULL, _IONBF, 0);

	gsf_init ();

	test_lcid_table ();
	test_mac_codepage_iconv ();
	test_namespace_uris ();
	test_odf_parse_error ();
	test_vector_roundtrip ();
	test_many_user_props_write ();
	test_unset_timestamp_write ();
	test_odf_vector_under_standard_name ();
	test_odf_vector_views_agree ();
	test_odf_truncated_user_defined ();
	test_msole_write_to_failing_sink ();
	test_msole_unsupported_type_is_skipped ();

	gsf_shutdown ();
	return regress_exit_status ();
}
