/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-io-context.h: IO context class
 *
 * Copyright (C) 2002-2004 Rodrigo Moya (rodrigo@gnome-db.org)
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

#ifndef GSF_IO_CONTEXT_H
#define GSF_IO_CONTEXT_H

#include <gsf/gsf-command-context.h>

G_BEGIN_DECLS

#define GSF_IO_CONTEXT_TYPE        (gsf_io_context_get_type ())
#define GSF_IO_CONTEXT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_IO_CONTEXT_TYPE, GsfIOContext))
#define GSF_IS_IO_CONTEXT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_IO_CONTEXT_TYPE))

typedef struct _GsfIOContext      GsfIOContext;
typedef struct _GsfIOContextClass GsfIOContextClass;

struct _GsfIOContextClass {
	GsfCommandContextClass parent_class;

	/* signals */
	void (* progress) (GsfIOContext *ioc, gdouble percent);
};

GType         gsf_io_context_get_type (void);
GsfIOContext *gsf_io_context_new (void);

void          gsf_io_context_clear (GsfIOContext *ioc);

void          gsf_io_context_update_progress (GsfIOContext *ioc, gdouble value);

G_END_DECLS

#endif
