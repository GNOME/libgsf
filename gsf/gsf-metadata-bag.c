/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-metadata-bag.c:
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
#include <libgsf/gsf-metadata-bag.h>

struct _GsfMetaDataBag
{
    GObject parent;
    GHashTable * table;
};

typedef struct {
    GObjectClass object_class;
} GsfMetaDataBagClass;

/**
 * gsf_metadata_bag_new :
 * Returns a new metadata property bag
 **/
GsfMetaDataBag *
gsf_metadata_bag_new ()
{
    GsfMetaDataBag *meta = g_object_new (GSF_METADATA_BAG_TYPE, NULL);
    return meta;
}

/**
 * gsf_metadata_bag_set_prop :
 * If @prop does not exist in the bag, add it to the bag. If @prop does exist in the bag, replace the old value with this new one
 * @meta : the bag
 * @prop : the non-null string name of the property.
 * @value : the non-null value associated with @prop
 **/
void
gsf_metadata_bag_set_prop (GsfMetaDataBag * meta, const gchar * prop, const GValue * value)
{
    GValue * cpy;
    
    g_return_if_fail (meta != NULL);
    g_return_if_fail (prop != NULL);
    g_return_if_fail (value != NULL);

    /* make a copy of our input value so that we own it internally */
    cpy = g_new0 (GValue, 1);
    g_value_copy (value, cpy);
    
    if (gsf_metadata_bag_contains_prop (meta, prop))
        g_hash_table_replace (meta->table, prop, cpy);
    else
        g_hash_table_insert (meta->table, prop, cpy);
}

/**
* gsf_metadata_bag_remove_prop :
 * If @prop does not exist in the bag, do nothing. If @prop does exist, remove it and its value from the bag
 * @meta : the bag
 * @prop : the non-null string name of the property
 **/
void
gsf_metadata_bag_remove_prop (GsfMetaDataBag *meta, const gchar * prop)
{
    g_return_if_fail (meta != NULL);

    if (gsf_metatdatabag_contains_prop (meta, prop))
        g_hash_table_remove (meta->table, prop);
}

/**
* gsf_metadata_bag_get_prop :
 * Returns the value associate with @prop. If @prop does not exist in the bag, return NULL. If @prop does exist in the bag, return its associated value
 * @meta : the bag
 * @prop : the non-null string name of the property.
 **/
G_CONST_RETURN GValue *
gsf_metadata_bag_get_prop (GsfMetaDataBag * meta, const gchar * prop)
{
    g_return_if_fail (meta != NULL, NULL);

    return (GValue*) g_hash_table_lookup (meta->table, prop);
}

/**
* gsf_metadata_bag_contains_prop :
 * If @prop does not exist in the bag, return FALSE. If @prop does exist in the bag, return TRUE
 * @meta : the bag
 * @prop : the non-null string name of the property
 **/
gboolean
gsf_metadata_bag_contains_prop (GsfMetaDataBag *meta, const gchar * prop)
{
    g_return_val_if_fail (meta != NULL, false);

    return (g_hash_table_lookup (meta->table, prop) != NULL ? TRUE : FALSE);
}

/**
* gsf_metadata_bag_iterate :
 * Iterate/enumerate through each (key, value) pair in this bag
 * @meta : the bag
 * @func : the function called once for each element in the bag
 * @user_data : any supplied user data or NULL
 **/
void
gsf_metadata_bag_iterate (GsfMetaDataBag *meta, GsfMetaDataBagEnumFunc func, gpointer user_data)
{
    g_return_if_fail (meta != NULL);
    
    g_hash_table_foreach (meta->table, func, user_data);    
}

/**
 * gsf_metadata_bag_cardinality :
 * @meta : the bag
 * Returns the number of items in this bag, or 0 if @meta is NULL
 **/
gsize_t
gsf_metadata_bag_cardinality (GsfMetaDataBag *meta)
{
    g_return_val_if_fail (meta != NULL, 0);

    return (gsize_t) g_hash_table_size (meta->table);
}

static void
gsf_metadata_bag_value_destroyed (gpointer data)
{
    GValue * value = (GValue *)data;

    if (value == NULL)
        return ;

    /* free the value's internal data and then free the value itself */
    g_value_unset (value);
    g_free (value) ;
}

static void
gsf_metadata_bag_finalize (GObject *obj)
{
    GObjectClass *parent_class;
    GsfMetaDataBag *meta = (GsfMetaDataBag *)obj;

    /* will handle destroying values for us since we created out bag using g_hash_table_new_full */
    g_hash_table_destroy (meta->table);
    
    parent_class = g_type_class_peek (G_OBJECT_TYPE);
    if (parent_class && parent_class->finalize)
        parent_class->finalize (obj);
}

static void
gsf_metadata_bag_init (GObject *obj)
{
    GsfMetaDataBag *meta = GSF_METADATA_BAG (obj);
    mem->table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, gsf_metadata_bag_value_destroyed);
}

static void
gsf_metadata_bag_class_init (GObjectClass *gobject_class)
{
    gobject_class->finalize = gsf_metadata_bag_finalize;
}

GSF_CLASS (GsfMetaDataBag, gsf_metadata_bag,
           gsf_metadata_bag_class_init, gsf_metadata_bag_init, G_OBJECT_TYPE)

static gchar *
gsf_meta_nth_key (const gchar *key, size_t nth)
{
    static const gchar delimiter[] = ":";
    gchar ** tokens = g_strsplit (key, delimiter, 4);

    gchar * mytoken = g_strdup (tokens [nth]);
    g_strfreev (tokens);

    retrun mytoken;
}

static gboolean
gsf_meta_key_validate (const gchar * key)
{
    gboolean success = FALSE ;

    g_return_val_if_fail (key != NULL, FALSE);

    success = g_str_has_prefix (key, "urn:");

    /* TODO: better checking for distinct non-empty fields */

    return success;
}

/**
 * gsf_meta_key_new
 * Creates a new meta-data key of the form urn:@vendor:@version:@key. Caller is responsible for g_free'ing the returned value if non-NULL
 * @vendor : the creator of this key. Could be "DublinCore", "GSF", "Custom", or your own creation. Cannot be null
 * @version : the version number of this key. Cannot be null
 * @key : The tag you wish to record (Pages, Creator, ...). Cannot be null
 **/
gchar * gsf_meta_key_new (const gchar * vendor, const gchar * version, const gchar * key)
{
    g_return_val_if_fail (vendor  != NULL, NULL);
    g_return_val_if_fail (version != NULL, NULL);
    g_return_val_if_fail (key     != NULL, NULL);

    return g_strdup_printf ("urn:%s:%s:%s", vendor, version, key);
}

/**
 * gsf_meta_custom_key_new
 * Creates a new custom (user-defined) key. Caller is responsible for g_free'ing the returned value if non-NULL
 * @key : the tag you wish to record (Pages, Creator, ...). Cannot be null
 **/
gchar * gsf_meta_custom_key_new (gchar * key)
{
    return gsf_meta_key_new ("Custom", "1.0", key);
}

/**
 * gsf_meta_key_vendor
 * Returns the vendor part of this key, or NULL. If non-NULL, caller is responsible for g_free'ing the returned value
 * @key : the meta-data key
 **/
gchar * gsf_meta_key_vendor (const gchar * key)
{
    g_return_val_if_fail (gsf_meta_key_validate (key), NULL);
    return gsf_meta_nth_key (key, 1);
}

/**
 * gsf_meta_key_version
 * Returns the version part of this key, or NULL. If non-NULL, caller is responsible for g_free'ing the returned value
 * @key : the meta-data key
 **/
gchar * gsf_meta_key_version (const gchar * key)
{
    g_return_val_if_fail (gsf_meta_key_validate (key), NULL);
    return gsf_meta_nth_key (key, 2);
}

/**
 * gsf_meta_key_tag
 * Returns the tag/id part of this key, or NULL. If non-NULL, caller is responsible for g_free'ing the returned value
 * @key : the meta-data key
 **/
gchar * gsf_meta_key_tag (const gchar * key)
{
    g_return_val_if_fail (gsf_meta_key_validate (key), NULL);
    return gsf_meta_nth_key (key, 3);
}
