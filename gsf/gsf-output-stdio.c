/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-stdio.c: stdio based output
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
#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-output-impl.h>
#include <gsf/gsf-impl-utils.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

struct _GsfOutputStdio {
	GsfOutput output;

	FILE     *file;
};

typedef struct {
	GsfOutputClass output_class;
} GsfOutputStdioClass;

/**
 * gsf_output_stdio_new :
 * @filename : in utf8.
 * @err	     : optionally NULL.
 *
 * Returns a new file or NULL.
 **/
GsfOutput *
gsf_output_stdio_new (char const *filename, GError **err)
{
	GsfOutputStdio *output;
	FILE *file = NULL;

	if (filename == NULL) {
		char *name;
		int fd = g_file_open_tmp ("gsf-output", &name, err);
		if (fd < 0)
			return NULL;
		filename = name;
		file = fdopen (fd, "w");
	} else
		file = fopen (filename, "w");

	if (file == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_output_error (), 0,
				"%s: %s", filename, g_strerror (errno));
		return NULL;
	}

	output = g_object_new (GSF_OUTPUT_STDIO_TYPE, NULL);
	output->file = file;

	return GSF_OUTPUT (output);
}

static gboolean
gsf_output_stdio_close (GsfOutput *output)
{
	GsfOutputStdio *stdio = GSF_OUTPUT_STDIO (output);
	gboolean res = FALSE;

	if (stdio->file != NULL) {
		res = (0 == fclose (stdio->file));
		stdio->file = NULL;
	}

	return res;
}

static void
gsf_output_stdio_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfOutput *output = (GsfOutput *)obj;

	if (!gsf_output_is_closed (output))
		gsf_output_close (output);

	parent_class = g_type_class_peek (GSF_OUTPUT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}


static gboolean
gsf_output_stdio_seek (GsfOutput *output, gsf_off_t offset, GSeekType whence)
{
	GsfOutputStdio const *stdio = GSF_OUTPUT_STDIO (output);
	long loffset;

	if (stdio->file == NULL)
		return TRUE;

	loffset = offset;
	if ((gsf_off_t) loffset != offset) { /* Check for overflow */
		g_warning ("offset too large for fseek");
		return TRUE;
	}
	switch (whence) {
	case G_SEEK_SET :
		if (0 == fseek (stdio->file, loffset, SEEK_SET))
			return FALSE;
		break;
	case G_SEEK_CUR :
		if (0 == fseek (stdio->file, loffset, SEEK_CUR))
			return FALSE;
		break;
	case G_SEEK_END :
		if (0 == fseek (stdio->file, loffset, SEEK_END))
			return FALSE;
	}

	return TRUE;
}

static gboolean
gsf_output_stdio_write (GsfOutput *output,
			size_t num_bytes,
			guint8 const *buffer)
{
	GsfOutputStdio *stdio = GSF_OUTPUT_STDIO (output);
	size_t res;

	g_return_val_if_fail (stdio != NULL, FALSE);
	g_return_val_if_fail (stdio->file != NULL, FALSE);

	res = fwrite (buffer, 1, num_bytes, stdio->file);
	return res == num_bytes;
}

static void
gsf_output_stdio_init (GObject *obj)
{
	GsfOutputStdio *stdio = GSF_OUTPUT_STDIO (obj);

	stdio->file = NULL;
}

static void
gsf_output_stdio_class_init (GObjectClass *gobject_class)
{
	GsfOutputClass *output_class = GSF_OUTPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_output_stdio_finalize;
	output_class->Close	= gsf_output_stdio_close;
	output_class->Seek	= gsf_output_stdio_seek;
	output_class->Write	= gsf_output_stdio_write;
}

GSF_CLASS (GsfOutputStdio, gsf_output_stdio,
	   gsf_output_stdio_class_init, gsf_output_stdio_init, GSF_OUTPUT_TYPE)
