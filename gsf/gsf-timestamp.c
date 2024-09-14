/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-timestamp.c:
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
#include <gsf/gsf-timestamp.h>
#include <gsf/gsf.h>

#include <string.h>
#include <time.h>

static void
timestamp_to_string (GValue const *src_value, GValue *dest_value)
{
	char *str = gsf_timestamp_as_string (g_value_get_boxed (src_value));
	g_value_set_string (dest_value, str);
	g_free (str);
}

GType
gsf_timestamp_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0) {
		our_type = g_boxed_type_register_static ("GsfTimestamp",
					(GBoxedCopyFunc)gsf_timestamp_copy,
					(GBoxedFreeFunc)gsf_timestamp_free);
		g_value_register_transform_func	(our_type, G_TYPE_STRING,
			&timestamp_to_string);
	}
	return our_type;
}

GsfTimestamp *
gsf_timestamp_new (void)
{
	GsfTimestamp *res = g_new0 (GsfTimestamp, 1);
	res->timet = -1;
	return res;
}

/**
 * gsf_timestamp_copy:
 * @stamp: timestamp to be copied
 *
 * Copies a timestamp.
 *
 * Returns: a separate copy of @stamp.
 */
GsfTimestamp *
gsf_timestamp_copy (GsfTimestamp const *stamp)
{
	GsfTimestamp *res = gsf_timestamp_new ();
	res->timet = stamp->timet;
	return res;
}

/**
 * gsf_timestamp_free:
 * @stamp: timestamp to be freed
 *
 * Releases the memory in use for @stamp (if any).
 **/
void
gsf_timestamp_free (GsfTimestamp *stamp)
{
	g_free (stamp);
}

/**
 * gsf_timestamp_load_from_string:
 * @stamp: #GsfTimestamp
 * @spec: The string to parse
 *
 * Parser for time stamps.  Requires a ISO 8601 formatted string.
 *
 * Since: 1.14.24
 *
 * Returns: %TRUE on success
 **/
int
gsf_timestamp_load_from_string (GsfTimestamp *stamp, char const *spec)
{
	GTimeZone *utc;
	GDateTime *dt;

	utc = g_time_zone_new_utc ();
	dt = g_date_time_new_from_iso8601 (spec, utc);
	g_time_zone_unref (utc);

	if (!dt)
		return FALSE;

	stamp->timet = g_date_time_to_unix (dt);

	g_date_time_unref (dt);
	return TRUE;
}

/**
 * gsf_timestamp_from_string: (skip)
 * @spec: The string to parse
 * @stamp: #GsfTimestamp
 *
 * Parser for time stamps.  Requires a ISO 8601 formatted string.
 *
 * Deprecated: 1.14.24, use gsf_timestamp_load_from_string
 *
 * Returns: %TRUE on success
 **/
int
gsf_timestamp_from_string (char const *spec, GsfTimestamp *stamp)
{
	return gsf_timestamp_load_from_string (stamp, spec);
}

/**
 * gsf_timestamp_parse: (skip)
 * @spec: The string to parse
 * @stamp: #GsfTimestamp
 *
 * Parser for time stamps.  Requires a ISO 8601 formatted string.
 *
 * Deprecated: Use gsf_timestamp_load_from_string
 *
 * Returns: %TRUE on success
 **/
int
gsf_timestamp_parse (char const *spec, GsfTimestamp *stamp)
{
	return gsf_timestamp_load_from_string (stamp, spec);
}

/**
 * gsf_timestamp_as_string:
 * @stamp: timestamp to be converted.
 *
 * Produce a string representation (ISO 8601 format) of @stamp.
 *
 * Returns: a string representation of @stamp. When @stamp is invalid, the
 * representation is "&lt;invalid&gt;".
 */
char *
gsf_timestamp_as_string	(GsfTimestamp const *stamp)
{
	GDateTime *dt;
	char *iso8601_string;

	g_return_val_if_fail (stamp != NULL, g_strdup ("<invalid>"));

	dt = g_date_time_new_from_unix_utc (stamp->timet);
	if (!dt)
		return g_strdup ("<invalid>");

	iso8601_string = g_date_time_format_iso8601 (dt);
	g_date_time_unref (dt);

	return iso8601_string;
}

guint
gsf_timestamp_hash (GsfTimestamp const *stamp)
{
	return stamp->timet;
}

/**
 * gsf_timestamp_equal:
 * @a: a timestamp
 * @b: another timestamp
 *
 * Compare timestamps @a and @b.
 *
 * Returns: true if @a and @b represent the same point in time; false otherwise.
 *
 **/
gboolean
gsf_timestamp_equal (GsfTimestamp const *a, GsfTimestamp const *b)
{
	return a->timet == b->timet;
}

/**
 * gsf_timestamp_to_value:
 * @stamp: #GsfTimestamp
 * @value: #GValue
 *
 * Calls g_value_set_box (value, stamp);
 *
 * Since: 1.14.24
 **/
void
gsf_timestamp_to_value (GsfTimestamp const *stamp, GValue *value)
{
	g_value_set_boxed (value, stamp);
}

/**
 * gsf_value_set_timestamp: (skip)
 * @value: #GValue
 * @stamp: #GsfTimestamp
 *
 * Deprecated: 1.14.24, use gsf_timestamp_to_value.
 **/
void
gsf_value_set_timestamp (GValue *value, GsfTimestamp const *stamp)
{
	g_value_set_boxed (value, stamp);
}

void
gsf_timestamp_set_time (GsfTimestamp *stamp, guint64 t)
{
	stamp->timet = t;
}
