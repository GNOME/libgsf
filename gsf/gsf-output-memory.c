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

#define MIN_BLOCK 512
#define MAX_STEP  MIN_BLOCK * 128

struct _GsfOutputMemory {
    GsfOutput output;
    guint8 *buffer;
    size_t nwritten;
    size_t capacity;
};

typedef struct {
    GsfOutputClass output_class;
} GsfOutputMemoryClass;

/**
 * gsf_output_memory_new :
 *
 * Returns a new file or NULL.
 **/
GsfOutput *
gsf_output_memory_new (void)
{
    GsfOutputMemory *output = g_object_new (GSF_OUTPUT_MEMORY_TYPE, NULL);

    return GSF_OUTPUT (output);
}

static gboolean
gsf_output_memory_close (GsfOutput *output)
{
    GsfOutputClass *parent_class;

    parent_class = g_type_class_peek (GSF_OUTPUT_TYPE);
    if (parent_class && parent_class->Close)
        parent_class->Close (output);

    return TRUE;
}

static void
gsf_output_memory_finalize (GObject *obj)
{
    GObjectClass *parent_class;
    GsfOutput *output = (GsfOutput *)obj;
    GsfOutputMemory *mem = GSF_OUTPUT_MEMORY (output);

    if (mem->buffer != NULL) {
        g_free (mem->buffer);
        mem->buffer = NULL;
    }
    mem->nwritten = 0;

    parent_class = g_type_class_peek (GSF_OUTPUT_TYPE);
    if (parent_class && parent_class->finalize)
        parent_class->finalize (obj);
}

static gboolean
gsf_output_memory_seek (GsfOutput *output, gsf_off_t offset,
			GSeekType whence)
{
    (void)output;
    (void)offset;
    (void)whence;
    return TRUE;
}

static gboolean
gsf_output_memory_expand (GsfOutputMemory *mem, gsf_off_t min_capacity)
{
    size_t capacity = MAX (mem->capacity, MIN_BLOCK);
    size_t needed   = min_capacity;

    if ((gsf_off_t) min_capacity != needed) { /* Checking for overflow */
	g_warning ("overflow in gsf_output_memory_expand");
	return FALSE;
    }

    while (capacity < needed) {
	if (capacity <= MAX_STEP)
	    capacity *= 2;
	else
	    capacity += MAX_STEP;
    }
	    
    mem->buffer  = g_realloc (mem->buffer, capacity);
    mem->capacity = capacity;

    return TRUE;
}

static gboolean
gsf_output_memory_write (GsfOutput *output,
                        size_t num_bytes,
                        guint8 const *buffer)
{
    GsfOutputMemory *mem = GSF_OUTPUT_MEMORY (output);

    g_return_val_if_fail (mem != NULL, FALSE);

    if (!mem->buffer) {
	    mem->buffer   = g_malloc (MIN_BLOCK);
	    mem->capacity = MIN_BLOCK;
    }
    if (num_bytes + mem->nwritten > mem->capacity) {
	    if (!gsf_output_memory_expand (mem, mem->nwritten + num_bytes))
		    return FALSE;
    }

    memcpy (mem->buffer + mem->nwritten, buffer, num_bytes);
    mem->nwritten += num_bytes;

    return TRUE;
}

static void
gsf_output_memory_init (GObject *obj)
{
    GsfOutputMemory *mem = GSF_OUTPUT_MEMORY (obj);

    mem->buffer   = NULL;
    mem->nwritten = 0;
    mem->capacity = 0;
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
gsf_output_memory_get_bytes (GsfOutputMemory * mem,
			     guint8 ** outbuffer, gsf_off_t * outlength)
{
    /* set these to 0 to start out with */
    if (outbuffer != NULL)
        *outbuffer = NULL;
    if (outlength != NULL)
        *outlength = 0;

    g_return_if_fail (mem != NULL);

    if (outbuffer != NULL)
        *outbuffer = mem->buffer;
    if (outlength != NULL)
        *outlength = mem->nwritten;
}

GSF_CLASS (GsfOutputMemory, gsf_output_memory,
           gsf_output_memory_class_init, gsf_output_memory_init, GSF_OUTPUT_TYPE)
