/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-outfile-zip.h: interface for zip archive output.
 *
 * Copyright (C) 2002 Jon K Hellan (hellan@acm.org)
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

#ifndef GSF_OUTFILE_ZIP_H
#define GSF_OUTFILE_ZIP_H

#include <gsf/gsf.h>

G_BEGIN_DECLS

typedef struct _GsfOutfileZip GsfOutfileZip;

#define GSF_OUTFILE_ZIP_TYPE	(gsf_outfile_zip_get_type ())
#define GSF_OUTFILE_ZIP(o)	(G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_OUTFILE_ZIP_TYPE, GsfOutfileZip))
#define GSF_IS_OUTFILE_ZIP(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_OUTFILE_ZIP_TYPE))

GType gsf_outfile_zip_get_type (void);
GsfOutfile *gsf_outfile_zip_new (GsfOutput *sink, GError **err);

G_END_DECLS

#endif /* GSF_OUTFILE_H */
