/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-priv.c:
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

#include <gsf-config.h>
#include <gsf/gsf-priv.h>
#include <gsf/gsf.h>

#include <gobject/gvaluecollector.h>
#include <glib/gi18n-lib.h>

struct _GsfParam {
	const gchar *name;
	GValue       value;
};

void
gsf_params_to_properties(GsfParam *params, size_t n_params,
			 const char ***p_names, GValue **p_values)
{
	size_t i;
	const char **names = g_new(const char *, n_params);
	GValue *values = g_new(GValue, n_params);

	for (i = 0; i < n_params; i++) {
		names[i] = params[i].name;
		values[i] = params[i].value;
	}

	*p_names = names;
	*p_values = values;
}

/* Largely a copy of g_object_new_valist.  */
void
gsf_prop_settings_collect_valist (GType object_type,
				  GsfParam **p_params,
				  size_t *p_n_params,
				  const gchar *first_property_name,
				  va_list var_args)
{
	GObjectClass *class;
	GsfParam *params = *p_params;
	const gchar *name;
	size_t n_params = *p_n_params;
	size_t n_alloced_params = n_params;  /* We might have more.  */

	g_return_if_fail (G_TYPE_IS_OBJECT (object_type));

	class = g_type_class_ref (object_type);

	name = first_property_name;
	while (name) {
		gchar *error = NULL;
		GParamSpec *pspec = g_object_class_find_property (class, name);
		if (!pspec) {
			g_warning ("%s: object class `%s' has no property named `%s'",
				   G_STRFUNC,
				   g_type_name (object_type),
				   name);
			break;
		}

		if (n_params >= n_alloced_params) {
			n_alloced_params += 16;
			params = g_renew (GsfParam, params, n_alloced_params);
		}
		params[n_params].name = name;
		params[n_params].value.g_type = 0;
		g_value_init (&params[n_params].value, G_PARAM_SPEC_VALUE_TYPE (pspec));
		G_VALUE_COLLECT (&params[n_params].value, var_args, 0, &error);
		if (error) {
			g_warning ("%s: %s", G_STRFUNC, error);
			g_free (error);
			g_value_unset (&params[n_params].value);
			break;
		}
		n_params++;
		name = va_arg (var_args, gchar*);
	}

	g_type_class_unref (class);

	*p_params = params;
	*p_n_params = n_params;
}

/* This is a vararg version of gsf_property_settings_collect_valist.  */
void
gsf_prop_settings_collect (GType object_type,
			   GsfParam **p_params,
			   size_t *p_n_params,
			   const gchar *first_property_name,
			   ...)
{
	va_list var_args;
	va_start (var_args, first_property_name);
	gsf_prop_settings_collect_valist (object_type, p_params, p_n_params, first_property_name, var_args);
	va_end (var_args);
}

const GsfParam *
gsf_prop_settings_find (const char *name,
			const GsfParam *params,
			size_t n_params)
{
	size_t i;

	for (i = 0; i < n_params; i++)
		if (g_str_equal (name, params[i].name))
			return params + i;

	return NULL;
}

void
gsf_prop_settings_free (GsfParam *params,
			size_t n_params)
{
	while (n_params--)
		g_value_unset (&params[n_params].value);
	g_free (params);
}
