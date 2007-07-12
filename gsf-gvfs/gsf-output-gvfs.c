/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-gvfs.c:
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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <gsf-config.h>
#include <gsf-gvfs/gsf-output-gvfs.h>
#include <gio/goutputstream.h>
#include <gio/gseekable.h>
#include <gsf/gsf-output-impl.h>
#include <gsf/gsf-impl-utils.h>
#include <string.h>

struct _GsfOutputGvfs {
	GsfOutput output;
	GFile *file;
	GOutputStream *stream;
};

typedef struct {
	GsfOutputClass output_class;
} GsfOutputGvfsClass;

GsfOutput *
gsf_output_gvfs_new (GFile *file)
{
	GsfOutputGvfs *output;

	g_return_val_if_fail (file != NULL, NULL);

	output = g_object_new (GSF_OUTPUT_GVFS_TYPE, NULL);
	output->file = file;
	output->stream = (GOutputStream *)g_file_create (file, NULL, NULL);
	g_object_ref (output->file);

	return GSF_OUTPUT(output);
}

GsfOutput *
gsf_output_gvfs_new_for_path (char const *path, GError **err)
{
	GFile *file;

	if (path == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_output_error_id (), 0,
					    "path is NULL");
		return NULL;
	}

	file = g_file_get_for_path (path);
	if (file != NULL) {
		GsfOutput *output;

		output = gsf_output_gvfs_new (file);
		g_object_unref (G_OBJECT (file));

		return output;
	}

	if (err != NULL)
		*err = g_error_new (gsf_output_error_id (), 0,
				    "couldn't open file");
	return NULL;
}

GsfOutput *
gsf_output_gvfs_new_for_uri (char const *uri, GError **err)
{
	GFile *file;

	if (uri == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_output_error_id (), 0,
					    "uri is NULL");
		return NULL;
	}

	file = g_file_get_for_uri (uri);
	if (file != NULL) {
		GsfOutput *output;

		output = gsf_output_gvfs_new (file);
		g_object_unref (G_OBJECT (file));

		return output;
	}

	if (err != NULL)
		*err = g_error_new (gsf_output_error_id (), 0,
				    "couldn't open file");
	return NULL;
}

static gboolean
gsf_output_gvfs_close (GsfOutput *output)
{
	GsfOutputGvfs *gvfs = GSF_OUTPUT_GVFS (output);

	if (gvfs->stream != NULL) {
		g_output_stream_close (gvfs->stream, NULL, NULL);
		g_object_unref (G_OBJECT (gvfs->stream));
		gvfs->stream = NULL;
		
		g_object_unref (G_OBJECT (gvfs->file));
		gvfs->file = NULL;

		return TRUE;
	}

	return FALSE;
}

static void
gsf_output_gvfs_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfOutputGvfs *output = (GsfOutputGvfs *)obj;

	gsf_output_gvfs_close (GSF_OUTPUT(output));

	parent_class = g_type_class_peek (GSF_OUTPUT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static gboolean
gsf_output_gvfs_write (GsfOutput *output,
		       size_t num_bytes,
		       guint8 const *buffer)
{
	GsfOutputGvfs *gvfs = GSF_OUTPUT_GVFS (output);
	size_t total_written = 0;

	g_return_val_if_fail (gvfs != NULL, FALSE);
	g_return_val_if_fail (gvfs->stream != NULL, FALSE);

	while (1) {
		gssize nwritten;

		nwritten = g_output_stream_write (gvfs->stream, (guint8 *)(buffer + total_written), (num_bytes - total_written), NULL, NULL);

		if (nwritten >= 0) {
			total_written += nwritten;
			if ((size_t)total_written == num_bytes)
				return TRUE;
		} else {
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
gsf_output_gvfs_seek (GsfOutput *output, gsf_off_t offset, GSeekType whence)
{
	GsfOutputGvfs *gvfs = GSF_OUTPUT_GVFS (output);

	g_return_val_if_fail (gvfs != NULL, FALSE);
	g_return_val_if_fail (gvfs->stream != NULL, FALSE);

	if (!G_IS_SEEKABLE (gvfs->stream))
		return FALSE;

	if (!g_seekable_can_seek (G_SEEKABLE (gvfs->stream)))
		return FALSE;

	return g_seekable_seek (G_SEEKABLE (gvfs->stream), offset, whence, NULL, NULL);
}

static void
gsf_output_gvfs_init (GObject *obj)
{
	GsfOutputGvfs *gvfs = GSF_OUTPUT_GVFS (obj);

	gvfs->file   = NULL;
	gvfs->stream = NULL;
}

static void
gsf_output_gvfs_class_init (GObjectClass *gobject_class)
{
	GsfOutputClass *output_class = GSF_OUTPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_output_gvfs_finalize;
	output_class->Close	= gsf_output_gvfs_close;
	output_class->Write	= gsf_output_gvfs_write;
	output_class->Seek	= gsf_output_gvfs_seek;
}

GSF_CLASS (GsfOutputGvfs, gsf_output_gvfs,
	   gsf_output_gvfs_class_init, gsf_output_gvfs_init, GSF_OUTPUT_TYPE)

/***************************************************************************/
/***************************************************************************/
