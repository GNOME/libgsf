/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-gzip.c: wrapper to compress to gzipped output. See rfc1952.
 *
 * Copyright (C) 2002 Jon K Hellan (hellan@acm.org)
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
#include <gsf/gsf-output-gzip.h>
#include <gsf/gsf-output-impl.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-utils.h>

#include <zlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#define Z_BUFSIZE 0x100

struct _GsfOutputGZip {
	GsfOutput output;

	GsfOutput *sink; /* compressed data */

	z_stream  stream;
/* 	guint8 const *gzipped_data; */
	uLong     crc;     /* crc32 of uncompressed data */
	size_t    isize;
	
	guint8   *buf;
	size_t    buf_size;
};

typedef struct {
	GsfOutputClass output_class;
} GsfOutputGZipClass;

/* gzip flag byte */
#define GZIP_ORIGINAL_NAME	0x08 /* the original is stored */

static gboolean
init_gzip (GsfOutputGZip *gzip, GError **err)
{
	int ret;
	
	ret = deflateInit2 (&gzip->stream, Z_DEFAULT_COMPRESSION,
			    Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL,
			    Z_DEFAULT_STRATEGY);
	if (ret != Z_OK) {
		if (err != NULL)
			*err = g_error_new (gsf_output_error_id (), 0,
					    "Unable to initialize deflate");
		return FALSE;
	}
	if (!gzip->buf) {
		gzip->buf_size = Z_BUFSIZE; 
		gzip->buf = g_new (guint8, gzip->buf_size);
	}
	gzip->stream.next_out  = gzip->buf;
	gzip->stream.avail_out = gzip->buf_size;

	return TRUE;
}

static gboolean
gzip_output_header (GsfOutputGZip *gzip)
{
	guint8 buf[3 + 1 + 4 + 2];
	static guint8 const gzip_signature[] = { 0x1f, 0x8b, 0x08 } ;
	time_t mtime = time (NULL);
	char const *name = gsf_output_name (gzip->sink);
	/* FIXME: What to do about gz extension ... ? */
	int nlen = 0;  /* name ? strlen (name) : 0; */
	gboolean ret;
	
	memset (buf, 0, sizeof buf);
	memcpy (buf, gzip_signature, 3);
	if (nlen > 0)
		buf[3] = GZIP_ORIGINAL_NAME;
	GSF_LE_SET_GUINT32 (buf + 4, (guint32) mtime);
	buf[9] = 3;	/* UNIX */
	ret = gsf_output_write (gzip->sink, sizeof buf, buf);
	if (ret && name && nlen > 0)
		ret = gsf_output_write (gzip->sink, nlen, name);

	return ret;
}
	
/**
 * gsf_output_gzip_new :
 * @sink : The underlying data source.
 * @err	   : optionally NULL.
 *
 * Adds a reference to @sink.
 *
 * Returns a new file or NULL.
 **/
GsfOutputGZip *
gsf_output_gzip_new (GsfOutput *sink, GError **err)
{
	GsfOutputGZip *gzip;

	g_return_val_if_fail (GSF_IS_OUTPUT (sink), NULL);

	gzip = g_object_new (GSF_OUTPUT_GZIP_TYPE, NULL);
	g_object_ref (G_OBJECT (sink));
	gzip->sink = sink;

	if (!init_gzip (gzip, err)) {
		g_object_unref (G_OBJECT (gzip));
		return NULL;
	}

	if (!gzip_output_header (gzip)) {
		g_object_unref (G_OBJECT (gzip));
		return NULL;
	}

	return gzip;
}

static void
gsf_output_gzip_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfOutputGZip *gzip = (GsfOutputGZip *)obj;

	if (gzip->sink != NULL) {
		g_object_unref (G_OBJECT (gzip->sink));
		gzip->sink = NULL;
	}

	parent_class = g_type_class_peek (GSF_OUTPUT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static gboolean
gzip_output_block (GsfOutputGZip *gzip)
{
	size_t num_bytes = gzip->buf_size - gzip->stream.avail_out;
	
	if (!gsf_output_write (gzip->sink, num_bytes, gzip->buf)) {
		return FALSE;
	}
	gzip->stream.next_out  = gzip->buf;
	gzip->stream.avail_out = gzip->buf_size;

	return TRUE;
}

static gboolean
gzip_flush (GsfOutputGZip *gzip)
{
	int zret;

	do {
		zret = deflate (&gzip->stream, Z_FINISH);
		if (zret == Z_OK) {
			/*  In this case Z_OK means more buffer space
			    needed  */
			if (!gzip_output_block (gzip))
				return FALSE;
		}
	} while (zret == Z_OK);
	if (zret != Z_STREAM_END)
		return FALSE;
	if (!gzip_output_block (gzip))
		return FALSE;

	return TRUE;
}

static gboolean
gsf_output_gzip_write (GsfOutput *output,
		       size_t num_bytes, guint8 const *data)
{
	GsfOutputGZip *gzip = GSF_OUTPUT_GZIP (output);

	g_return_val_if_fail (data, FALSE);

	gzip->stream.next_in  = (unsigned char *) data;
	gzip->stream.avail_in = num_bytes;
	
	while (gzip->stream.avail_in > 0) {
		if (gzip->stream.avail_out == 0) {
			if (!gzip_output_block (gzip))
				return FALSE;
		}
		if (deflate (&gzip->stream, Z_NO_FLUSH) != Z_OK)
			return FALSE;
	}

	gzip->crc = crc32 (gzip->crc, data, num_bytes);
	gzip->isize += num_bytes;

	return TRUE;
}

static gboolean
gsf_output_gzip_seek (GsfOutput *output, gsf_off_t offset, GSeekType whence)
{
	(void) output;
	(void) offset;
	(void) whence;
	
	return FALSE;
}

static gboolean
gsf_output_gzip_close (GsfOutput *output)
{
	GsfOutputGZip *gzip = GSF_OUTPUT_GZIP (output);
	guint8 buf[8];

	if (!gzip_flush (gzip))
		return FALSE;
	
	/* TODO: CRC, ISIZE */
	GSF_LE_SET_GUINT32 (buf,     gzip->crc);
	GSF_LE_SET_GUINT32 (buf + 4, gzip->isize);

	return gsf_output_write (gzip->sink, 8, buf);
}

static void
gsf_output_gzip_init (GObject *obj)
{
	GsfOutputGZip *gzip = GSF_OUTPUT_GZIP (obj);

	gzip->sink = NULL;
	gzip->stream.zalloc	= (alloc_func)0;
	gzip->stream.zfree	= (free_func)0;
	gzip->stream.opaque	= (voidpf)0;
	gzip->stream.next_in	= Z_NULL;
	gzip->stream.next_out	= Z_NULL;
	gzip->stream.avail_in	= gzip->stream.avail_out = 0;
	gzip->crc		= crc32 (0L, Z_NULL, 0);
	gzip->isize             = 0;
	gzip->buf		= NULL;
	gzip->buf_size		= 0;
}

static void
gsf_output_gzip_class_init (GObjectClass *gobject_class)
{
	GsfOutputClass *output_class = GSF_OUTPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_output_gzip_finalize;
	output_class->Write	= gsf_output_gzip_write;
	output_class->Seek	= gsf_output_gzip_seek;
	output_class->Close	= gsf_output_gzip_close;
}

GSF_CLASS (GsfOutputGZip, gsf_output_gzip,
	   gsf_output_gzip_class_init, gsf_output_gzip_init, GSF_OUTPUT_TYPE)

