/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-meta_data-bag.h: get, set, remove custom meta properties associated with documents
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

#ifndef GSF_DOC_META_DATA_H
#define GSF_DOC_META_DATA_H

#include <gsf/gsf.h>
#include <sys/types.h>

G_BEGIN_DECLS

struct _GsfDocProp {
	char const *name;
	GValue *val;
	char const *linked_to; /* optionally NULL */
};

#define GSF_DOC_META_DATA_TYPE        (gsf_doc_meta_data_get_type ())
#define GSF_DOC_META_DATA(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_DOC_META_DATA_TYPE, GsfDocMetaData))
#define GSF_IS_DOC_META_DATA(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_DOC_META_DATA_TYPE))

GType  gsf_doc_meta_data_get_type (void);
GQuark gsf_doc_meta_data_error (void);

GsfDocMetaData	 *gsf_doc_meta_data_new		(void);
GsfDocProp const *gsf_doc_meta_data_get_prop	(GsfDocMetaData *meta, char const *prop_name);
void		  gsf_doc_meta_data_set_prop	(GsfDocMetaData *meta, GsfDocProp *prop);
void		  gsf_doc_meta_data_remove_prop	(GsfDocMetaData *meta, char const *prop);
void		  gsf_doc_meta_data_foreach	(GsfDocMetaData *meta, GHFunc func, gpointer user_data);
int		  gsf_doc_meta_data_size	(GsfDocMetaData *meta);

G_END_DECLS

#endif /* GSF_DOC_META_DATA_H */
