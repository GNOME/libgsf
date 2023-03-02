/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-gio.c:
 *
 * Copyright (C) 2007 Dom Lachowicz <cinamod@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf-output-gio.h>
#include <gsf/gsf.h>

#include <string.h>

struct _GsfOutputGio {
	GsfOutput output;
	GOutputStream *stream;
	gboolean can_seek;
};

typedef struct {
	GsfOutputClass output_class;
} GsfOutputGioClass;

static gboolean
can_seek (GOutputStream *stream)
{
	return (G_IS_SEEKABLE (stream) &&
		g_seekable_can_seek (G_SEEKABLE (stream)));
}

/**
 * gsf_output_gio_new_full:
 * @file: an existing GFile
 *
 * Returns: (transfer full): A new #GsfOutputGio or %NULL
 */
static GsfOutput *
gsf_output_gio_new_full (GFile *file, GError **err)
{
	GsfOutputGio *output;
	GOutputStream *stream;

	g_return_val_if_fail (file != NULL, NULL);

	stream = (GOutputStream *)g_file_replace (file, NULL, 0, FALSE, NULL, err);
	if (stream == NULL) {
		return NULL;
	}

	output = g_object_new (GSF_OUTPUT_GIO_TYPE, NULL);
	output->stream = stream;
	output->can_seek = can_seek (stream);

	return GSF_OUTPUT (output);
}

/**
 * gsf_output_gio_new:
 * @file: an existing GFile
 *
 * Returns: (transfer full): A new #GsfOutputGio or %NULL
 */
GsfOutput *
gsf_output_gio_new (GFile *file)
{
	return gsf_output_gio_new_full (file, NULL);
}

/**
 * gsf_output_gio_new_for_path:
 * @path:
 * @err: (allow-none): place to store a #GError if anything goes wrong
 *
 * Returns: (transfer full): A new #GsfOutputGio or %NULL
 */
GsfOutput *
gsf_output_gio_new_for_path (char const *path, GError **err)
{
	GFile *file;
	GsfOutput *output;

	g_return_val_if_fail (path != NULL, NULL);

	file = g_file_new_for_path (path);
	output = gsf_output_gio_new_full (file, err);
	g_object_unref (file);

	return output;
}

/**
 * gsf_output_gio_new_for_uri:
 * @uri:
 * @err: (allow-none): place to store a #GError if anything goes wrong
 *
 * Returns: (transfer full): A new #GsfOutputGio or %NULL
 */
GsfOutput *
gsf_output_gio_new_for_uri (char const *uri, GError **err)
{
	GFile *file;
	GsfOutput *output;

	g_return_val_if_fail (uri != NULL, NULL);

	file = g_file_new_for_uri (uri);
	output = gsf_output_gio_new_full (file, err);
	g_object_unref (file);

	return output;
}

static gboolean
gsf_output_gio_close (GsfOutput *output)
{
	GsfOutputGio *gio = GSF_OUTPUT_GIO (output);

	if (gio->stream != NULL) {
		g_output_stream_close (gio->stream, NULL, NULL);
		g_object_unref (gio->stream);
		gio->stream = NULL;

		return TRUE;
	}

	return FALSE;
}

static void
gsf_output_gio_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfOutputGio *output = (GsfOutputGio *)obj;

	gsf_output_gio_close (GSF_OUTPUT(output));

	parent_class = g_type_class_peek (GSF_OUTPUT_TYPE);
	parent_class->finalize (obj);
}

static gboolean
gsf_output_gio_write (GsfOutput *output,
		       size_t num_bytes,
		       guint8 const *buffer)
{
	GsfOutputGio *gio = GSF_OUTPUT_GIO (output);

	g_return_val_if_fail (gio != NULL, FALSE);
	g_return_val_if_fail (gio->stream != NULL, FALSE);

	while (num_bytes > 0) {
		gssize nwritten =
			g_output_stream_write (gio->stream,
					       buffer, num_bytes,
					       NULL, NULL);
		if (nwritten < 0)
			return FALSE;

		buffer += nwritten;
		num_bytes -= nwritten;
	}

	return TRUE;
}

static gboolean
gsf_output_gio_seek (GsfOutput *output, gsf_off_t offset, GSeekType whence)
{
	GsfOutputGio *gio = GSF_OUTPUT_GIO (output);

	g_return_val_if_fail (gio != NULL, FALSE);
	g_return_val_if_fail (gio->stream != NULL, FALSE);

	if (!gio->can_seek)
		return FALSE;

	return g_seekable_seek (G_SEEKABLE (gio->stream), offset, whence, NULL, NULL);
}

static void
gsf_output_gio_init (GObject *obj)
{
	GsfOutputGio *gio = GSF_OUTPUT_GIO (obj);

	gio->stream = NULL;
	gio->can_seek = FALSE;
}

static void
gsf_output_gio_class_init (GObjectClass *gobject_class)
{
	GsfOutputClass *output_class = GSF_OUTPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_output_gio_finalize;
	output_class->Close	= gsf_output_gio_close;
	output_class->Write	= gsf_output_gio_write;
	output_class->Seek	= gsf_output_gio_seek;
}

GSF_CLASS (GsfOutputGio, gsf_output_gio,
	   gsf_output_gio_class_init, gsf_output_gio_init, GSF_OUTPUT_TYPE)

/***************************************************************************/
/***************************************************************************/
