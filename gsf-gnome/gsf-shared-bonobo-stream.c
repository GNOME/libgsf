/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-shared-bonobo-stream.h: helper class for gsf-input-bonobo
 *
 * Copyright (C) 2002-2003 Jon K Hellan (hellan@acm.org)
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

#include <gsf-config.h>
#include <gsf-gnome/gsf-shared-bonobo-stream.h>
#include <gsf/gsf-impl-utils.h>

typedef struct {
	GObjectClass g_object_class;
} GsfSharedBonoboStreamClass;

GsfSharedBonoboStream *
gsf_shared_bonobo_stream_new (Bonobo_Stream stream)
{
	GsfSharedBonoboStream *bst =
		g_object_new (GSF_SHARED_BONOBO_STREAM_TYPE, NULL);
	bst->stream = stream;
	return bst;
}

static void
gsf_shared_bonobo_stream_finalize (GObject *obj)
{
	GsfSharedBonoboStream *bst = (GsfSharedBonoboStream *) (obj);

	if (bst->stream != NULL) {
	}
}

static void
gsf_shared_bonobo_stream_init (GObject *obj)
{
	GsfSharedBonoboStream *bst = (GsfSharedBonoboStream *) (obj);
	bst->stream = NULL;
	bst->pos    = 0;
}

static void
gsf_shared_bonobo_stream_class_init (GObjectClass *gobject_class)
{
	gobject_class->finalize = gsf_shared_bonobo_stream_finalize;
}

GSF_CLASS (GsfSharedBonoboStream, gsf_shared_bonobo_stream,
	   gsf_shared_bonobo_stream_class_init,
	   gsf_shared_bonobo_stream_init,
	   G_TYPE_OBJECT)
