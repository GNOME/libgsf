/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input-file.h: interface for used by the ole layer to read raw data
 *
 * Copyright (C) 2002 Jody Goldberg (jody@gnome.org)
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

#ifndef GSF_INPUT_GNOMEVFS_H
#define GSF_INPUT_GNOMEVFS_H

#include <gsf/gsf-input.h>

G_BEGIN_DECLS

#define MS_OLE_INPUT_GNOMEVFS_TYPE        (gsf_input_gnomevfs_get_type ())
#define MS_OLE_INPUT_GNOMEVFS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), MS_OLE_INPUT_GNOMEVFS_TYPE, GsfInputStdio))
#define IS_MS_OLE_INPUT_GNOMEVFS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), MS_OLE_INPUT_GNOMEVFS_TYPE))

typedef struct _GsfInputGnomeVFS GsfInputGnomeVFS;

GType	  gsf_input_gnomevfs_get_type (void);
GsfInput *gsf_input_gnomevfs_new      (char const *filename, GError **error);

G_END_DECLS

#endif /* MS_OLE_INPUT_GNOMEVFS_H */
