/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-outfile-msole.c: 
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
 * Foundation, Outc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf-outfile-impl.h>
#include <gsf/gsf-outfile-msole.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-utils.h>

#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "libgsf:msole"

struct _GsfOutfileMSOle {
	GsfOutfile parent;

	GsfOutput    *sink;
};

typedef struct {
	GsfOutfileClass  parent_class;
} GsfOutfileMSOleClass;

#define GSF_OUTFILE_MSOLE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GSF_OUTFILE_MSOLE_TYPE, GsfOutfileMSOleClass))
#define GSF_IS_OUTFILE_MSOLE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GSF_OUTFILE_MSOLE_TYPE))

static void
gsf_outfile_msole_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfOutfileMSOle *ole = GSF_OUTFILE_MSOLE (obj);

	if (ole->sink != NULL) {
		g_object_unref (G_OBJECT (ole->sink));
		ole->sink = NULL;
	}

	parent_class = g_type_class_peek (GSF_OUTFILE_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static gboolean
gsf_outfile_msole_write (GsfOutput *output,
			 size_t num_bytes, guint8 const *data)
{
}

static gboolean
gsf_outfile_msole_seek (GsfOutput *output, off_t offset, GsfOff_t whence)
{
}

static gboolean
gsf_outfile_msole_close (GsfOutput *output)
{
}

static GsfOutput *
gsf_outfile_msole_new_child (GsfOutfile *outfile, char const *name,
			     gboolean is_dir)
{
}

static void
gsf_outfile_msole_init (GObject *obj)
{
	GsfOutfileMSOle *ole = GSF_OUTFILE_MSOLE (obj);

	ole->sink		= NULL;
}

static void
gsf_outfile_msole_class_init (GObjectClass *gobject_class)
{
	GsfOutputClass  *input_class  = GSF_OUTPUT_CLASS (gobject_class);
	GsfOutfileClass *outfile_class = GSF_OUTFILE_CLASS (gobject_class);

	gobject_class->finalize		= gsf_outfile_msole_finalize;
	input_class->Write		= gsf_outfile_msole_write;
	input_class->Seek		= gsf_outfile_msole_seek;
	input_class->Close		= gsf_outfile_msole_close;
	outfile_class->new_child	= gsf_outfile_msole_new_child;
}

GSF_CLASS (GsfOutfileMSOle, gsf_outfile_msole,
	   gsf_outfile_msole_class_init, gsf_outfile_msole_init,
	   GSF_OUTFILE_TYPE)

/**
 * gsf_outfile_msole_new :
 * @sink :
 * @err   :
 *
 * Creates the root directory of an MS OLE file and manages the addition of
 * children.
 *
 * NOTE : adds a reference to @sink
 *
 * Returns : the new ole file handler
 **/
GsfOutfile *
gsf_outfile_msole_new (GsfOutput *sink, GError **err)
{
	GsfOutfileMSOle *ole;

	g_return_val_if_fail (GSF_IS_OUTPUT (sink), NULL);

	ole = g_object_new (GSF_OUTFILE_MSOLE_TYPE, NULL);
	g_object_ref (G_OBJECT (sink));
	ole->sink = sink;

	return GSF_OUTFILE (ole);
}
