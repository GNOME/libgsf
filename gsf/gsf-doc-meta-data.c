/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-doc-meta-data.c:
 *
 * Copyright (C) 2002-2004 Dom Lachowicz (cinamod@hotmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf-doc-meta-data.h>
#include <gsf/gsf-impl-utils.h>

static GObjectClass *parent_class;

struct _GsfDocMetaData {
	GObject parent;

	GHashTable *table;
};

typedef struct {
	GObjectClass object_class;
} GsfDocMetaDataClass;

/**
 * gsf_doc_meta_data_new :
 *
 * Returns a new metadata property collection
 **/
GsfDocMetaData *
gsf_doc_meta_data_new (void)
{
	GsfDocMetaData *meta = g_object_new (GSF_DOC_META_DATA_TYPE, NULL);
	return meta;
}

/**
 * gsf_doc_meta_data_set_prop :
 * @meta : the collection
 * @name : the non-null string name of the property.
 * @value : the non-null value associated with @name
 *
 * If @name does not exist in the collection, add it to the collection. If
 * @name does exist in the collection, replace the old value with this new one
 **/
void
gsf_doc_meta_data_set_prop (GsfDocMetaData *meta,
			    char const *name, GValue const *value)
{
	GValue *cpy;

	g_return_if_fail (meta != NULL);
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);

	/* make a copy of our input value so that we own it internally */
	cpy = g_new0 (GValue, 1);
	g_value_init (cpy, G_VALUE_TYPE (value));
	g_value_copy (value, cpy);
	g_hash_table_replace (meta->table, g_strdup (name), cpy);
}

/**
 * gsf_doc_meta_data_remove_prop :
 * @meta : the collection
 * @name : the non-null string name of the property
 *
 * If @name does not exist in the collection, do nothing. If @name does exist,
 * remove it and its value from the collection
 **/
void
gsf_doc_meta_data_remove_prop (GsfDocMetaData *meta, char const *name)
{
	g_return_if_fail (meta != NULL);

	g_hash_table_remove (meta->table, name);
}


/**
 * gsf_get_prop_val :
 * @prop : the property
 *
 * Returns the value associated with @prop. If @prop is NULL, 
 * return NULL. If @prop does exist in the collection,
 * return its associated value.
 **/
GValue const*
gsf_get_prop_val (GsfDocProp const *prop)
{

	g_return_val_if_fail (prop != NULL, NULL);
	return (prop->val);
}

/**
 * gsf_get_prop_val_str :
 * @prop : the property
 *
 * Returns the value associated with @prop. If @prop is NULL,
 * return NULL. If @prop does exist in the collection,
 * return its associated value.
 **/
gchar *
gsf_get_prop_val_str (GsfDocProp const *prop)
{

	g_return_val_if_fail (prop != NULL, NULL);
	return (g_strdup_value_contents (prop->val));
}

/**
 * gsf_doc_meta_data_get_prop :
 * @meta : the collection
 * @name : the non-null string name of the property.
 *
 * Returns the prop associated with @name. If @name does not exist in the
 * collection, return NULL. If @name does exist in the collection, return 
 * a GsfDocProp pointer.  Callers are responsible for releasing the result
 * and its allocated members. ;-)
 **/
GsfDocProp *
gsf_doc_meta_data_get_prop (GsfDocMetaData const *meta, char const *name)
{
	GsfDocProp *prop = NULL;
	GValue *val = NULL;

	g_return_val_if_fail (meta != NULL, NULL);
	val = g_hash_table_lookup (meta->table, name);
	if (G_IS_VALUE (val)) {
		prop = g_new (GsfDocProp, 1);
		prop->val = g_new0 (GValue, 1);
		g_value_init (prop->val, G_VALUE_TYPE (val)); 
		g_value_copy (val, prop->val);
		prop->name = g_strdup (name);
		prop->linked_to = NULL;
	}
	return prop;
}

/**
 * gsf_doc_meta_data_foreach :
 * @meta : the collection
 * @func : the function called once for each element in the collection
 * @user_data : any supplied user data or NULL
 *
 * Iterate through each (key, value) pair in this collection
 **/
void
gsf_doc_meta_data_foreach (GsfDocMetaData const *meta, GHFunc func, gpointer user_data)
{
	g_return_if_fail (meta != NULL);

	g_hash_table_foreach (meta->table, func, user_data);
}

/**
 * gsf_doc_meta_data_size :
 * @meta : the collection
 *
 * Returns the number of items in this collection
 **/
gsize
gsf_doc_meta_data_size (GsfDocMetaData const *meta)
{
	g_return_val_if_fail (meta != NULL, 0);

	return (gsize) g_hash_table_size (meta->table);
}

static void
gsf_doc_meta_data_value_destroyed (gpointer data)
{
	GValue *value = (GValue *)data;

	if (value == NULL)
		return;

	/* free the value's internal data and then free the value itself */
	g_value_unset (value);
	g_free (value);
}

static void
gsf_doc_meta_data_finalize (GObject *obj)
{
	/* will handle destroying values for us since we created out
	 * collection using g_hash_table_new_full
	 */
	g_hash_table_destroy (GSF_DOC_META_DATA (obj)->table);
	parent_class->finalize (obj);
}

static void
gsf_doc_meta_data_init (GObject *obj)
{
	GsfDocMetaData *meta = GSF_DOC_META_DATA (obj);
	meta->table = g_hash_table_new_full (g_str_hash, g_str_equal,
		g_free, gsf_doc_meta_data_value_destroyed);
}

static void
gsf_doc_meta_data_class_init (GObjectClass *gobject_class)
{
	gobject_class->finalize = gsf_doc_meta_data_finalize;
	parent_class = g_type_class_peek_parent (gobject_class);
}

GSF_CLASS (GsfDocMetaData, gsf_doc_meta_data,
	   gsf_doc_meta_data_class_init, gsf_doc_meta_data_init,
	   G_TYPE_OBJECT)
