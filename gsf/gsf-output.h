/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output.h: interface for storing data
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

#ifndef GSF_OUTPUT_H
#define GSF_OUTPUT_H

#include <gsf/gsf.h>

G_BEGIN_DECLS

#define GSF_OUTPUT_TYPE        (gsf_output_get_type ())
#define GSF_OUTPUT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_OUTPUT_TYPE, GsfOutput))
#define IS_GSF_OUTPUT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_OUTPUT_TYPE))

GType gsf_output_get_type (void);

gboolean gsf_output_close (GsfOutput *output);
int      gsf_output_tell  (GsfOutput *output);
gboolean gsf_output_seek  (GsfOutput *output,
			   int offset, GsfOff_t whence);
gboolean gsf_output_write (GsfOutput *output,
			   guint8 const *data, int num_bytes);

GQuark gsf_output_error (void);

G_END_DECLS

#endif /* GSF_OUTPUT_H */
