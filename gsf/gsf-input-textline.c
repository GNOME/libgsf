/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input-textline.c: textline based input
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
#include <gsf/gsf-input-textline.h>
#include <gsf/gsf-input-impl.h>
#include <gsf/gsf-impl-utils.h>

struct _GsfInputTextline {
	GsfInput input;

	GsfInput	*source;
	guint8 const	*remainder;
	unsigned	 remainder_size;
	unsigned	 max_line_size;
	guint8		*buf;
	unsigned	 buf_size;
	int		 current_line;
};

typedef struct {
	GsfInputClass input_class;
} GsfInputTextlineClass;

/**
 * gsf_input_textline_new :
 * @source : in some combination of ascii and utf8
 *
 * NOTE : adds a reference to @source
 *
 * Returns a new file or NULL.
 **/
GsfInputTextline *
gsf_input_textline_new (GsfInput *source)
{
	GsfInputTextline *input;

	g_return_val_if_fail (source != NULL, NULL);

	input = g_object_new (GSF_INPUT_TEXTLINE_TYPE, NULL);
	g_object_ref (G_OBJECT (source));
	input->source = source;
	input->buf  = NULL;
	input->buf_size = 0;
	gsf_input_set_size (GSF_INPUT (source), (unsigned)gsf_input_size (source));

	return input;
}

static void
gsf_input_textline_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInputTextline *input = (GsfInputTextline *)obj;

	if (input->source != NULL) {
		g_object_unref (G_OBJECT (input->source));
		input->source = NULL;
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
gsf_input_textline_dup (GsfInput *src_input, GError **err)
{
	GsfInputTextline const *src = (GsfInputTextline *)src_input;
	GsfInputTextline *dst = g_object_new (GSF_INPUT_TEXTLINE_TYPE, NULL);

	(void)err;
	dst->source = src->source;
	g_object_ref (G_OBJECT (dst->source));

	return GSF_INPUT (dst);
}

static guint8 const *
gsf_input_textline_read (GsfInput *input, size_t num_bytes, guint8 *buffer)
{
	GsfInputTextline *textline = GSF_INPUT_TEXTLINE (input);
	textline->remainder = NULL;
	return gsf_input_read (textline->source, num_bytes, buffer);
}

static gboolean
gsf_input_textline_seek (GsfInput *input, off_t offset, GsfOff_t whence)
{
	GsfInputTextline *textline = GSF_INPUT_TEXTLINE (input);
	textline->remainder = NULL;
	return gsf_input_seek (textline->source, offset, whence);
}

static void
gsf_input_textline_init (GObject *obj)
{
	GsfInputTextline *textline = GSF_INPUT_TEXTLINE (obj);

	textline->source	 = NULL;
	textline->remainder	 = NULL;
	textline->remainder_size = 0;
	textline->max_line_size  = 512;	/* an initial guess */
	textline->buf		 = NULL;
	textline->buf_size	 = 0;
}

static void
gsf_input_textline_class_init (GObjectClass *gobject_class)
{
	GsfInputClass *input_class = GSF_INPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_input_textline_finalize;
	input_class->Dup	= gsf_input_textline_dup;
	input_class->Read	= gsf_input_textline_read;
	input_class->Seek	= gsf_input_textline_seek;
}

GSF_CLASS (GsfInputTextline, gsf_input_textline,
	   gsf_input_textline_class_init, gsf_input_textline_init, GSF_INPUT_TYPE)

/**
 * gsf_input_textline_ascii_gets :
 * @input :
 *
 * A utility routine to read things line by line from the underlying source.
 * Trailing newlines and carriage returns are stipped, and the resultant buffer
 * can be edited.
 *
 * returns the string read, or NULL on eof.
 **/
char *
gsf_input_textline_ascii_gets (GsfInputTextline *textline)
{
	unsigned n, i;
	guint8 const *ptr;

	g_return_val_if_fail (textline != NULL, NULL);

	if (textline->remainder == NULL ||
	    textline->remainder_size == 0) {
		n = gsf_input_remaining (textline->source);
		if (n > textline->max_line_size)
			n = textline->max_line_size;

		textline->remainder = gsf_input_read (textline->source, n, NULL);
		if (textline->remainder == NULL)
			return NULL;
	}

	for (ptr = textline->remainder; i < n; ptr++)
		if (*ptr == '\n' || *ptr == '\r')
			break;
	textline->remainder = ptr;
	textline->remainder_size = n - i;

	return (char *)textline->buf;
}

guint8 *
gsf_input_textline_utf8_gets (GsfInputTextline *textline)
{
	unsigned n, i;
	guint8 const *ptr;

	g_return_val_if_fail (textline != NULL, NULL);

	if (textline->remainder == NULL ||
	    textline->remainder_size == 0) {
		n = gsf_input_remaining (textline->source);
		if (n > textline->max_line_size)
			n = textline->max_line_size;

		textline->remainder = gsf_input_read (textline->source, n, NULL);
		if (textline->remainder == NULL)
			return NULL;
	}
}
