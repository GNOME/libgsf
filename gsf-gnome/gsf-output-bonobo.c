/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-bonobo.c: interface for use by the structured file layer to write raw data
 *
 * Copyright (C) 2002 Dom Lachowicz (cinamod@hotmail.com)
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
#include <gsf-gnome/gsf-output-bonobo.h>
#include <gsf/gsf-output-impl.h>
#include <gsf/gsf-impl-utils.h>

struct _GsfOutputBonobo {
    GsfOutput output;
    Bonobo_Stream stream ;
};

typedef struct {
    GsfOutputClass output_class;
} GsfOutputBonoboClass;

/**
* gsf_output_bonobo_new :
 * @stream : non-NULL bonobo stream
 * @err	     : optionally NULL.
 *
 * Returns a new file or NULL.
 **/
GsfOutput *
gsf_output_bonobo_new (Bonobo_Stream const stream, GError **err)
{
    GsfOutputBonobo *output;

    output = g_object_new (GSF_OUTPUT_BONOBO_TYPE, NULL);
    output->stream = stream;

    return GSF_OUTPUT (output);
}

static gboolean
gsf_output_bonobo_close (GsfOutput *output)
{
    GsfOutputBonobo *boutput = GSF_OUTPUT_BONOBO (output);
    gboolean res = FALSE;

    if (boutput->stream != NULL) {
        boutput->stream = NULL;
        res = TRUE;
    }

    return res;
}

static gboolean
gsf_output_bonobo_seek (GsfOutput *output, gsf_off_t offset,
			GSeekType whence)
{
    GsfOutputBonobo *boutput = GSF_OUTPUT_BONOBO (output);
    Bonobo_Stream_SeekType bwhence;
    CORBA_long pos;
    CORBA_Environment ev;

    g_return_val_if_fail (boutput != NULL, TRUE);
    g_return_val_if_fail (boutput->stream != NULL, TRUE);

    switch (whence) {
        case G_SEEK_SET :
            bwhence =  Bonobo_Stream_SeekSet;
            break;
        case G_SEEK_CUR :
            bwhence = Bonobo_Stream_SeekCur;
            break;
        case G_SEEK_END :
            bwhence = Bonobo_Stream_SeekEnd;
            break;
        default:
            return TRUE;
    }

    CORBA_exception_init (&ev);
    pos = Bonobo_Stream_seek
        (boutput->stream, offset, bwhence, &ev);
    if (BONOBO_EX (&ev)) {
        g_warning (bonobo_exception_get_text (&ev));
        return TRUE;
    } else {
        return FALSE;
    }
}

static gboolean
gsf_output_bonobo_write (GsfOutput *output,
                         size_t num_bytes,
                         guint8 const *buffer)
{
    GsfOutputBonobo *boutput = GSF_OUTPUT_BONOBO (output);
    size_t res;
    CORBA_unsigned_long num_written;
    Bonobo_Stream_iobuf *bsobuf;
    CORBA_Environment ev;

    g_return_val_if_fail (boutput != NULL, FALSE);
    g_return_val_if_fail (boutput->stream != NULL, FALSE);

    CORBA_exception_init (&ev);
    Bonobo_Stream_write (boutput->stream, (CORBA_long) num_bytes,
                         &bsobuf, &ev);
    if (BONOBO_EX (&ev)) {
        g_warning (bonobo_exception_get_text (&ev));
        return FALSE;
    } else {
        memcpy (buffer, bsobuf->_buffer, bsobuf->_length);
        num_read = bsobuf->_length;
        CORBA_free (bsobuf);
    }
    return ((size_t) num_read == num_bytes);
}

static void
gsf_output_bonobo_finalize (GObject *obj)
{
    GObjectClass *parent_class;
    GsfOutput *output = (GsfOutput *)obj;

    gsf_output_bonobo_close (output);

    parent_class = g_type_class_peek (GSF_OUTPUT_TYPE);
    if (parent_class && parent_class->finalize)
        parent_class->finalize (obj);
}

static void
gsf_output_bonobo_init (GObject *obj)
{
    GsfOutputBonobo *stream = GSF_OUTPUT_BONOBO (obj);

    stream->stream = NULL;
}

static void
gsf_output_bonobo_class_init (GObjectClass *gobject_class)
{
    GsfOutputClass *output_class = GSF_OUTPUT_CLASS (gobject_class);

    gobject_class->finalize = gsf_output_bonobo_finalize;
    output_class->Close	= gsf_output_bonobo_close;
    output_class->Seek	= gsf_output_bonobo_seek;
    output_class->Write	= gsf_output_bonobo_write;
}

GSF_CLASS (GsfOutputBonobo, gsf_output_bonobo,
           gsf_output_bonobo_class_init, gsf_output_bonobo_init, GSF_OUTPUT_TYPE)
