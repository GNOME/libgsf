/*
 * gsf-command-context.h: Command context class
 *
 * Copyright (C) 2002-2003 Rodrigo Moya (rodrigo@gnome-db.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef GSF_COMMAND_CONTEXT_H
#define GSF_COMMAND_CONTEXT_H

#include <gsf/gsf.h>

G_BEGIN_DECLS

#define GSF_COMMAND_CONTEXT_TYPE        (gsf_command_context_get_type ())
#define GSF_COMMAND_CONTEXT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_COMMAND_CONTEXT_TYPE, GsfCommandContext))
#define GSF_IS_COMMAND_CONTEXT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_COMMAND_CONTEXT_TYPE))

typedef struct _GsfCommandContext      GsfCommandContext;
typedef struct _GsfCommandContextClass GsfCommandContextClass;

struct _GsfCommandContext {
	GObject object;

	/* errors */
	gboolean error_occurred;
	GList *errors;

	gboolean has_warnings;
	GList *warnings;
};

struct _GsfCommandContextClass {
	GObjectClass parent_class;

	/* signals */
	void (* error_occurred) (GsfCommandContext *cc);
	void (* warning) (GsfCommandContext *cc);
};

GType              gsf_command_context_get_type (void);
GsfCommandContext *gsf_command_context_new (void);

gboolean           gsf_command_context_error_occurred (GsfCommandContext *cc);
void               gsf_command_context_error_message (GsfCommandContext *cc, const gchar *msg, gint code);
void               gsf_command_context_push_error (GsfCommandContext *cc, const GError *error);
GError            *gsf_command_context_pop_error (GsfCommandContext *cc);

gboolean           gsf_command_context_has_warnings (GsfCommandContext *cc);
void               gsf_command_context_push_warning (GsfCommandContext *cc, const GError *warning);
GError            *gsf_command_context_pop_warning (GsfCommandContext *cc);

void               gsf_command_context_clear (GsfCommandContext *cc);

G_END_DECLS

#endif
