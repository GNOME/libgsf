/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input-stdio.c: stdio based input
 *
 * Copyright (C) 2002-2004 Jody Goldberg (jody@gnome.org)
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
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-input-impl.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-utils.h>

#include <stdio.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#define S_ISREG(x) ((x) & _S_IFREG)
#endif

struct _GsfInputStdio {
	GsfInput input;

	FILE     *file;
	guint8   *buf;
	size_t   buf_size;
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
GsfInputStdio *
gsf_input_stdio_new (char const *filename, GError **err)
{
	GsfInputStdio *input;
	struct stat st;
	FILE *file;
	gsf_off_t size;

	file = fopen (filename, "rb");
	if (file == NULL || fstat (fileno (file), &st) < 0) {
		if (err != NULL) {
			char *utf8name = gsf_filename_to_utf8 (filename, FALSE);
			*err = g_error_new (gsf_input_error (), 0,
				"%s: %s", utf8name, g_strerror (errno));
			g_free (utf8name);
		}
		if (file) fclose (file); /* Just in case.  */
		return NULL;
	}

	if (!S_ISREG (st.st_mode)) {
		if (err != NULL) {
			char *utf8name = gsf_filename_to_utf8 (filename, FALSE);
			*err = g_error_new (gsf_input_error (), 0,
				"%s: Is not a regular file", utf8name);
			g_free (utf8name);
		}
		fclose (file);
		return NULL;
	}

	size = st.st_size;
	input = g_object_new (GSF_INPUT_STDIO_TYPE, NULL);
	input->file = file;
	input->buf  = NULL;
	input->buf_size = 0;
	gsf_input_set_size (GSF_INPUT (input), size);
	gsf_input_set_name (GSF_INPUT (input), filename);

	return input;
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
gsf_input_stdio_dup (GsfInput *src_input, GError **err)
{
	GsfInputStdio const *src = (GsfInputStdio *)src_input;
	return GSF_INPUT (gsf_input_stdio_new (src->input.name, err));
}

static guint8 const *
gsf_input_stdio_read (GsfInput *input, size_t num_bytes,
		      guint8 *buffer)
{
	GsfInputStdio *stdio = GSF_INPUT_STDIO (input);
	size_t nread = 0, total_read = 0;

	g_return_val_if_fail (stdio != NULL, NULL);
	g_return_val_if_fail (stdio->file != NULL, NULL);

	if (buffer == NULL) {
		if (stdio->buf_size < num_bytes) {
			stdio->buf_size = num_bytes;
			if (stdio->buf != NULL)
				g_free (stdio->buf);
			stdio->buf = g_new (guint8, stdio->buf_size);
		}
		buffer = stdio->buf;
	}

	while (total_read < num_bytes) {
		nread = fread (buffer + total_read, 1, 
			       num_bytes - total_read, stdio->file);
		total_read += nread;
		if (total_read < num_bytes &&
		    (ferror (stdio->file) || feof (stdio->file)))
			return NULL;
	}

	return buffer;
}

static gboolean
gsf_input_stdio_seek (GsfInput *input, gsf_off_t offset, GSeekType whence)
{
	GsfInputStdio const *stdio = GSF_INPUT_STDIO (input);
	gsf_off_t loffset;
	int stdio_whence = SEEK_SET;

	if (stdio->file == NULL)
		return TRUE;

	loffset = offset;
	if ((gsf_off_t) loffset != offset) { /* Check for overflow */
		g_warning ("offset too large for fseek");
		return TRUE;
	}
	switch (whence) {
	case G_SEEK_CUR : stdio_whence = SEEK_CUR; break;
	case G_SEEK_END : stdio_whence = SEEK_END; break;
	case G_SEEK_SET:
	default:
		break;
	}

	if (0 == fseek (stdio->file, (unsigned long)loffset, stdio_whence))
		return FALSE;
	
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
	input_class->Dup	= gsf_input_stdio_dup;
	input_class->Read	= gsf_input_stdio_read;
	input_class->Seek	= gsf_input_stdio_seek;
}

GSF_CLASS (GsfInputStdio, gsf_input_stdio,
	   gsf_input_stdio_class_init, gsf_input_stdio_init, GSF_INPUT_TYPE)
