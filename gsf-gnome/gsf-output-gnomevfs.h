/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-gnomevfs.h: gnomevfs based output
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

#ifndef GSF_OUTPUT_GNOMEVFS_H
#define GSF_OUTPUT_GNOMEVFS_H

#include <gsf/gsf-output.h>

G_BEGIN_DECLS

#define GSF_OUTPUT_GNOMEVFS_TYPE        (gsf_output_gnomevfs_get_type ())
#define GSF_OUTPUT_GNOMEVFS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_OUTPUT_GNOMEVFS_TYPE, GsfOutputGnomeVFS))
#define GSF_IS_OUTPUT_GNOMEVFS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_OUTPUT_GNOMEVFS_TYPE))

typedef struct _GsfOutputGnomeVFS GsfOutputGnomeVFS;

GType     gsf_output_gnomevfs_get_type (void);
GsfOutput *gsf_output_gnomevfs_new      (char const *filename, GError **err);

G_END_DECLS

#endif /* GSF_OUTPUT_GNOMEVFS_H */
