/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input-memory.c: 
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
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-input-impl.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-shared-memory.h>

struct _GsfInputMemory {
	GsfInput parent;
	GsfSharedMemory *shared;
};
typedef struct {
	GsfInputClass input_class;
} GsfInputMemoryClass;

GsfInput *
gsf_input_memory_new (guint8 const *buf, unsigned length, gboolean needs_free)
{
	GsfInputMemory *mem = g_object_new (GSF_INPUT_MEMORY_TYPE, NULL);
	mem->shared = gsf_shared_memory_new ((void *)buf, length, needs_free);
	gsf_input_set_size (GSF_INPUT (mem), length);
	return GSF_INPUT (mem);
}

static void
gsf_input_memory_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInputMemory *mem = (GsfInputMemory *) (obj);

	if (mem->shared)
		g_object_unref (G_OBJECT (mem->shared));

	parent_class = g_type_class_peek (GSF_INPUT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static GsfInput *
gsf_input_memory_dup (GsfInput *src_input)
{
	GsfInputMemory const *src = (GsfInputMemory *) (src_input);
	GsfInputMemory *dst = g_object_new (GSF_INPUT_MEMORY_TYPE, NULL);

	dst->shared = src->shared;
	g_object_ref (G_OBJECT (dst->shared));

	return GSF_INPUT (dst);
}

static guint8 const *
gsf_input_memory_read (GsfInput *input, unsigned num_bytes, guint8 *optional_buffer)
{
	GsfInputMemory *mem = (GsfInputMemory *) (input);
	const char *src = mem->shared->buf;

	if (src == NULL)
		return NULL;
	if (optional_buffer) {
		memcpy (optional_buffer, src + input->cur_offset, num_bytes);
		return optional_buffer;
	} else
		return src + input->cur_offset;
}

static int
gsf_input_memory_seek (GsfInput *input, int offset, GsfOff_t whence)
{
	(void)input;
	(void)offset;
	(void)whence;
	return FALSE;
}

static void
gsf_input_memory_init (GObject *obj)
{
	GsfInputMemory *mem = (GsfInputMemory *) (obj);
	mem->shared = NULL;
}

static void
gsf_input_memory_class_init (GObjectClass *gobject_class)
{
	GsfInputClass *input_class = GSF_INPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_input_memory_finalize;
	input_class->Dup	= gsf_input_memory_dup;
	input_class->Read	= gsf_input_memory_read;
	input_class->Seek	= gsf_input_memory_seek;
}

GSF_CLASS (GsfInputMemory, gsf_input_memory,
	   gsf_input_memory_class_init, gsf_input_memory_init,
	   GSF_INPUT_TYPE)

/***************************************************************************/

#ifdef HAVE_MMAP

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifndef PROT_READ
#define PROT_READ 0x1
#endif

#if !defined(MAP_FAILED) || defined(__osf__)
/* Someone needs their head examined - BSD ? */
#	define MAP_FAILED ((void *)-1)
#endif
#endif

GsfInput *
gsf_input_mmap_new (char const *filename, GError **err)
{
#ifdef HAVE_MMAP
	GsfInputMemory *mem;
	guint8 *buf = NULL;
	struct stat st;
	int fd;
	size_t size;

	fd = open (filename, O_RDONLY);
	if (fd < 0 || fstat (fd, &st) < 0) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"%s: %s", filename, g_strerror (errno));
		return NULL;
	}

	if (!S_ISREG (st.st_mode)) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"%s: Is not a regular file", filename);
		return NULL;
	}
	
	size = st.st_size;
	buf = mmap (0, size, PROT_READ, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"%s: %s", filename, g_strerror (errno));
		close (fd);
		return NULL;
	}
	close (fd);

	mem = g_object_new (GSF_INPUT_MEMORY_TYPE, NULL);
	mem->shared = gsf_shared_memory_mmapped_new (buf, size);
	gsf_input_set_size (GSF_INPUT (mem), size);
	return GSF_INPUT (mem);
#else
	(void)filename;
	if (err != NULL)
		*err = g_error_new (gsf_input_error (),
			"MMAP Unsupported");
	return NULL;
#endif
}
