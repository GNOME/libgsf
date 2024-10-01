/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-memory.c:
 *
 * Copyright (C) 2002-2006 Dom Lachowicz (cinamod@hotmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */
#include <gsf-config.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf.h>

#include <string.h>

#define MIN_BLOCK 512

static GsfOutputClass *parent_class;

struct _GsfOutputMemory {
	GsfOutput output;
	guint8 *buffer;
	size_t capacity;
};

typedef struct {
	GsfOutputClass output_class;
} GsfOutputMemoryClass;

/**
 * gsf_output_memory_new:
 *
 * Returns: (nullable): a new file.
 **/
GsfOutput *
gsf_output_memory_new (void)
{
	return g_object_new (GSF_OUTPUT_MEMORY_TYPE, NULL);
}

static gboolean
gsf_output_memory_close (GsfOutput *output)
{
	return (parent_class->Close == NULL) ||
		parent_class->Close (output);
}

static void
gsf_output_memory_finalize (GObject *obj)
{
	GsfOutputMemory *mem = GSF_OUTPUT_MEMORY (obj);

	g_free (mem->buffer);
	mem->buffer = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static gboolean
gsf_output_memory_seek (G_GNUC_UNUSED GsfOutput *output,
			G_GNUC_UNUSED gsf_off_t offset,
			G_GNUC_UNUSED GSeekType whence)
{
	/* let parent implementation handle maneuvering cur_offset */
	return TRUE;
}


/* Taken from gutilsprivate.h: (g_nearest_pow)
 * Returns the smallest power of 2 greater than or equal to n,
 * or 0 if such power does not fit in a gsize
 */
static inline gsize
gsf_round_up_to_pow2 (gsize num)
{
	gsize n = num - 1;

	g_assert (num > 0 && num <= G_MAXSIZE / 2);

	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
#if GLIB_SIZEOF_SIZE_T == 8
	n |= n >> 32;
#endif

	return n + 1;
}


/* Returns false in the event of overflow */
static inline gboolean
maybe_expand (GsfOutputMemory *mem, size_t offset, size_t requested)
{
	size_t needed, new_capacity;

	if (G_UNLIKELY (!g_size_checked_add (&needed, offset, requested))) {
		return FALSE;
	}

	if (needed < mem->capacity) {
		/* We've already allocated enough, nothing more to do */
		return TRUE;
	}

	new_capacity = gsf_round_up_to_pow2 (MAX (needed, MIN_BLOCK));
	/* if we can't over-allocate, allocate exactly */
	new_capacity = G_LIKELY (new_capacity > 0) ? new_capacity : needed;

	mem->buffer = g_renew (guint8, mem->buffer, new_capacity);
	mem->capacity = new_capacity;

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

	/* We can't allocate more than MAXSIZE bytes, so the offset will always
	 * be within size_t's range */
	if (!maybe_expand (mem, (size_t) output->cur_offset, num_bytes)) {
		g_warning ("overflow in gsf_output_memory_write");
		return FALSE;
	}

	memcpy (mem->buffer + output->cur_offset, buffer, num_bytes);
	return TRUE;
}

static gsf_off_t gsf_output_memory_vprintf (GsfOutput *output,
	char const *format, va_list args) G_GNUC_PRINTF (2, 0);

static gsf_off_t
gsf_output_memory_vprintf (GsfOutput *output, char const *format, va_list args)
{
	GsfOutputMemory *mem = (GsfOutputMemory *)output;

	if (mem->buffer) {
		va_list args2;
		gsf_off_t len;

		/*
		 * We need to make a copy as args will become unusable after
		 * the g_vsnprintf call.
		 */
		G_VA_COPY (args2, args);

		len = g_vsnprintf (mem->buffer + output->cur_offset,
				   mem->capacity - output->cur_offset,
				   format, args);

		/* There was insufficient space */
		if (len >= (gsf_off_t)(mem->capacity - output->cur_offset))
			len = parent_class->Vprintf (output, format, args2);

		va_end (args2);

		return len;
	}
	return parent_class->Vprintf (output, format, args);
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

	parent_class = GSF_OUTPUT_CLASS (g_type_class_peek_parent (gobject_class));
}

/**
 * gsf_output_memory_get_bytes:
 * @mem: the output device.
 *
 * Returns: (array) (nullable): The data that has been written to @mem
 **/
const guint8 *
gsf_output_memory_get_bytes (GsfOutputMemory * mem)
{
	g_return_val_if_fail (mem != NULL, NULL);
	return mem->buffer;
}

/**
 * gsf_output_memory_steal_bytes:
 * @mem: the output device.
 *
 * Returns: (array) (nullable): The data that has been written to @mem.
 * The caller takes ownership and the buffer belonging to @mem is set
 * to %NULL.
 **/
guint8 *
gsf_output_memory_steal_bytes (GsfOutputMemory * mem)
{
	guint8 *bytes;

	g_return_val_if_fail (mem != NULL, NULL);
	bytes = mem->buffer;
	mem->buffer = NULL;
	mem->capacity = 0;
	return bytes;
}

GSF_CLASS (GsfOutputMemory, gsf_output_memory,
	   gsf_output_memory_class_init, gsf_output_memory_init,
	   GSF_OUTPUT_TYPE)

