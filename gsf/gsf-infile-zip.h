/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-zip.h: 
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

#ifndef GSF_INFILE_ZIP_H
#define GSF_INFILE_ZIP_H

#include <gsf/gsf.h>

G_BEGIN_DECLS

typedef struct _GsfInfileZip GsfInfileZip;

#define GSF_INFILE_ZIP_TYPE        (gsf_infile_zip_get_type ())
#define GSF_INFILE_ZIP(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_INFILE_ZIP_TYPE, GsfInfileZip))
#define IS_GSF_INFILE_ZIP(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_INFILE_ZIP_TYPE))

GType gsf_infile_zip_get_type (void);

GsfInfile  *gsf_infile_zip_new	      (GsfInput *source, GError **err);
char const *gsf_infile_zip_get_ooname (GsfInput *input);

G_END_DECLS

#endif /* GSF_INFILE_ZIP_H */
