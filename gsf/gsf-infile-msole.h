/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-msole.h: 
 *
 * Copyright (C) 2002-2003 Jody Goldberg (jody@gnome.org)
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

#ifndef GSF_INFILE_MSOLE_H
#define GSF_INFILE_MSOLE_H

#include <gsf/gsf.h>

G_BEGIN_DECLS

typedef struct _GsfInfileMSOle GsfInfileMSOle;

#define GSF_INFILE_MSOLE_TYPE        (gsf_infile_msole_get_type ())
#define GSF_INFILE_MSOLE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_INFILE_MSOLE_TYPE, GsfInfileMSOle))
#define GSF_IS_INFILE_MSOLE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_INFILE_MSOLE_TYPE))

GType gsf_infile_msole_get_type (void);
GsfInfileMSOle *gsf_infile_msole_new (GsfInput *source, GError **err);

G_END_DECLS

#endif /* GSF_INFILE_MSOLE_H */
