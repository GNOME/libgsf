/*
 * gsf-io-context.h: IO context class
 *
 * Copyright (C) 2002 Rodrigo Moya (rodrigo@gnome-db.org)
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
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-io-context.h>

struct _GsfIOContext {
	GObject object;

	/* errors */
	gboolean error_occurred;
	GList *errors;

	/* operation progress */
	GList *progress_ranges;
	gdouble last_progress;
	gdouble last_time;
};

static void gsf_io_context_class_init (GsfIOContextClass *klass);
static void gsf_io_context_init       (GsfIOContext *ioc, GsfIOContextClass *klass);
static void gsf_io_context_finalize   (GObject *object);

enum {
	ERROR_OCCURRED,
	PROGRESS,
	LAST_SIGNAL
};

static guint io_context_signals[LAST_SIGNAL];
static GObjectClass *parent_class = NULL;

static void
gsf_io_context_class_init (GsfIOContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gsf_io_context_finalize;

	/* class signals */
	io_context_signals[ERROR_OCCURRED] =
		g_signal_new ("error_occurred",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GsfIOContextClass, error_occurred),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	io_context_signals[PROGRESS] =
		g_signal_new ("progress",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GsfIOContextClass, progress),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__DOUBLE,
			      G_TYPE_NONE, 1, G_TYPE_DOUBLE);
}

static void
gsf_io_context_init (GsfIOContext *ioc, GsfIOContextClass *klass)
{
	ioc->errors = NULL;
	ioc->error_occurred = FALSE;

	ioc->progress_ranges = NULL;
	ioc->last_progress = -1.0;
	ioc->last_time = 0.0;
}

static void
gsf_io_context_finalize (GObject *object)
{
	GsfIOContext *ioc = (GsfIOContext *) object;

	g_return_if_fail (GSF_IS_IO_CONTEXT (ioc));

	/* free memory */
	gsf_io_context_clear (ioc);

	parent_class->finalize (object);
}

GSF_CLASS(GsfIOContext, gsf_io_context,
	  gsf_io_context_class_init, gsf_io_context_init, G_TYPE_OBJECT)

/**
 * gsf_io_context_new
 *
 * Create a new #GsfIOContext, which is a class for status and
 * progress messages to be sent from one place to another one,
 * normally totally unknown and unrelated to each other.
 *
 * Returns: the newly created #GsfIOContext object.
 */
GsfIOContext *
gsf_io_context_new (void)
{
	GsfIOContext *ioc;

	ioc = g_object_new (GSF_IO_CONTEXT_TYPE, NULL);
	return ioc;
}

/**
 * gsf_io_context_error_occurred
 * @ioc: a #GsfIOContext object.
 *
 * Check whether there's been an error notification in the given
 * #GsfIOContext object or not.
 *
 * Returns: TRUE if there have been errors, FALSE if not.
 */
gboolean
gsf_io_context_error_occurred (GsfIOContext *ioc)
{
	g_return_val_if_fail (GSF_IS_IO_CONTEXT (ioc), FALSE);
	return ioc->error_occurred;
}

/**
 * gsf_io_context_error_message
 * @ioc: a #GsfIOContext object.
 * @msg: a message describing the error that has occured.
 * @code: code for the error (optional).
 *
 * Add a new error to the given #GsfIOContext object.
 */
void
gsf_io_context_error_message (GsfIOContext *ioc, const gchar *msg, gint code)
{
	GError *error;

	g_return_if_fail (GSF_IS_IO_CONTEXT (ioc));

	error = g_error_new_literal (g_quark_from_static_string ("GSF"), code, msg);
	gsf_io_context_push_error (ioc, error);
}

/**
 * gsf_io_context_push_error
 * @ioc: a #GsfIOContext object.
 * @error: an error to be added.
 *
 * Adds a new GError to the given #GsfIOContext. The error is added
 * to the end of the error queue, so that it can only be read,
 * via #gsf_io_context_pop_error, after all previous errors have
 * being read.
 */
void
gsf_io_context_push_error (GsfIOContext *ioc, const GError *error)
{
	GError *copy;

	g_return_if_fail (GSF_IS_IO_CONTEXT (ioc));
	g_return_if_fail (error != NULL);

	copy = g_error_copy (error);
	ioc->errors = g_list_append (ioc->errors, copy);
	ioc->error_occurred = TRUE;

	g_signal_emit (G_OBJECT (ioc), io_context_signals[ERROR_OCCURRED], 0);
}

/**
 * gsf_io_context_pop_error
 * @ioc: a #GsfIOContext object.
 *
 * Get the next error in the given #GsfIOContext object.
 *
 * Returns: a GError structure that contains all the information
 * for the next error. The caller of this function is responsible
 * of freeing the returned value, by calling #g_error_free.
 */
GError *
gsf_io_context_pop_error (GsfIOContext *ioc)
{
	GError *error = NULL;

	g_return_val_if_fail (GSF_IS_IO_CONTEXT (ioc), NULL);

	if (ioc->errors) {
		error = (GError *) ioc->errors->data;
		ioc->errors = g_list_remove (ioc->errors, error);
	}

	return error;
}

/**
 * gsf_io_context_clear
 * @ioc: a #GsfIOContext object.
 *
 * Clear the given a #GsfIOContext object, which means that the
 * error queue and any other information stored on it is cleared
 * up.
 */
void
gsf_io_context_clear (GsfIOContext *ioc)
{
	g_return_if_fail (GSF_IS_IO_CONTEXT (ioc));

	g_list_foreach (ioc->errors, (GFunc) g_error_free, NULL);
	g_list_free (ioc->errors);
	ioc->errors = NULL;
	
	ioc->error_occurred = FALSE;
	
	g_list_foreach (ioc->progress_ranges, (GFunc) g_free, NULL);
	g_list_free (ioc->progress_ranges);
	ioc->progress_ranges = NULL;

	ioc->last_progress = -1.0;
	ioc->last_time = 0.0;
}

/**
 * gsf_io_context_update_progress
 * @ioc: a #GsfIOContext object.
 * @value: new value for the progress status.
 *
 * Update the progress percentage for the given #GsfIOContext. This
 * will lead to the "progress_changed" signal to be emitted on the
 * IO context object.
 */
void
gsf_io_context_update_progress (GsfIOContext *ioc, gdouble value)
{
	gdouble *new_range;
	
	g_return_if_fail (GSF_IS_IO_CONTEXT (ioc));

	/* check value */
	if (value <= ioc->last_progress) {
		g_warning ("Invalid progress value");
		return;
	}

	/* update progress status */
	ioc->last_progress = value;

	new_range = g_new (gdouble, 1);
	*new_range = value;
	ioc->progress_ranges = g_list_append (ioc->progress_ranges, new_range);
	/* FIXME: update ioc->last_time */

	g_signal_emit (G_OBJECT (ioc), io_context_signals[PROGRESS], 0, ioc->last_progress);
}
