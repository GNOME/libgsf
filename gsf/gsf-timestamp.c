/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-timestamp.c: 
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

#include <gsf-config.h>
#include <gsf/gsf-timestamp.h>
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
gsf_timestamp_copy (GsfTimestamp const *stamp)
{
	GsfTimestamp *res = g_new0 (GsfTimestamp, 1);
	res->timet = stamp->timet;
	return res;
}

void
gsf_timestamp_free (GsfTimestamp *stamp)
{
	g_free (stamp);
}

int
gsf_timestamp_parse (char const *spec, GsfTimestamp *stamp)
{
	return 0;
}

char *
gsf_timestamp_as_string	(GsfTimestamp const *stamp)
{
	g_return_val_if_fail (stamp != NULL, g_strdup ("<invalid>"));
	return g_strdup (ctime (&stamp->timet));
}

guint
gsf_timestamp_hash (GsfTimestamp const *stamp)
{
	return stamp->timet;
}

gboolean
gsf_timestamp_equal (GsfTimestamp const *a, GsfTimestamp const *b)
{
	return a->timet == b->timet;
}

void
g_value_set_timestamp (GValue *value, GsfTimestamp const *stamp)
{
	g_value_set_boxed (value, stamp);
}
