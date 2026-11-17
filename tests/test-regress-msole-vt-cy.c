/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-msole-vt-cy.c: fixed-size property types must consume exactly
 * their own size (VT_CY is 8 bytes, VT_BOOL a 16-bit word).
 *
 * msole_prop_parse is static, so this test includes the implementation file
 * and calls it directly.
 */

#include "../gsf/gsf-msole-utils.c"

#include "libgsf-regress.h"

static guint
vector_len (GValue const *res)
{
	return G_VALUE_HOLDS (res, GSF_DOCPROP_VECTOR_TYPE)
		? gsf_value_get_docprop_array (res)->len
		: 0;
}

/*
 * The VT_CY case read its value but never advanced the data pointer, so the
 * element loop of a vector saw "no progress" after the first element and
 * stopped; in a VT_VARIANT vector the following elements were misparsed.
 */
static void
test_vt_cy_vector (void)
{
	guint8 buf[4 + 3 * 8];
	guint8 const *d = buf;
	GValue res = G_VALUE_INIT;
	int i;

	memset (buf, 0, sizeof buf);
	GSF_LE_SET_GUINT32 (buf, 3);
	for (i = 0; i < 3; i++)
		GSF_LE_SET_GINT64 (buf + 4 + 8 * i, 100 + i);

	check (msole_prop_parse (NULL, VT_VECTOR | VT_CY, &d, buf + sizeof buf,
				 &res, 0),
	       "VT_VECTOR|VT_CY parses");
	check (vector_len (&res) == 3, "all 3 VT_CY elements are read (got %u)",
	       vector_len (&res));
	check (d == buf + sizeof buf, "all %d bytes consumed (consumed %d)",
	       (int) sizeof buf, (int) (d - buf));
	if (G_IS_VALUE (&res))
		g_value_unset (&res);
}

static void
test_vt_cy_in_variant_vector (void)
{
	guint8 buf[4 + 2 * (4 + 8)];
	guint8 const *d = buf;
	GValue res = G_VALUE_INIT;
	int i;

	memset (buf, 0, sizeof buf);
	GSF_LE_SET_GUINT32 (buf, 2);
	for (i = 0; i < 2; i++) {
		GSF_LE_SET_GUINT32 (buf + 4 + 12 * i, VT_CY);
		GSF_LE_SET_GINT64 (buf + 8 + 12 * i, 7 + i);
	}

	check (msole_prop_parse (NULL, VT_VECTOR | VT_VARIANT, &d,
				 buf + sizeof buf, &res, 0),
	       "VT_VECTOR|VT_VARIANT of VT_CY parses");
	check (vector_len (&res) == 2, "both variant elements are read (got %u)",
	       vector_len (&res));
	check (d == buf + sizeof buf, "all %d bytes consumed (consumed %d)",
	       (int) sizeof buf, (int) (d - buf));
	if (G_IS_VALUE (&res))
		g_value_unset (&res);
}

static void
quiet (G_GNUC_UNUSED char const *d, G_GNUC_UNUSED GLogLevelFlags l,
       G_GNUC_UNUSED char const *m, G_GNUC_UNUSED gpointer u)
{
}

static int
bool_at (GValue const *res, unsigned i)
{
	GArray *a = gsf_value_get_docprop_array (res);
	GValue const *v = &g_array_index (a, GValue, i);
	return G_VALUE_HOLDS_BOOLEAN (v) ? (g_value_get_boolean (v) ? 1 : 0) : -1;
}

/*
 * VT_BOOL is a 16-bit VARIANT_BOOL, but the reader consumed one byte, so
 * every element of a VT_VECTOR|VT_BOOL was read from half the right offset
 * (true,false,true came back as true,true,false) and a non-zero word whose
 * low byte is zero was read as false.
 */
static void
test_vt_bool (void)
{
	guint8 vec[4 + 3 * 2];
	guint8 const *d = vec;
	GValue res = G_VALUE_INIT;
	guint8 word_0100[2] = { 0x00, 0x01 };
	guint8 word_ffff[2] = { 0xFF, 0xFF };
	guint8 word_0000[2] = { 0x00, 0x00 };
	guint8 one_byte[1] = { 0xFF };

	memset (vec, 0, sizeof vec);
	GSF_LE_SET_GUINT32 (vec, 3);
	GSF_LE_SET_GUINT16 (vec + 4, 0xFFFF);	/* true */
	GSF_LE_SET_GUINT16 (vec + 6, 0x0000);	/* false */
	GSF_LE_SET_GUINT16 (vec + 8, 0xFFFF);	/* true */

	check (msole_prop_parse (NULL, VT_VECTOR | VT_BOOL, &d, vec + sizeof vec,
				 &res, 0),
	       "VT_VECTOR|VT_BOOL parses");
	check (vector_len (&res) == 3, "all 3 booleans are read (got %u)",
	       vector_len (&res));
	if (vector_len (&res) == 3)
		check (bool_at (&res, 0) == 1 && bool_at (&res, 1) == 0 &&
		       bool_at (&res, 2) == 1,
		       "values are true,false,true (got %d,%d,%d)",
		       bool_at (&res, 0), bool_at (&res, 1), bool_at (&res, 2));
	check (d == vec + sizeof vec, "all %d bytes consumed (consumed %d)",
	       (int) sizeof vec, (int) (d - vec));
	if (G_IS_VALUE (&res))
		g_value_unset (&res);

#define SCALAR(bytes, want, what)						\
	do {									\
		guint8 const *p = (bytes);					\
		GValue r = G_VALUE_INIT;					\
		gboolean ok = msole_prop_parse (NULL, VT_BOOL, &p,		\
						(bytes) + sizeof (bytes), &r, 0);	\
		check (ok && G_VALUE_HOLDS_BOOLEAN (&r) &&			\
		       g_value_get_boolean (&r) == (want) &&			\
		       p == (bytes) + 2, what);					\
		if (G_IS_VALUE (&r))						\
			g_value_unset (&r);					\
	} while (0)
	SCALAR (word_ffff, TRUE,  "scalar 0xFFFF is true and uses 2 bytes");
	SCALAR (word_0000, FALSE, "scalar 0x0000 is false");
	SCALAR (word_0100, TRUE,  "non-zero word 0x0100 is true");
#undef SCALAR

	/* One byte is not a whole VT_BOOL. */
	{
		guint8 const *p = one_byte;
		GValue r = G_VALUE_INIT;
		g_log_set_default_handler (quiet, NULL);
		check (!msole_prop_parse (NULL, VT_BOOL, &p,
					  one_byte + sizeof one_byte, &r, 0),
		       "a single byte is rejected as truncated");
		g_log_set_default_handler (g_log_default_handler, NULL);
		if (G_IS_VALUE (&r))
			g_value_unset (&r);
	}
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();
	test_vt_cy_vector ();
	test_vt_cy_in_variant_vector ();
	test_vt_bool ();
	gsf_shutdown ();
	return regress_exit_status ();
}
