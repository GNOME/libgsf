/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input-gvfs.c:
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
#include <gsf-gvfs/gsf-input-gvfs.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-input-impl.h>
#include <gsf/gsf-impl-utils.h>
#include <string.h>

struct _GsfInputGvfs {
	GsfInput     input;
	GFile        *file;
	GInputStream *stream;
	guint8       *buf;
	size_t       buf_size;
};

typedef struct {
	GsfInputClass input_class;
} GsfInputGvfsClass;

static gboolean
can_seek (GInputStream *stream)
{
	if (!G_IS_SEEKABLE (stream))
		return FALSE;

	return g_seekable_can_seek (G_SEEKABLE (stream));
}

static GsfInput *
make_local_copy (GFile *file, GInputStream *stream)
{
	GsfOutput *out;
	GsfInput  *copy;
	GFileInfo *info;
	
	out = gsf_output_memory_new ();

	while (1) {
		guint8 buf[1024];
		gssize nread;
		
		nread = g_input_stream_read (stream, buf, sizeof(buf), NULL, NULL);
		
		if (nread > 0) {
			if (!gsf_output_write (out, nread, buf)) {
				copy = NULL;
				goto cleanup_and_exit;
			}
		}
		else if (nread == 0)
			break;
		else {
			copy = NULL;
			goto cleanup_and_exit;
		}
	}

	copy = gsf_input_memory_new_clone (gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (out)),
					   gsf_output_size (out));

	if (copy != NULL) {
		info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, NULL);
		if (info) {
			gsf_input_set_name (GSF_INPUT (copy), g_file_info_get_name (info));
			g_object_unref (G_OBJECT (info));
		}
	}

 cleanup_and_exit:

	gsf_output_close (out);
	g_object_unref (G_OBJECT (out));
	
	g_input_stream_close (stream, NULL, NULL);
	g_object_unref (G_OBJECT (stream));
	return copy;
}

GsfInput *
gsf_input_gvfs_new (GFile *file, GError **err)
{
	GsfInputGvfs *input;
	GInputStream *stream;
	GFileInfo    *info;

	if (file == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error_id (), 0,
					    "file is NULL");
		return NULL;
	}

	stream = (GInputStream *)g_file_read (file, NULL, err);
	if (stream == NULL)
		return NULL;

	if (!can_seek (stream))
		return make_local_copy (file, stream);

	input = g_object_new (GSF_INPUT_GVFS_TYPE, NULL);
	if (G_UNLIKELY (NULL == input)) {
		g_input_stream_close (stream, NULL, NULL);
		g_object_unref (G_OBJECT (stream));
		return NULL;
	}

	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE, 0, NULL, NULL);
	if (info) {
		gsf_input_set_size (GSF_INPUT (input), g_file_info_get_size (info));
		g_object_unref (G_OBJECT (info));
	}
	else
		return make_local_copy (file, stream);

	g_object_ref (G_OBJECT (file));

	input->stream = stream;
	input->file = file;
	input->buf  = NULL;
	input->buf_size = 0;

	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, NULL);
	if (info) {
		gsf_input_set_name (GSF_INPUT (input), g_file_info_get_name (info));
		g_object_unref (G_OBJECT (info));
	}

	return GSF_INPUT (input);
}

GsfInput *
gsf_input_gvfs_new_for_path (char const *path, GError **err)
{
	GFile *file;

	if (path == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error_id (), 0,
					    "path is NULL");
		return NULL;
	}

	file = g_file_new_for_path (path);
	if (file != NULL) {
		GsfInput *input;

		input = gsf_input_gvfs_new (file, err);
		g_object_unref (G_OBJECT (file));

		return input;
	}

	if (err != NULL)
		*err = g_error_new (gsf_input_error_id (), 0,
				    "couldn't open file");
	return NULL;
}

GsfInput *
gsf_input_gvfs_new_for_uri (char const *uri, GError **err)
{
	GFile *file;

	if (uri == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error_id (), 0,
					    "uri is NULL");
		return NULL;
	}

	file = g_file_new_for_uri (uri);
	if (file != NULL) {
		GsfInput *input;

		input = gsf_input_gvfs_new (file, err);
		g_object_unref (G_OBJECT (file));

		return input;
	}

	if (err != NULL)
		*err = g_error_new (gsf_input_error_id (), 0,
				    "couldn't open file");
	return NULL;
}

static void
gsf_input_gvfs_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInputGvfs *input = (GsfInputGvfs *)obj;

	g_input_stream_close (input->stream, NULL, NULL);
	g_object_unref (G_OBJECT (input->stream));
	input->stream = NULL;

	g_object_unref (G_OBJECT (input->file));
	input->file = NULL;

	if (input->buf != NULL) {
		g_free (input->buf);
		input->buf  = NULL;
		input->buf_size = 0;
	}

	parent_class = g_type_class_peek (GSF_INPUT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static GsfInput *
gsf_input_gvfs_dup (GsfInput *src_input, GError **err)
{
	GsfInputGvfs *src = (GsfInputGvfs *)src_input;
	GFile *clone;

	g_return_val_if_fail (src_input != NULL, NULL);
	g_return_val_if_fail (src->file != NULL, NULL);

	clone = g_file_dup (src->file);
	if (clone != NULL) {
		GsfInput *dst;

		dst = gsf_input_gvfs_new (clone, err);
	        g_object_unref (G_OBJECT (clone)); /* gsf_input_gvfs_new() adds a ref, or fails to create a new file. 
						      in any case, we need to unref the clone */
		return dst;
	}

	return NULL;
}

static guint8 const *
gsf_input_gvfs_read (GsfInput *input, size_t num_bytes,
		     guint8 *buffer)
{
	GsfInputGvfs *gvfs = GSF_INPUT_GVFS (input);
	size_t total_read = 0;

	g_return_val_if_fail (gvfs != NULL, NULL);
	g_return_val_if_fail (gvfs->stream != NULL, NULL);

	if (buffer == NULL) {
		if (gvfs->buf_size < num_bytes) {
			gvfs->buf_size = num_bytes;
			g_free (gvfs->buf);
			gvfs->buf = g_new (guint8, gvfs->buf_size);
		}
		buffer = gvfs->buf;
	}

	while (1) {
		gssize nread;
		
		nread = g_input_stream_read (gvfs->stream, (buffer + total_read), (num_bytes - total_read), NULL, NULL);
		
		if (nread >= 0) {
			total_read += nread;
			if ((size_t) total_read == num_bytes) {
				return buffer;
			}
		} else
			break;
	}

	return NULL;
}

static gboolean
gsf_input_gvfs_seek (GsfInput *input, gsf_off_t offset, GSeekType whence)
{
	GsfInputGvfs *gvfs = GSF_INPUT_GVFS (input);

	g_return_val_if_fail (gvfs != NULL, TRUE);
	g_return_val_if_fail (gvfs->stream != NULL, TRUE);
	g_return_val_if_fail (can_seek (gvfs->stream), TRUE);

	return (g_seekable_seek (G_SEEKABLE (gvfs->stream), offset, whence, NULL, NULL) ? FALSE : TRUE);
}

static void
gsf_input_gvfs_init (GObject *obj)
{
	GsfInputGvfs *gvfs = GSF_INPUT_GVFS (obj);

	gvfs->file = NULL;
	gvfs->stream = NULL;
	gvfs->buf  = NULL;
	gvfs->buf_size = 0;
}

static void
gsf_input_gvfs_class_init (GObjectClass *gobject_class)
{
	GsfInputClass *input_class = GSF_INPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_input_gvfs_finalize;
	input_class->Dup	= gsf_input_gvfs_dup;
	input_class->Read	= gsf_input_gvfs_read;
	input_class->Seek	= gsf_input_gvfs_seek;
}

GSF_CLASS (GsfInputGvfs, gsf_input_gvfs,
	   gsf_input_gvfs_class_init, gsf_input_gvfs_init, GSF_INPUT_TYPE)

/***************************************************************************/
/***************************************************************************/
