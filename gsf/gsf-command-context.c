/*
 * gsf-command-context.c: Command context class
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
#include <gsf/gsf-command-context.h>

static void gsf_command_context_class_init (GsfCommandContextClass *klass);
static void gsf_command_context_init       (GsfCommandContext *cc, GsfCommandContextClass *klass);
static void gsf_command_context_finalize   (GObject *object);

enum {
	ERROR_OCCURRED,
	LAST_SIGNAL
};

static guint cmd_context_signals[LAST_SIGNAL];
static GObjectClass *parent_class = NULL;

static void
gsf_command_context_class_init (GsfCommandContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gsf_command_context_finalize;

	/* class signals */
	cmd_context_signals[ERROR_OCCURRED] =
		g_signal_new ("error_occurred",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GsfCommandContextClass, error_occurred),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
gsf_command_context_init (GsfCommandContext *cc, GsfCommandContextClass *klass)
{
	cc->errors = NULL;
	cc->error_occurred = FALSE;
}

static void
gsf_command_context_finalize (GObject *object)
{
	GsfCommandContext *cc = (GsfCommandContext *) object;

	g_return_if_fail (GSF_IS_COMMAND_CONTEXT (cc));

	/* free memory */
	gsf_command_context_clear (cc);

	parent_class->finalize (object);
}

GSF_CLASS(GsfCommandContext, gsf_command_context,
	  gsf_command_context_class_init, gsf_command_context_init, G_TYPE_OBJECT)

/**
 * gsf_command_context_new
 *
 * Create a new #GsfCommandContext, which is a class that allows
 * sending and retrieving status information such as errors
 * from/to command contexts.
 *
 * Returns: the newly created #GsfCommandContext object.
 */
GsfCommandContext *
gsf_command_context_new (void)
{
	GsfCommandContext *cc;

	cc = g_object_new (GSF_COMMAND_CONTEXT_TYPE, NULL);
	return cc;
}

/**
 * gsf_command_context_error_occurred
 * @ioc: a #GsfCommandContext object.
 *
 * Check whether there's been an error notification in the given
 * #GsfCommandContext object or not.
 *
 * Returns: TRUE if there have been errors, FALSE if not.
 */
gboolean
gsf_command_context_error_occurred (GsfCommandContext *cc)
{
	g_return_val_if_fail (GSF_IS_COMMAND_CONTEXT (cc), FALSE);
	return cc->error_occurred;
}

/**
 * gsf_command_context_error_message
 * @cc: a #GsfCommandContext object.
 * @msg: a message describing the error that has occured.
 * @code: code for the error (optional).
 *
 * Add a new error to the given #GsfCommandContext object.
 */
void
gsf_command_context_error_message (GsfCommandContext *cc, const gchar *msg, gint code)
{
	GError *error;

	g_return_if_fail (GSF_IS_COMMAND_CONTEXT (cc));

	error = g_error_new_literal (g_quark_from_static_string ("GSF"), code, msg);
	gsf_command_context_push_error (cc, error);
}

/**
 * gsf_command_context_push_error
 * @cc: a #GsfCommandContext object.
 * @error: an error to be added.
 *
 * Adds a new GError to the given #GsfCommandContext. The error is added
 * to the end of the error queue, so that it can only be read,
 * via #gsf_command_context_pop_error, after all previous errors have
 * being read.
 */
void
gsf_command_context_push_error (GsfCommandContext *cc, const GError *error)
{
	GError *copy;

	g_return_if_fail (GSF_IS_COMMAND_CONTEXT (cc));
	g_return_if_fail (error != NULL);

	copy = g_error_copy (error);
	cc->errors = g_list_append (cc->errors, copy);
	cc->error_occurred = TRUE;

	g_signal_emit (G_OBJECT (cc), cmd_context_signals[ERROR_OCCURRED], 0);
}

/**
 * gsf_command_context_pop_error
 * @cc: a #GsfCommandContext object.
 *
 * Get the next error in the given #GsfCommandContext object.
 *
 * Returns: a GError structure that contains all the information
 * for the next error. The caller of this function is responsible
 * of freeing the returned value, by calling #g_error_free.
 */
GError *
gsf_command_context_pop_error (GsfCommandContext *cc)
{
	GError *error = NULL;

	g_return_val_if_fail (GSF_IS_COMMAND_CONTEXT (cc), NULL);

	if (cc->errors) {
		error = (GError *) cc->errors->data;
		cc->errors = g_list_remove (cc->errors, error);
	}

	return error;
}

/**
 * gsf_command_context_clear
 * @cc: a #GsfCommandContext object.
 *
 * Clear the given #GsfCommandContext object, which means that the
 * error and warnings queues and any other information that has been
 * associated to the command context will be cleared up.
 */
void
gsf_command_context_clear (GsfCommandContext *cc)
{
	g_return_if_fail (GSF_IS_COMMAND_CONTEXT (cc));

	g_list_foreach (cc->errors, (GFunc) g_error_free, NULL);
	g_list_free (cc->errors);
	cc->errors = NULL;
	
	cc->error_occurred = FALSE;
}
