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

#ifndef GSF_INPUT_H
#define GSF_INPUT_H

#include <gsf/gsf.h>
#include <sys/types.h>

G_BEGIN_DECLS

#define GSF_INPUT_TYPE        (gsf_input_get_type ())
#define GSF_INPUT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_INPUT_TYPE, GsfInput))
#define IS_GSF_INPUT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_INPUT_TYPE))

GType gsf_input_get_type (void);

char const   *gsf_input_name	  (GsfInput *input);
GsfInfile    *gsf_input_container (GsfInput *input);

GsfInput     *gsf_input_dup	  (GsfInput *input);
ssize_t	      gsf_input_size	  (GsfInput *input);
gboolean      gsf_input_eof	  (GsfInput *file);
guint8 const *gsf_input_read	  (GsfInput *input, size_t num_bytes,
				   guint8 *optional_buffer);
size_t        gsf_input_tell	  (GsfInput *input);
gboolean      gsf_input_seek	  (GsfInput *input,
				   off_t offset, GsfOff_t whence);

GQuark gsf_input_error (void);

G_END_DECLS

#endif /* GSF_INPUT_H */
