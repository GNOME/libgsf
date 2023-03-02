/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-priv.h:
 *
 * Copyright (C) 2002-2006 Jody Goldberg (jody@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef GSF_PRIV_H
#define GSF_PRIV_H

#include <gsf/gsf-fwd.h>

G_BEGIN_DECLS

typedef struct _GsfParam GsfParam;

void        gsf_params_to_properties(GsfParam *params, size_t n_params,
				     const char ***p_names, GValue **p_values);

void        gsf_prop_settings_collect_valist (GType object_type,
						  GsfParam **p_params,
						  size_t *p_n_params,
						  const gchar *first_property_name,
						  va_list var_args);
void        gsf_prop_settings_collect (GType object_type,
				       GsfParam **p_params,
				       size_t *p_n_params,
				       const gchar *first_property_name,
				       ...);
const GsfParam *gsf_prop_settings_find (const char *name,
					const GsfParam *params,
					size_t n_params);
void        gsf_prop_settings_free (GsfParam *params,
				    size_t n_params);


G_END_DECLS

#endif /* GSF_PRIV_H */
