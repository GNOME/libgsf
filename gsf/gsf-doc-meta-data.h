/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-metadata-bag.h: get, set, remove custom meta properties associated with documents
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

#ifndef GSF_METADATA_BAG_H
#define GSF_METADATA_BAG_H

#include <gsf/gsf.h>
#include <sys/types.h>

G_BEGIN_DECLS

#define GSF_METADATA_BAG_TYPE        (gsf_metadata_bag_get_type ())
#define GSF_METADATA_BAG(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_METADATA_BAG_TYPE, GsfMetaDataBag))
#define GSF_IS_METADATA_BAG(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_METADATA_BAG_TYPE))

GType  gsf_metadata_bag_get_type (void);
GQuark gsf_metadata_bag_error (void);

typedef struct _GsfMetaDataBag GsfMetaDataBag;
typedef GHFunc GMetaDataBagEnumFunc;

/*
 * TODO 1: make the GsfInFile and GsfOutFile classes each contain a GsfMetaDataBag
 * TODO 2: make the GsfInFile subclasses actually fill in the metadata bags (OLE2, OpenOffice, ...)
 * TODO 3: make the GsfOutFile subclasses actually write out the metadata bags (OLE2, OpenOffice, ...)
 *
 * TODO 4: figure out how to put metadata into the *gzip* layer when
 *         the underlying file format cannot handle it itself, such
 *         as for .gnumeric (which is a gzip'd text file.) -- MW
 *
 * Proposed schema so that we can have some accessor and compose routines (getvendorname, getversion, gettag):
 * urn:VendorName:Version:Tag
 * Proposed default vendor names:
 * DublinCore (http://dublincore.org)
 * GSF (ourselves, remaining properties from OLE that don't map cleanly onto DublinCore)
 * Custom (user-defined namespace)
 *
 */

#include <libgsf/gsf-meta-keys.h>

GsfMetaDataBag * gsf_metadata_bag_new (void);
void             gsf_metadata_bag_set_prop (GsfMetaDataBag * meta, const gchar * prop, const GValue * value);
void             gsf_metadata_bag_remove_prop (GsfMetaDataBag *meta, const gchar * prop);
G_CONST_RETURN GValue *         gsf_metadata_bag_get_prop (GsfMetaDataBag * meta, const gchar * prop);
gboolean         gsf_metadata_bag_contains_prop (GsfMetaDataBag *meta, const gchar * prop);
void             gsf_metadata_bag_iterate (GsfMetaDataBag *meta, GsfMetaDataBagEnumFunc func, gpointer user_data);
gsize_t          gsf_metadata_bag_cardinality (GsfMetaDataBag *meta);

gchar * gsf_meta_key_new (const gchar * vendor, const gchar * version, const gchar * key);
gchar * gsf_meta_custom_key_new (gchar * key);
gchar * gsf_meta_key_vendor (const gchar * key);
gchar * gsf_meta_key_version (const gchar * key);
gchar * gsf_meta_key_tag (const gchar * key);

G_END_DECLS

#endif /* GSF_METADATA_BAG_H */
