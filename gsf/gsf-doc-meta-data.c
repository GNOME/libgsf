/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-doc-meta-data.c:
 *
 * Copyright (C) 2002 Dom Lachowicz (cinamod@hotmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf-doc-meta-data.h>
#include <gsf/gsf-impl-utils.h>

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
 * @prop : the non-null string name of the property.
 * @value : the non-null value associated with @prop
 *
 * If @prop does not exist in the collection, add it to the collection. If
 * @prop does exist in the collection, replace the old value with this new one
 **/
void
gsf_doc_meta_data_set_prop (GsfDocMetaData * meta, const gchar * prop, const GValue * value)
{
	GValue *cpy;

	g_return_if_fail (meta != NULL);
	g_return_if_fail (prop != NULL);
	g_return_if_fail (value != NULL);

	/* make a copy of our input value so that we own it internally */
	cpy = g_new0 (GValue, 1);
	g_value_copy (value, cpy);
	g_hash_table_replace (meta->table, g_strdup (prop), cpy);
}

/**
 * gsf_doc_meta_data_remove_prop :
 * @meta : the collection
 * @prop : the non-null string name of the property
 *
 * If @prop does not exist in the collection, do nothing. If @prop does exist,
 * remove it and its value from the collection
 **/
void
gsf_doc_meta_data_remove_prop (GsfDocMetaData *meta, const gchar * prop)
{
	g_return_if_fail (meta != NULL);
	g_hash_table_remove (meta->table, prop);
}

/**
 * gsf_doc_meta_data_get_prop :
 * @meta : the collection
 * @prop : the non-null string name of the property.
 *
 * Returns the value associate with @prop. If @prop does not exist in the
 * collection, return NULL. If @prop does exist in the collection, return its
 * associated value
 **/
GValue const *
gsf_doc_meta_data_get_prop (GsfDocMetaData * meta, const gchar * prop)
{
	g_return_val_if_fail (meta != NULL, NULL);
	return g_hash_table_lookup (meta->table, prop);
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
gsf_doc_meta_data_foreach (GsfDocMetaData *meta, GHFunc func, gpointer data)
{
	g_return_if_fail (meta != NULL);

	g_hash_table_foreach (meta->table, func, data);
}

/**
 * gsf_doc_meta_data_size :
 * @meta : the collection
 *
 * Returns the number of items in this collection
 **/
int
gsf_doc_meta_data_size (GsfDocMetaData *meta)
{
	g_return_val_if_fail (meta != NULL, 0);

	return g_hash_table_size (meta->table);
}

static void
gsf_doc_meta_data_value_destroyed (gpointer data)
{
	GValue *value = (GValue *)data;

	if (value == NULL)
		return ;

	/* free the value's internal data and then free the value itself */
	g_value_unset (value);
	g_free (value) ;
}

static void
gsf_doc_meta_data_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfDocMetaData *meta = (GsfDocMetaData *)obj;

	/* will handle destroying values for us since we created out collection using g_hash_table_new_full */
	g_hash_table_destroy (meta->table);

	parent_class = g_type_class_peek (G_TYPE_OBJECT);
	if (parent_class && parent_class->finalize)
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
}

GSF_CLASS (GsfDocMetaData, gsf_doc_meta_data,
	   gsf_doc_meta_data_class_init, gsf_doc_meta_data_init, G_TYPE_OBJECT)
