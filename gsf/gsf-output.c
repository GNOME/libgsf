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
	GsfOutput *output;

	output = GSF_OUTPUT (obj);

	/* it is too late to close things, we are partially destroyed.
	 * Keep this as a warning for silly mistakes
	 */
	if (!output->is_closed)
		g_warning ("unrefing an unclosed stream");
	
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

	output->cur_offset	= 0;
	output->cur_size	= 0;
	output->name		= NULL;
	output->container	= NULL;
	output->wrapped_by	= NULL;
	output->is_closed	= FALSE;
}

static void
gsf_output_class_init (GObjectClass *gobject_class)
{
	gobject_class->finalize = gsf_output_finalize;
}

GSF_CLASS_ABSTRACT (GsfOutput, gsf_output,
		    gsf_output_class_init, gsf_output_init,
		    G_TYPE_OBJECT)

/**
 * gsf_output_name :
 * @output : 
 *
 * Returns @output's name in utf8 form, DO NOT FREE THIS STRING
 **/
char const *
gsf_output_name (GsfOutput const *output)
{
	g_return_val_if_fail (GSF_IS_OUTPUT (output), NULL);
	return output->name;
}

/**
 * gsf_output_container :
 * @output : 
 *
 * Returns, but does not add a reference to @output's container.
 * Potentially NULL
 **/
GsfOutfile *
gsf_output_container (GsfOutput const *output)
{
	g_return_val_if_fail (GSF_IS_OUTPUT (output), NULL);
	return output->container;
}

/**
 * gsf_output_size :
 * @output :
 *
 * Returns the size of the output, or -1 if it does not have a size.
 **/
ssize_t
gsf_output_size (GsfOutput *output)
{
	g_return_val_if_fail (GSF_IS_OUTPUT (output), -1);
	return output->cur_size;
}

/**
 * gsf_output_close :
 * @output :
 *
 * Close a stream.
 * Returns TRUE if things are successful.
 **/
gboolean
gsf_output_close (GsfOutput *output)
{
	g_return_val_if_fail (output != NULL, FALSE);
	g_return_val_if_fail (!output->is_closed, FALSE);

	if (GET_CLASS (output)->Close (output)) {
		output->is_closed = TRUE;
		return TRUE;
	}
	return FALSE;
}

/**
 * gsf_output_is_closed :
 * @output :
 *
 * Returns TRUE if @output has already been closed.
 **/
gboolean
gsf_output_is_closed (GsfOutput const *output)
{
	g_return_val_if_fail (GSF_IS_OUTPUT (output), TRUE);
	return output->is_closed;
}

/**
 * gsf_output_tell :
 * @output :
 *
 * Returns the current position in the file
 **/
int
gsf_output_tell	(GsfOutput *output)
{
	g_return_val_if_fail (output != NULL, 0);

	return output->cur_offset;
}

/**
 * gsf_output_seek :
 * @output :
 * @offset :
 * @whence :
 *
 * Returns TRUE on error.
 **/
gboolean
gsf_output_seek	(GsfOutput *output, off_t offset, GsfOff_t whence)
{
	ssize_t pos = offset;

	g_return_val_if_fail (output != NULL, -1);

	switch (whence) {
	case GSF_SEEK_SET : break;
	case GSF_SEEK_CUR : pos += output->cur_offset;	break;
	case GSF_SEEK_END : pos += output->cur_size;	break;
	default : return TRUE;
	}

	if (pos < 0)
		return TRUE;

	/* If we go nowhere, just return.  This in particular handles null
	 * seeks for streams with no seek method.
	 */
	if ((size_t)pos == output->cur_offset)
		return FALSE;

	if (GET_CLASS (output)->Seek (output, offset, whence))
		return TRUE;

	output->cur_offset = pos;

	/* NOTE : it is possible for the current pos to be beyond the end of
	 * the file.  The intervening space is not filled with 0 until
	 * something is written.
	 */

	return FALSE;
}

/**
 * gsf_output_write :
 * @output :
 * @num_bytes :
 * @data :
 *
 * Returns FALSE on error.
 **/
gboolean
gsf_output_write (GsfOutput *output,
		  size_t num_bytes, guint8 const *data)
{
	g_return_val_if_fail (output != NULL, FALSE);

	if (num_bytes == 0)
		return TRUE;
	if (GET_CLASS (output)->Write (output, num_bytes, data)) {
		output->cur_offset += num_bytes;
		if (output->cur_size < (size_t)output->cur_offset)
			output->cur_size = output->cur_offset;
		return TRUE;
	}
	return FALSE;
}

/**
 * gsf_output_set_name :
 * @output :
 * @name :
 *
 * protected.
 *
 * Returns : TRUE if the assignment was ok.
 **/
gboolean
gsf_output_set_name (GsfOutput *output, char const *name)
{
	char *buf;

	g_return_val_if_fail (output != NULL, FALSE);

	buf = g_strdup (name);
	if (output->name != NULL)
		g_free (output->name);
	output->name = buf;
	return TRUE;
}

/**
 * gsf_output_set_container :
 * @output :
 * @container :
 *
 * Returns : TRUE if the assignment was ok.
 */
gboolean
gsf_output_set_container (GsfOutput *output, GsfOutfile *container)
{
	g_return_val_if_fail (output != NULL, FALSE);

	if (container != NULL)
		g_object_ref (G_OBJECT (container));
	if (output->container != NULL)
		g_object_unref (G_OBJECT (output->container));
	output->container = container;
	return TRUE;
}

static void
cb_output_wrap_screwup (gpointer wrapee, GObject *wrapper)
{
	g_warning ("%p died while still claiming ownership of %p",
		   wrapper, wrapee);
}

/**
 * gsf_output_wrap :
 * @wrapper :
 * @wrapee :
 *
 * Returns TRUE if the wrapping succeeded.
 **/
gboolean
gsf_output_wrap (GsfOutput *wrapper, GsfOutput *wrapee)
{
	g_return_val_if_fail (wrapper != NULL, FALSE);
	g_return_val_if_fail (wrapee != NULL, FALSE);

	if (wrapee->wrapped_by != NULL) {
		g_warning ("Attempt to wrap an output that is already wrapped.");
		return FALSE;
	}

	/* worry about someone dying with out unwrapping */
	g_object_weak_ref (G_OBJECT (wrapper), cb_output_wrap_screwup, wrapee);
	wrapee->wrapped_by = wrapper;
	return TRUE;
}

/**
 * gsf_output_unwrap :
 * @wrapper :
 * @wrapee :
 *
 * Returns TRUE if the wrapping succeeded.
 **/
gboolean
gsf_output_unwrap (GsfOutput *wrapper, GsfOutput *wrapee)
{
	g_return_val_if_fail (wrapee != NULL, FALSE);
	g_return_val_if_fail (wrapee->wrapped_by == wrapper, FALSE);

	wrapee->wrapped_by = NULL;
	g_object_weak_unref (G_OBJECT (wrapper), cb_output_wrap_screwup, wrapee);
	return TRUE;
}

GQuark 
gsf_output_error (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gsf_output_error");
	return quark;
}
