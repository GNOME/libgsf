/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-doc-meta-data.c: tests for gsf-doc-meta-data.c
 */

#include "libgsf-regress.h"

#include <gsf/gsf-doc-meta-data.h>
#include <gsf/gsf-meta-names.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Basic CRUD: insert, lookup, size, remove.  Note that
 * gsf_doc_meta_data_insert takes ownership of both the name and the
 * GValue pointer.
 */
static void
test_basic_crud (void)
{
	GsfDocMetaData *md = gsf_doc_meta_data_new ();
	GsfDocProp *prop;
	GValue *val;

	check (md != NULL, "metadata created");
	if (md == NULL)
		return;

	check (gsf_doc_meta_data_size (md) == 0, "empty collection has size 0");
	check (gsf_doc_meta_data_lookup (md, "nonexistent") == NULL,
	       "lookup of a missing property returns NULL");

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_STRING);
	g_value_set_string (val, "hello");
	gsf_doc_meta_data_insert (md, g_strdup ("MyTitle"), val);
	check (gsf_doc_meta_data_size (md) == 1, "size is 1 after insert");

	prop = gsf_doc_meta_data_lookup (md, "MyTitle");
	check (prop != NULL, "property found after insert");
	check (prop != NULL && 0 == strcmp (gsf_doc_prop_get_name (prop), "MyTitle"),
	       "property name matches");
	check (prop != NULL && gsf_doc_prop_get_val (prop) != NULL,
	       "property has a value");
	check (prop != NULL && gsf_doc_prop_get_val (prop) != NULL &&
	       0 == strcmp (g_value_get_string (gsf_doc_prop_get_val (prop)),
			    "hello"),
	       "property value matches");

	gsf_doc_meta_data_remove (md, "MyTitle");
	check (gsf_doc_meta_data_size (md) == 0, "size is 0 after remove");
	check (gsf_doc_meta_data_lookup (md, "MyTitle") == NULL,
	       "lookup after remove returns NULL");

	/* removing a nonexistent property is a no-op */
	gsf_doc_meta_data_remove (md, "MyTitle");
	check (gsf_doc_meta_data_size (md) == 0, "remove of missing is a no-op");

	g_object_unref (md);
}

/*
 * Inserting with the same name replaces the old value and loses the
 * link.
 */
static void
test_insert_replaces (void)
{
	GsfDocMetaData *md = gsf_doc_meta_data_new ();
	GValue *val1, *val2;
	GsfDocProp *prop;

	check (md != NULL, "metadata created");
	if (md == NULL)
		return;

	val1 = g_new0 (GValue, 1);
	g_value_init (val1, G_TYPE_STRING);
	g_value_set_string (val1, "first");
	gsf_doc_meta_data_insert (md, g_strdup ("Key"), val1);

	val2 = g_new0 (GValue, 1);
	g_value_init (val2, G_TYPE_INT);
	g_value_set_int (val2, 42);
	gsf_doc_meta_data_insert (md, g_strdup ("Key"), val2);

	check (gsf_doc_meta_data_size (md) == 1,
	       "re-insert does not increase the size");

	prop = gsf_doc_meta_data_lookup (md, "Key");
	check (prop != NULL && gsf_doc_prop_get_val (prop) != NULL,
	       "the replaced property is still found");
	check (prop != NULL && G_VALUE_HOLDS_INT (gsf_doc_prop_get_val (prop)),
	       "the new value type is INT");
	check (prop != NULL &&
	       g_value_get_int (gsf_doc_prop_get_val (prop)) == 42,
	       "the new value is 42");

	g_object_unref (md);
}

static void
cb_collect_name (char const *name, GsfDocProp *prop G_GNUC_UNUSED,
		 GPtrArray *names)
{
	g_ptr_array_add (names, g_strdup (name));
}

/*
 * foreach iterates in sorted order by property name.
 */
static void
test_foreach_sorted (void)
{
	GsfDocMetaData *md = gsf_doc_meta_data_new ();
	GPtrArray *names = g_ptr_array_new_with_free_func (g_free);
	unsigned i;
	gboolean sorted = TRUE;

	check (md != NULL, "metadata created");
	if (md == NULL) {
		g_ptr_array_unref (names);
		return;
	}

	/* Insert in non-sorted order. */
	{
		char const *insert_order[] = { "Charlie", "Alpha", "Bravo" };
		unsigned j;
		for (j = 0; j < 3; j++) {
			GValue *v = g_new0 (GValue, 1);
			g_value_init (v, G_TYPE_INT);
			g_value_set_int (v, (int)j);
			gsf_doc_meta_data_insert (md, g_strdup (insert_order[j]), v);
		}
	}

	check (gsf_doc_meta_data_size (md) == 3, "3 properties stored");

	gsf_doc_meta_data_foreach (md,
		(GHFunc)(void (*)(void))cb_collect_name, names);

	check (names->len == 3, "foreach visited 3 properties");
	for (i = 1; i < names->len; i++) {
		if (strcmp (g_ptr_array_index (names, i - 1),
			    g_ptr_array_index (names, i)) > 0)
			sorted = FALSE;
	}
	check (sorted, "foreach visits properties in sorted order");
	check (names->len >= 3 &&
	       0 == strcmp (g_ptr_array_index (names, 0), "Alpha"),
	       "first property is 'Alpha'");

	g_ptr_array_unref (names);
	g_object_unref (md);
}

/*
 * Steal removes the property from the collection and returns it; store
 * puts it back.
 */
static void
test_steal_and_store (void)
{
	GsfDocMetaData *md = gsf_doc_meta_data_new ();
	GsfDocProp *prop;
	GValue *val;

	check (md != NULL, "metadata created");
	if (md == NULL)
		return;

	val = g_new0 (GValue, 1);
	g_value_init (val, G_TYPE_STRING);
	g_value_set_string (val, "stolen value");
	gsf_doc_meta_data_insert (md, g_strdup ("Stolen"), val);
	check (gsf_doc_meta_data_size (md) == 1, "size is 1 before steal");

	prop = gsf_doc_meta_data_steal (md, "Stolen");
	check (prop != NULL, "steal returns the property");
	check (gsf_doc_meta_data_size (md) == 0,
	       "size is 0 after steal");
	check (gsf_doc_meta_data_lookup (md, "Stolen") == NULL,
	       "lookup after steal returns NULL");

	check (prop != NULL && gsf_doc_prop_get_val (prop) != NULL &&
	       0 == strcmp (g_value_get_string (gsf_doc_prop_get_val (prop)),
			    "stolen value"),
	       "the stolen property retains its value");

	/* Re-store it. */
	gsf_doc_meta_data_store (md, prop);
	check (gsf_doc_meta_data_size (md) == 1, "size is 1 after re-store");
	check (gsf_doc_meta_data_lookup (md, "Stolen") != NULL,
	       "the property is found after re-store");

	g_object_unref (md);
}

/*
 * Property value: set_val takes ownership and replaces the old value;
 * swap_val returns the old and installs the new.
 */
static void
test_prop_val (void)
{
	GsfDocProp *prop = gsf_doc_prop_new (g_strdup ("TestProp"));
	GValue *v1, *v2, *old;

	check (prop != NULL, "property created");
	if (prop == NULL)
		return;

	check (gsf_doc_prop_get_val (prop) == NULL,
	       "a fresh property has no value");

	/* set_val */
	v1 = g_new0 (GValue, 1);
	g_value_init (v1, G_TYPE_INT);
	g_value_set_int (v1, 1);
	gsf_doc_prop_set_val (prop, v1);
	check (gsf_doc_prop_get_val (prop) == v1,
	       "set_val stores the pointer");
	check (g_value_get_int (gsf_doc_prop_get_val (prop)) == 1,
	       "set_val stores the value");

	/* set_val replaces: the old value is unset and freed */
	v2 = g_new0 (GValue, 1);
	g_value_init (v2, G_TYPE_INT);
	g_value_set_int (v2, 2);
	gsf_doc_prop_set_val (prop, v2);
	check (gsf_doc_prop_get_val (prop) == v2, "set_val replaced the value");
	check (g_value_get_int (gsf_doc_prop_get_val (prop)) == 2,
	       "the new value is 2");

	/* swap_val returns the old and installs the new */
	v1 = g_new0 (GValue, 1);
	g_value_init (v1, G_TYPE_INT);
	g_value_set_int (v1, 3);
	old = gsf_doc_prop_swap_val (prop, v1);
	check (old == v2, "swap_val returned the old value");
	check (g_value_get_int (gsf_doc_prop_get_val (prop)) == 3,
	       "the swapped-in value is 3");

	/* Clean up the swapped-out value manually.  */
	g_value_unset (old);
	g_free (old);

	/* set_val with NULL clears the value.  */
	gsf_doc_prop_set_val (prop, NULL);
	check (gsf_doc_prop_get_val (prop) == NULL, "set_val(NULL) clears");

	gsf_doc_prop_free (prop);
}

/*
 * Property link: set/get/clear.
 */
static void
test_prop_link (void)
{
	GsfDocProp *prop = gsf_doc_prop_new (g_strdup ("LinkedProp"));

	check (prop != NULL, "property created");
	if (prop == NULL)
		return;

	check (gsf_doc_prop_get_link (prop) == NULL, "no link initially");

	gsf_doc_prop_set_link (prop, g_strdup ("some-other-prop"));
	check (gsf_doc_prop_get_link (prop) != NULL &&
	       0 == strcmp (gsf_doc_prop_get_link (prop), "some-other-prop"),
	       "the link is stored");

	/* Set the same link again — should not corrupt.  */
	gsf_doc_prop_set_link (prop, g_strdup ("some-other-prop"));
	check (gsf_doc_prop_get_link (prop) != NULL &&
	       0 == strcmp (gsf_doc_prop_get_link (prop), "some-other-prop"),
	       "the link is still correct after re-set");

	/* Clear the link.  */
	gsf_doc_prop_set_link (prop, NULL);
	check (gsf_doc_prop_get_link (prop) == NULL, "the link can be cleared");

	gsf_doc_prop_free (prop);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();

	test_basic_crud ();
	test_insert_replaces ();
	test_foreach_sorted ();
	test_steal_and_store ();
	test_prop_val ();
	test_prop_link ();

	gsf_shutdown ();
	return regress_exit_status ();
}
