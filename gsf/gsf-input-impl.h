/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input.h: interface for used by the ole layer to read raw data
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

#ifndef GSF_INPUT_IMPL_H
#define GSF_INPUT_IMPL_H

#include <gsf/gsf.h>
#include <gsf/gsf-input.h>
#include <glib-object.h>

G_BEGIN_DECLS

struct _GsfInput {
	GObject g_object;

	unsigned   size, cur_offset;
	char      *name;
	GsfInfile *container;
};

typedef struct {
	GObjectClass g_object_class;

	GsfInput     *(*Dup)  (GsfInput *input);
	guint8 const *(*Read) (GsfInput *input, unsigned num_bytes,
			       guint8 *optional_buffer);
	gboolean      (*Seek) (GsfInput *input, int offset, GsfOff_t whence);
} GsfInputClass;

#define GSF_INPUT_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GSF_INPUT_TYPE, GsfInputClass))
#define IS_GSF_INPUT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GSF_INPUT_TYPE))

/* protected */
gboolean gsf_input_set_name	 (GsfInput *input, char const *name);
gboolean gsf_input_set_container (GsfInput *input, GsfInfile *container);
gboolean gsf_input_set_size	 (GsfInput *input, unsigned size);

G_END_DECLS

#endif /* GSF_INPUT_IMPL_H */
