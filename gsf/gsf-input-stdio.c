/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input-stdio.c: stdio based input
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
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-input-impl.h>
#include <gsf/gsf-impl-utils.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

struct _GsfInputStdio {
	GsfInput input;

	FILE     *file;
	guint8   *buf;
	unsigned  buf_size;
};

typedef struct {
	GsfInputClass input_class;
} GsfInputStdioClass;

/**
 * gsf_input_stdio_new :
 * @filename : in utf8.
 * @err	     : optionally NULL.
 *
 * Returns a new file or NULL.
 **/
GsfInput *
gsf_input_stdio_new (char const *filename, GError **err)
{
	GsfInputStdio *input;
	struct stat st;
	FILE *file;
	
	file = fopen (filename, "r");
	if (file == NULL || fstat (fileno (file), &st) < 0) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"%s: %s", filename, g_strerror (errno));
		return NULL;
	}

	if (!S_ISREG (st.st_mode)) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"%s: Is not a regular file", filename);
		return NULL;
	}

	input = g_object_new (GSF_INPUT_STDIO_TYPE, NULL);
	input->file = file;
	input->buf  = NULL;
	input->buf_size = 0;
	gsf_input_set_size (GSF_INPUT (input), st.st_size);

	return GSF_INPUT (input);
}

static void
gsf_input_stdio_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInputStdio *input = (GsfInputStdio *)obj;

	if (input->file != NULL) {
		fclose (input->file);
		input->file = NULL;
	}
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
gsf_input_stdio_dup (GsfInput *src_input)
{
	GsfInputStdio const *src = (GsfInputStdio *)src_input;
	GsfInputStdio *dst = g_object_new (GSF_INPUT_STDIO_TYPE, NULL);

	if (src->file != NULL)
		dst->file = fdopen (fileno (src->file), "r");

	return GSF_INPUT (dst);
}

static gboolean
gsf_input_stdio_eof (GsfInput *input)
{
	GsfInputStdio *stdio = GSF_INPUT_STDIO (input);
	return stdio->file == NULL || feof (stdio->file);
}

static guint8 const *
gsf_input_stdio_read (GsfInput *input, unsigned num_bytes)
{
	GsfInputStdio *stdio = GSF_INPUT_STDIO (input);
	size_t res;

	g_return_val_if_fail (stdio != NULL, NULL);
	g_return_val_if_fail (stdio->file != NULL, NULL);

	if (stdio->buf_size < num_bytes) {
		stdio->buf_size = num_bytes;
		if (stdio->buf != NULL)
			g_free (stdio->buf);
		stdio->buf = g_malloc (stdio->buf_size);
	}

	res = fread (stdio->buf, 1, num_bytes, stdio->file);
	if (res < num_bytes)
		return NULL;

	return stdio->buf;
}

static gboolean
gsf_input_stdio_seek (GsfInput *input, int offset, GsfOff_t whence)
{
	GsfInputStdio const *stdio = GSF_INPUT_STDIO (input);

	if (stdio->file == NULL)
		return TRUE;

	switch (whence) {
	case GSF_SEEK_SET :
		if (0 == fseek (stdio->file, offset, SEEK_SET))
			return FALSE;
		break;
	case GSF_SEEK_CUR :
		if (0 == fseek (stdio->file, offset, SEEK_CUR))
			return FALSE;
		break;
	case GSF_SEEK_END :
		if (0 == fseek (stdio->file, offset, SEEK_END))
			return FALSE;
	}

	return TRUE;
}

static void
gsf_input_stdio_init (GObject *obj)
{
	GsfInputStdio *stdio = GSF_INPUT_STDIO (obj);

	stdio->file = NULL;
	stdio->buf  = NULL;
	stdio->buf_size = 0;
}

static void
gsf_input_stdio_class_init (GObjectClass *gobject_class)
{
	GsfInputClass *input_class = GSF_INPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_input_stdio_finalize;
	input_class->dup	= gsf_input_stdio_dup;
	input_class->eof	= gsf_input_stdio_eof;
	input_class->read	= gsf_input_stdio_read;
	input_class->seek	= gsf_input_stdio_seek;
}

GSF_CLASS (GsfInputStdio, gsf_input_stdio,
	   gsf_input_stdio_class_init, gsf_input_stdio_init, GSF_INPUT_TYPE)
