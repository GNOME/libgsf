/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-win32.c: 
 *
 * Copyright (C) 2003-2004 Dom Lachowicz <cinamod@hotmail.com>
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
#include <gsf-win32/gsf-output-win32.h>
#include <gsf/gsf-output-impl.h>
#include <gsf/gsf-impl-utils.h>
#include <string.h>

struct _GsfOutputIStream {
	GsfOutput output;
	IStream * stream;
};

typedef struct {
	GsfOutputClass output_class;
} GsfOutputStreamClass;

static void 
hresult_to_gerror (HRESULT hr, GError ** err)
{
	if (err)
		*err = g_error_new (gsf_output_error (), hr,
				    "HRESULT != S_OK");
}

/**
 * gsf_output_istream_new :
 * @stream   : IStream stream
 *
 * Returns a new output object or NULL.
 **/
GsfOutputIStream *
gsf_output_istream_new (IStream * stream)
{
	GsfOutputIStream *output;

	if (stream == NULL) {
		return NULL;
	}

	output = g_object_new (GSF_OUTPUT_ISTREAM_TYPE, NULL);
	output->stream = stream;
	output->stream->Ref ();

	return output;
}

static gboolean
gsf_output_istream_close (GsfOutput *output)
{
	GsfOutputIStream *istream = GSF_OUTPUT_ISTREAM (output);
	gboolean res = FALSE;

	if (istream->stream != NULL) {
		istream->stream->Release (istream->stream);
		istream->stream = NULL;
		res = TRUE;
	}

	return res;
}

static void
gsf_output_istream_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfOutputIStream *output = (GsfOutputIStream *)obj;

	gsf_output_istream_close (output);

	parent_class = g_type_class_peek (GSF_OUTPUT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static gboolean
gsf_output_istream_write (GsfOutput *output,
			  size_t num_bytes,
			  guint8 const *buffer)
{
	GsfOutputIStream *istm = GSF_OUTPUT_ISTREAM (output);
	HRESULT hr;
	ULONG nwritten, total_written = 0;

	g_return_val_if_fail (istm != NULL, FALSE);
	g_return_val_if_fail (istm->stream != NULL, FALSE);

	while(1) {
		hr = istm->stream->Write(istm->stream, (buffer + total_written), (ULONG)(num_bytes - total_written), &nwritten);
		
		if (SUCCEEDED(hr)) {
			total_written += nwritten;
			if (total_written == num_bytes)
				return TRUE;
		} else {
			g_warning ("Write failed\n");
			return FALSE;
		}	
	}
	
	return TRUE;
}

static gboolean
gsf_output_istream_seek (GsfOutput *output, gsf_off_t offset, GSeekType whence)
{
	GsfOutputIStream *istm = GSF_OUTPUT_ISTREAM (output);
	DWORD dwhence;

	g_return_val_if_fail (istm != NULL, gsf_output_set_error(output, 0, "missing handle"));
	g_return_val_if_fail (istm->stream != NULL, gsf_output_set_error(output, 0, "missing handle"));
	
	switch (whence) {
	case G_SEEK_SET :
		dwhence = STREAM_SEEK_SET;
		break;
	case G_SEEK_CUR :
		dwhence = STREAM_SEEK_CUR;
		break;
	case G_SEEK_END :
		dwhence = STREAM_SEEK_END;
		break;
	default:
		break; /* checked in parent wrapper */
	}
	
	if (SUCCEEDED(istm->stream->Seek (istm->stream, (LARGE_INTEGER)offset, dwhence, NULL)))
		return TRUE;

	return gsf_output_set_error(output, 0, "seek failed");
}

static void
gsf_output_istream_init (GObject *obj)
{
	GsfOutputIStream *istm = GSF_OUTPUT_ISTREAM (obj);

	istm->stream = NULL;
}

static void
gsf_output_istream_class_init (GObjectClass *gobject_class)
{
	GsfOutputClass *output_class = GSF_OUTPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_output_istream_finalize;
	output_class->Close	= gsf_output_istream_close;
	output_class->Write	= gsf_output_istream_write;
	output_class->Seek	= gsf_output_istream_seek;
}

GSF_CLASS (GsfOutputIStream, gsf_output_istream,
	   gsf_output_istream_class_init, gsf_output_istream_init, GSF_OUTPUT_TYPE)

/***************************************************************************/
/***************************************************************************/
