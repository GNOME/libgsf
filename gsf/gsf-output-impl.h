/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output.h: interface for used by the ole layer to read raw data
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

#ifndef GSF_OUTPUT_IMPL_H
#define GSF_OUTPUT_IMPL_H

#include <gsf/gsf.h>
#include <gsf/gsf-output.h>
#include <glib-object.h>

G_BEGIN_DECLS

struct _GsfOutput {
	GObject g_object;

	gsf_off_t   cur_size, cur_offset;
	char       *name;
	GsfOutput  *wrapped_by;
	GsfOutfile *container;
	gboolean    is_closed;
};

typedef struct {
	GObjectClass g_object_class;

	gboolean (*Close) (GsfOutput *output);
	gboolean (*Seek)  (GsfOutput *output,
			   gsf_off_t offset, GSeekType whence);
	gboolean (*Write) (GsfOutput *output,
			   size_t num_bytes, guint8 const *data);
} GsfOutputClass;

#define GSF_OUTPUT_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GSF_OUTPUT_TYPE, GsfOutputClass))
#define GSF_IS_OUTPUT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GSF_OUTPUT_TYPE))

/* protected */
gboolean gsf_output_set_name	  (GsfOutput *input, char const *name);
gboolean gsf_output_set_container (GsfOutput *input, GsfOutfile *container);

G_END_DECLS

#endif /* GSF_OUTPUT_IMPL_H */
