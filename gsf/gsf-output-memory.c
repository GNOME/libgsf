/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-memory.c:
 *
 * Copyright (C) 2002-2004 Dom Lachowicz (cinamod@hotmail.com)
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
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-output-impl.h>
#include <gsf/gsf-impl-utils.h>
#include <string.h>

#define MIN_BLOCK 512
#define MAX_STEP  MIN_BLOCK * 128

struct _GsfOutputMemory {
	GsfOutput output;
	guint8 *buffer;
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
	return g_object_new (GSF_OUTPUT_MEMORY_TYPE, NULL);	
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
	
	parent_class = g_type_class_peek (GSF_OUTPUT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static gboolean
gsf_output_memory_seek (GsfOutput *output, gsf_off_t offset,
			GSeekType whence)
{
	/* let parent implementation handle maneuvering cur_offset */
	(void)output;
	(void)offset;
	(void)whence;

	return TRUE;
}

static gboolean
gsf_output_memory_expand (GsfOutputMemory *mem, gsf_off_t min_capacity)
{
	gsf_off_t capacity = MAX (mem->capacity, MIN_BLOCK);
	gsf_off_t needed   = min_capacity;
	
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
	
	mem->buffer   = g_renew (guint8, mem->buffer, capacity);
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
		mem->buffer   = g_new (guint8, MIN_BLOCK);
		mem->capacity = MIN_BLOCK;
	}
	if (num_bytes + output->cur_offset > mem->capacity) {
		if (!gsf_output_memory_expand (mem, output->cur_offset + num_bytes))
			return FALSE;
	}
	
	memcpy (mem->buffer + output->cur_offset, buffer, num_bytes);	
	return TRUE;
}

#define GET_OUTPUT_CLASS(instance) \
         G_TYPE_INSTANCE_GET_CLASS (instance, GSF_OUTPUT_TYPE, GsfOutputClass)

static gboolean
gsf_output_memory_vprintf (GsfOutput *output, char const *format, va_list args)
{
	GsfOutputMemory *mem = (GsfOutputMemory *)output;
	GsfOutputClass *klass;
	gulong len;
	
	if (mem->buffer) {
		len = g_vsnprintf (mem->buffer + output->cur_offset,
				   mem->capacity - output->cur_offset,
				   format, args);
		if (len < mem->capacity - output->cur_offset) {
			/* There was sufficient space */
			output->cur_offset += len;
			return TRUE;
		}
	}
	klass = (GsfOutputClass *) (g_type_class_peek_parent
				    (GET_OUTPUT_CLASS (output)));
	return klass->Vprintf (output, format, args);
}

static void
gsf_output_memory_init (GObject *obj)
{
	GsfOutputMemory *mem = GSF_OUTPUT_MEMORY (obj);

	mem->buffer   = NULL;
	mem->capacity = 0;
}

static void
gsf_output_memory_class_init (GObjectClass *gobject_class)
{
	GsfOutputClass *output_class = GSF_OUTPUT_CLASS (gobject_class);
	
	gobject_class->finalize = gsf_output_memory_finalize;
	output_class->Close     = gsf_output_memory_close;
	output_class->Seek      = gsf_output_memory_seek;
	output_class->Write     = gsf_output_memory_write;
	output_class->Vprintf   = gsf_output_memory_vprintf;
}

/**
 * gsf_output_memory_get_bytes :
 * @mem : the output device.
 * 
 * Returns: The data that has been written to @mem, or %null
 **/
const guint8 *
gsf_output_memory_get_bytes (GsfOutputMemory * mem)
{
	g_return_val_if_fail (mem != NULL, NULL);
	return mem->buffer;
}

GSF_CLASS (GsfOutputMemory, gsf_output_memory,
           gsf_output_memory_class_init, gsf_output_memory_init, GSF_OUTPUT_TYPE)

