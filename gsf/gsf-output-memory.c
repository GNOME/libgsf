/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-memory.c:
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
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-output-impl.h>
#include <gsf/gsf-impl-utils.h>

struct _GsfOutputMemory {
    GsfOutput output;
    guint8 *buffer;
    size_t nwritten;
};

typedef struct {
    GsfOutputClass output_class;
} GsfOutputMemoryClass;

/**
* gsf_output_memory_new :
* Returns a new file or NULL.
**/
GsfOutput *
gsf_output_memory_new (void)
{
    GsfOutputMemory *output = g_object_new (GSF_OUTPUT_MEMORY_TYPE, NULL);
    output->buffer   = NULL;
    output->nwritten = 0;

    return GSF_OUTPUT (output);
}

static gboolean
gsf_output_memory_close (GsfOutput *output)
{
    GsfOutputMemory *mem = GSF_OUTPUT_MEMORY (output);
    gboolean res = FALSE;

    if (mem->buffer != NULL) {
        g_free (mem->buffer);
        mem->buffer = NULL;
    }
    mem->nwritten = 0;

    return TRUE;
}

static void
gsf_output_memory_finalize (GObject *obj)
{
    GObjectClass *parent_class;
    GsfOutput *output = (GsfOutput *)obj;

    gsf_output_memory_close (output);

    parent_class = g_type_class_peek (GSF_OUTPUT_TYPE);
    if (parent_class && parent_class->finalize)
        parent_class->finalize (obj);
}

static gboolean
gsf_output_memory_seek (GsfOutput *output, gsf_off_t offset,
			GsfSeekType whence)
{
    (void)output;
    (void)offset;
    (void)whence;
    return FALSE;
}

static gboolean
gsf_output_memory_write (GsfOutput *output,
                        size_t num_bytes,
                        guint8 const *buffer)
{
    GsfOutputMemory *mem = GSF_OUTPUT_MEMORY (output);
    size_t res;

    g_return_val_if_fail (mem != NULL, FALSE);

    mem->buffer = g_realloc (mem->buffer, mem->nwritten + num_bytes);
    memcpy (mem->buffer + mem->nwritten, buffer, num_bytes);

    return TRUE;
}

static void
gsf_output_memory_init (GObject *obj)
{
    GsfOutputMemory *mem = GSF_OUTPUT_MEMORY (obj);

    mem->buffer   = NULL;
    mem->nwritten = 0;
}

static void
gsf_output_memory_class_init (GObjectClass *gobject_class)
{
    GsfOutputClass *output_class = GSF_OUTPUT_CLASS (gobject_class);

    gobject_class->finalize = gsf_output_memory_finalize;
    output_class->Close	= gsf_output_memory_close;
    output_class->Seek	= gsf_output_memory_seek;
    output_class->Write	= gsf_output_memory_write;
}

/**
* gsf_output_memory_get_bytes :
 * @output : the output device.
 * @outbuffer     : optionally NULL, where to store a *non-freeable* pointer to the contents (i.e. get the length and copy it yourself before unref'ing @output).
 * @outlength     : optionally NULL, the returned size of the in-memory contents
 **/
void
gsf_output_memory_get_bytes (GsfOutput * output, guint8 ** outbuffer, gsf_off_t * outlength)
{
    GsfOutputMemory * mem;

    /* set these to 0 to start out with */
    if (outbuffer != NULL)
        *outbuffer = NULL;
    if (outlength != NULL)
        *outlength = 0;

    g_return_if_fail (output != NULL);

    mem = GSF_OUTPUT_MEMORY (output);
    if (outbuffer != NULL)
        *outbuffer = mem->buffer;
    if (outlength != NULL)
        *outlength = mem->nwritten;
}

GSF_CLASS (GsfOutputMemory, gsf_output_memory,
           gsf_output_memory_class_init, gsf_output_memory_init, GSF_OUTPUT_TYPE)
