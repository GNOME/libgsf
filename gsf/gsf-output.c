/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output.c: interface for used by the ole layer to read raw data
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
#include <gsf/gsf-output-impl.h>
#include <gsf/gsf-impl-utils.h>

#define GET_CLASS(instance) G_TYPE_INSTANCE_GET_CLASS (instance, GSF_OUTPUT_TYPE, GsfOutputClass)

static void
gsf_output_finalize (GObject *obj)
{
	GsfOutput *output = GSF_OUTPUT (obj);
	if (output->name != NULL) {
		g_free (output->name);
		output->name = NULL;
	}
	if (output->container != NULL) {
		g_object_unref (G_OBJECT (output->container));
		output->container = NULL;
	}
}

static void
gsf_output_init (GObject *obj)
{
	GsfOutput *output = GSF_OUTPUT (obj);

	output->cur_offset = 0;
	output->name = NULL;
	output->container = NULL;
}

static void
gsf_output_class_init (GObjectClass *gobject_class)
{
	gobject_class->finalize = gsf_output_finalize;
}

GSF_CLASS_ABSTRACT (GsfOutput, gsf_output,
		    gsf_output_class_init, gsf_output_init,
		    G_TYPE_OBJECT)

gboolean
gsf_output_close (GsfOutput *output)
{
	g_return_val_if_fail (output != NULL, FALSE);

	return GET_CLASS (output)->close (output);
}

int
gsf_output_tell	(GsfOutput *output)
{
	g_return_val_if_fail (output != NULL, 0);

	return output->cur_offset;
}

gboolean
gsf_output_seek	(GsfOutput *output, int offset, GsfOff_t whence)
{
	g_return_val_if_fail (output != NULL, -1);

	offset = GET_CLASS (output)->seek (output, offset, whence);
	if (offset < 0)
		return TRUE;
	output->cur_offset = offset;
	return FALSE;
}

gboolean
gsf_output_write (GsfOutput *output,
		  guint8 const *data, int num_bytes)
{
	g_return_val_if_fail (output != NULL, FALSE);

	return GET_CLASS (output)->write (output, data, num_bytes);
}

GQuark 
gsf_output_error (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gsf_output_error");
	return quark;
}

