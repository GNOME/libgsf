/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output.h: interface for storing data
 *
 * Copyright (C) 2002-2004 Jody Goldberg (jody@gnome.org)
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

#ifndef GSF_OUTPUT_H
#define GSF_OUTPUT_H

#include <gsf/gsf.h>
#include <sys/types.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GSF_OUTPUT_TYPE        (gsf_output_get_type ())
#define GSF_OUTPUT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_OUTPUT_TYPE, GsfOutput))
#define GSF_IS_OUTPUT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_OUTPUT_TYPE))

GType gsf_output_get_type (void);

char const   *gsf_output_name	   (GsfOutput const *output);
GsfOutfile   *gsf_output_container (GsfOutput const *output);
GError const *gsf_output_error	   (GsfOutput const *output);

gsf_off_t     gsf_output_size      (GsfOutput *output);
gboolean      gsf_output_close     (GsfOutput *output);
gboolean      gsf_output_is_closed (GsfOutput const *output);
gsf_off_t     gsf_output_tell      (GsfOutput *output);
gboolean      gsf_output_seek      (GsfOutput *output,
				    gsf_off_t offset, GSeekType whence);
gboolean      gsf_output_write     (GsfOutput *output,
				    size_t num_bytes, guint8 const *data);

gboolean gsf_output_wrap   (GObject *wrapper, GsfOutput *wrapee);
gboolean gsf_output_unwrap (GObject *wrapper, GsfOutput *wrapee);

GQuark gsf_output_error_id (void);

gboolean gsf_output_printf (GsfOutput *output, char const *format,
			    ...) G_GNUC_PRINTF (2, 3);
gboolean gsf_output_puts (GsfOutput *output, char const *line);

G_END_DECLS

#endif /* GSF_OUTPUT_H */
