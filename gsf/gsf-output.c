/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output.c: interface for storing data
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
	g_clear_error (&output->err);
}

static void
gsf_output_init (GObject *obj)
{
	GsfOutput *output = GSF_OUTPUT (obj);

	output->cur_offset	= 0;
	output->cur_size	= 0;
	output->name		= NULL;
	output->wrapped_by	= NULL;
	output->container	= NULL;
	output->err		= NULL;
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
gsf_off_t
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
 * Returns FALSE on error
 **/
gboolean
gsf_output_close (GsfOutput *output)
{
	gboolean res;

	g_return_val_if_fail (GSF_IS_OUTPUT (output),
		gsf_output_set_error (output, 0, "<internal>"));
	g_return_val_if_fail (!output->is_closed,
		gsf_output_set_error (output, 0, "<internal>"));

	/* The implementation will log any errors, but we can never try to
	 * close multiple times even on failure.
	 */
	res = GET_CLASS (output)->Close (output);
	output->is_closed = TRUE;
	return res;
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
gsf_off_t
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
 * Returns FALSE on error.
 **/
gboolean
gsf_output_seek	(GsfOutput *output, gsf_off_t offset, GSeekType whence)
{
	gsf_off_t pos = offset;

	g_return_val_if_fail (output != NULL, FALSE);

	switch (whence) {
	case G_SEEK_SET: break;
	case G_SEEK_CUR: pos += output->cur_offset;	break;
	case G_SEEK_END: pos += output->cur_size;	break;
	default :
		g_warning ("Invalid seek type %d", (int)whence);
		return FALSE;
	}

	if (pos < 0) {
		g_warning ("Invalid seek position %" GSF_OFF_T_FORMAT
			   ", which is before the start of the file", pos);
		return FALSE;
	}

	/* If we go nowhere, just return.  This in particular handles null
	 * seeks for streams with no seek method.
	 */
	if (pos == output->cur_offset)
		return TRUE;

	if (GET_CLASS (output)->Seek (output, offset, whence)) {
		/* NOTE : it is possible for the current pos to be beyond the
		 * end of the file.  The intervening space is not filled with 0
		 * until something is written.
		 */
		output->cur_offset = pos;
		return TRUE;
	}

	/* the implementation should have assigned whatever errors are necessary */
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
		if (output->cur_size < output->cur_offset)
			output->cur_size = output->cur_offset;
		return TRUE;
	}

	/* the implementation should have assigned whatever errors are necessary */
	return FALSE;
}

/**
 * gsf_output_error :
 * @output :
 *
 * Returns the last error logged on the output, or NULL.
 **/
GError const *
gsf_output_error (GsfOutput const *output)
{
	g_return_val_if_fail (GSF_IS_OUTPUT (output), NULL);
	return output->err;
}

/**
 * gsf_output_set_name :
 * @output :
 * @name :
 *
 * <protected> This is a utility routine that should only be used by derived
 * outputs.
 *
 * Returns : TRUE if the assignment was ok.
 **/
gboolean
gsf_output_set_name (GsfOutput *output, char const *name)
{
	char *buf;

	g_return_val_if_fail (GSF_IS_OUTPUT (output), FALSE);

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
 * <protected> This is a utility routine that should only be used by derived
 * outputs.
 *
 * Returns : TRUE if the assignment was ok.
 */
gboolean
gsf_output_set_container (GsfOutput *output, GsfOutfile *container)
{
	g_return_val_if_fail (GSF_IS_OUTPUT (output), FALSE);

	if (container != NULL)
		g_object_ref (G_OBJECT (container));
	if (output->container != NULL)
		g_object_unref (G_OBJECT (output->container));
	output->container = container;
	return TRUE;
}

/**
 * gsf_output_set_error :
 * @output :
 * @container :
 *
 * <protected> This is a utility routine that should only be used by derived
 * outputs.
 *
 * Returns : Always returns FALSE to facilitate its use.
 */
gboolean
gsf_output_set_error (GsfOutput  *output,
		      gint        code,
		      char const *format,
		      ...)
{
	g_return_val_if_fail (GSF_IS_OUTPUT (output), FALSE);

	g_clear_error (&output->err);

	if (format != NULL) {
		va_list args;
		va_start (args, format);
		output->err = g_new (GError, 1);
		output->err->domain = gsf_output_error_id ();
		output->err->code = code;
		output->err->message = g_strdup_vprintf (format, args);
		va_end (args);
	}

	return FALSE;
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
gsf_output_error_id (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gsf_output_error");
	return quark;
}
