/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input-gzip.c: wrapper to uncompress gzipped input
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
#include <gsf/gsf-input-gzip.h>
#include <gsf/gsf-input-impl.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-utils.h>

#include <zlib.h>
#include <stdio.h>
#include <string.h>

#define Z_BUFSIZE 0x100

struct _GsfInputGZip {
	GsfInput input;

	GsfInput *source; /* compressed data */

	z_stream  stream;
	guint8 const *gzipped_data;
	uLong     crc;     /* crc32 of uncompressed data */

	guint8   *buf;
	size_t    buf_size;
	size_t    seek_skipped;
};

typedef struct {
	GsfInputClass input_class;
} GsfInputGZipClass;

/* gzip flag byte */
#define GZIP_IS_ASCII		0x01 /* file contains text ? */
#define GZIP_HEADER_CRC		0x02 /* there is a CRC in the header */
#define GZIP_EXTRA_FIELD	0x04 /* there is an 'extra' field */
#define GZIP_ORIGINAL_NAME	0x08 /* the original is stored */
#define GZIP_HAS_COMMENT	0x10 /* There is a comment in the header */
#define GZIP_HEADER_FLAGS (unsigned)(GZIP_IS_ASCII |GZIP_HEADER_CRC |GZIP_EXTRA_FIELD |GZIP_ORIGINAL_NAME |GZIP_HAS_COMMENT)

static gboolean
check_header (GsfInputGZip *input)
{
	static guint8 const signature [2] = {0x1f, 0x8b};

	guint8 const *data;
	unsigned flags, len;

	/* Get the uncompressed size first, so that the seeking is finished */
	if (gsf_input_seek (input->source, -4, GSF_SEEK_END) ||
	    NULL == (data = gsf_input_read (input->source, 4, NULL)))
		return TRUE;
	gsf_input_set_size (GSF_INPUT (input), (size_t) GSF_LE_GET_GUINT32 (data));

	/* Check signature */
	if (gsf_input_seek (input->source, 0, GSF_SEEK_SET) ||
	    NULL == (data = gsf_input_read (input->source, 2 + 1 + 1 + 6, NULL)) ||
	    0 != memcmp (data, signature, sizeof (signature)))
		return TRUE;

	/* verify flags and compression type */
	flags  = data [3];
	if (data [2] != Z_DEFLATED || (flags & ~GZIP_HEADER_FLAGS) != 0)
		return TRUE;

	if (flags & GZIP_EXTRA_FIELD) {
		if (NULL == (data = gsf_input_read (input->source, 2, NULL)))
			return TRUE;
		len = data [0] | (data [1] << 8);
		if (NULL == gsf_input_read (input->source, len, NULL))
			return TRUE;
	}
	if (flags & GZIP_ORIGINAL_NAME)
		do {
			if (NULL == (data = gsf_input_read (input->source, 1, NULL)))
				return TRUE;
		} while (*data != 0);

	if (flags & GZIP_HAS_COMMENT)
		do {
			if (NULL == (data = gsf_input_read (input->source, 1, NULL)))
				return TRUE;
		} while (*data != 0);

	if (flags & GZIP_HEADER_CRC &&
	    NULL == (data = gsf_input_read (input->source, 2, NULL)))
		return TRUE;

	return FALSE;
}

/**
 * gsf_input_gzip_new :
 * @source : The underlying data source.
 * @err	   : optionally NULL.
 *
 * Adds a reference to @source.
 *
 * Returns a new file or NULL.
 **/
GsfInputGZip *
gsf_input_gzip_new (GsfInput *source, GError **err)
{
	GsfInputGZip *gzip;

	g_return_val_if_fail (GSF_IS_INPUT (source), NULL);

	gzip = g_object_new (GSF_INPUT_GZIP_TYPE, NULL);
	g_object_ref (G_OBJECT (source));
	gzip->source = source;

	if (Z_OK != inflateInit2 (&(gzip->stream), -MAX_WBITS)) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"Unable to initialize zlib");
		g_object_unref (G_OBJECT (gzip));
		return NULL;
	}
	if (check_header (gzip)) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"Invalid gzip header");
		g_object_unref (G_OBJECT (gzip));
		return NULL;
	}

	return gzip;
}

static void
gsf_input_gzip_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInputGZip *gzip = (GsfInputGZip *)obj;

	if (gzip->source != NULL) {
		g_object_unref (G_OBJECT (gzip->source));
		gzip->source = NULL;
	}

	if (gzip->stream.state != NULL)
		inflateEnd (&(gzip->stream));

	parent_class = g_type_class_peek (GSF_INPUT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static GsfInput *
gsf_input_gzip_dup (GsfInput *src_input, GError **err)
{
	GsfInputGZip const *src = (GsfInputGZip *)src_input;
	GsfInputGZip *dst = g_object_new (GSF_INPUT_GZIP_TYPE, NULL);

	(void) err;

	dst->source = src->source;
	g_object_ref (G_OBJECT (dst->source));

	return GSF_INPUT (dst);
}

static guint8 const *
gsf_input_gzip_read (GsfInput *input, size_t num_bytes, guint8 *buffer)
{
	GsfInputGZip *gzip = GSF_INPUT_GZIP (input);
	size_t n;
	int zerr;

	if (buffer == NULL) {
		if (gzip->buf_size < num_bytes) {
			gzip->buf_size = num_bytes;
			if (gzip->buf != NULL)
				g_free (gzip->buf);
			gzip->buf = g_malloc (gzip->buf_size);
		}
		buffer = gzip->buf;
	}

	gzip->stream.next_out = buffer;
	gzip->stream.avail_out = num_bytes;
	while (gzip->stream.avail_out != 0) {
		if (gzip->stream.avail_in == 0) {
			/* the last 8 bytes are the crc and size, but we need 1
			 * byte to flush the decompresion.
			 */
			n = gsf_input_remaining (gzip->source) - 7;
			if (n > Z_BUFSIZE)
				n = Z_BUFSIZE;

			if (NULL == (gzip->gzipped_data = gsf_input_read (gzip->source, n, NULL)))
				return NULL;
			gzip->stream.avail_in = n;
			gzip->stream.next_in = (Byte *)gzip->gzipped_data;
		}
		zerr = inflate (&(gzip->stream), Z_NO_FLUSH);
	}

	gzip->crc = crc32 (gzip->crc, buffer, (uInt)(gzip->stream.next_out - buffer));
	return buffer;
}

static gboolean
gsf_input_gzip_seek (GsfInput *input, off_t offset, GsfOff_t whence)
{
	GsfInputGZip *gzip = GSF_INPUT_GZIP (input);
	off_t pos = offset;

	switch (whence) {
	case GSF_SEEK_SET : break;
	case GSF_SEEK_CUR : pos += input->cur_offset;	break;
	case GSF_SEEK_END : pos += input->size;		break;
	default : return TRUE;
	}

	/* Note, that pos has already been sanity checked.  */
	if ((size_t)pos < input->cur_offset) {
		if (Z_OK != inflateReset (&(gzip->stream)))
			return TRUE;
		input->cur_offset = 0;
	}

	while ((size_t)pos > input->cur_offset) {
		static gboolean warned = FALSE;
		size_t readcount = pos - input->cur_offset;
		char buffer[8192];
		if (readcount > sizeof (buffer))
			readcount = sizeof (buffer);
		if (!gsf_input_read (input, readcount, buffer))
			return TRUE;

		gzip->seek_skipped += readcount;
		if (!warned && gzip->seek_skipped >= 1000000) {
			warned = TRUE;
			g_warning ("Seeking in gzipped streams is awfully slow.");
		}		
	}
	return FALSE;
}

static void
gsf_input_gzip_init (GObject *obj)
{
	GsfInputGZip *gzip = GSF_INPUT_GZIP (obj);

	gzip->source = NULL;
	gzip->stream.zalloc	= (alloc_func)0;
	gzip->stream.zfree	= (free_func)0;
	gzip->stream.opaque	= (voidpf)0;
	gzip->stream.next_in	= Z_NULL;
	gzip->stream.next_out	= Z_NULL;
	gzip->stream.avail_in	= gzip->stream.avail_out = 0;
	gzip->crc		= crc32 (0L, Z_NULL, 0);
	gzip->buf		= NULL;
	gzip->buf_size		= 0;
}

static void
gsf_input_gzip_class_init (GObjectClass *gobject_class)
{
	GsfInputClass *input_class = GSF_INPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_input_gzip_finalize;
	input_class->Dup	= gsf_input_gzip_dup;
	input_class->Read	= gsf_input_gzip_read;
	input_class->Seek	= gsf_input_gzip_seek;
}

GSF_CLASS (GsfInputGZip, gsf_input_gzip,
	   gsf_input_gzip_class_init, gsf_input_gzip_init, GSF_INPUT_TYPE)

