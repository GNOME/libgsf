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

struct _GsfInputMemory {
	GsfInput parent;

	gboolean needs_free;
	guint8 const *buf;
	int length;
	int cur_pos;
};
typedef struct {
	GsfInputClass input_class;
} GsfInputMemoryClass;

static GsfInput *
gsf_input_memory_construct (GsfInputMemory *mem,
			    guint8 const *buf, int length,
			    gboolean needs_free)
{
	mem->buf        = buf;
	mem->length     = length;
	mem->needs_free = needs_free;
	mem->cur_pos    = 0;
	return GSF_INPUT (mem);
}

GsfInput *
gsf_input_memory_new (guint8 const *buf, int length, gboolean needs_free)
{
	return gsf_input_memory_construct (
			g_object_new (GSF_INPUT_MEMORY_TYPE, NULL),
			buf, length, needs_free);
}

static guint8 const *
gsf_input_memory_read (GsfInput *input, int num_bytes)
{
	GsfInputMemory *mem = GSF_INPUT_MEMORY (input);
	guint8 const *res;

	if (mem->buf == NULL || (mem->cur_pos + num_bytes) >= mem->length)
		return NULL;
	res = mem->buf + mem->cur_pos;
	mem->cur_pos += num_bytes;
	return res;
}

static void
gsf_input_memory_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInputMemory *mem = GSF_INPUT_MEMORY (obj);

	if (mem->buf != NULL) {
		g_return_if_fail (mem->length > 0);
		if (mem->needs_free)
			g_free ((char *)mem->buf);
	}
	mem->buf = NULL;
	mem->length = -1;
	mem->needs_free = FALSE;

	parent_class = g_type_class_peek (GSF_INPUT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static void
gsf_input_memory_init (GObject *obj)
{
	GsfInputMemory *mem = GSF_INPUT_MEMORY (obj);

	mem->buf = NULL;
	mem->length = -1;
	mem->needs_free = FALSE;
}

static void
gsf_input_memory_class_init (GObjectClass *gobject_class)
{
	GsfInputClass *input_class = GSF_INPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_input_memory_finalize;
	input_class->read   = gsf_input_memory_read;
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

struct _GsfInputMMap {
	GsfInputMemory parent;

	int fd;
};
typedef struct {
	GsfInputMemoryClass mem_class;
} GsfInputMMapClass;

GsfInput *
gsf_input_mmap_new (char const *filename, GError **err)
{
	GsfInputMMap *m_map;
	guint8 *buf = NULL;
	struct stat st;
	int fd;

	fd = open (filename, O_RDONLY);
	if (fd < 0 || fstat (fd, &st) < 0) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"%s : %s", filename, g_strerror (errno));
		return NULL;
	}

	buf = mmap (0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if ((caddr_t)buf == (caddr_t)MAP_FAILED) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"%s : %s", filename, g_strerror (errno));
		close (fd);
		return NULL;
	}

	m_map = g_object_new (GSF_INPUT_MMAP_TYPE, NULL);
	m_map->fd = fd;

	return gsf_input_memory_construct (GSF_INPUT_MEMORY (m_map),
		buf, st.st_size, FALSE);
}

static void
gsf_input_mmap_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInputMMap *m_map = GSF_INPUT_MMAP (obj);

	if (m_map->fd >= 0) {
		GsfInputMemory *mem = GSF_INPUT_MEMORY (obj);
		munmap ((char *)mem->buf, mem->length);
		mem->buf = NULL;
		mem->length = -1;
		m_map->fd = -1;
	}

	parent_class = g_type_class_peek (GSF_INPUT_MEMORY_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static void
gsf_input_mmap_init (GObject *obj)
{
	GsfInputMMap *m_map = GSF_INPUT_MMAP (obj);

	m_map->fd = -1;
}

static void
gsf_input_mmap_class_init (GObjectClass *gobject_class)
{
	gobject_class->finalize = gsf_input_mmap_finalize;
}

GSF_CLASS (GsfInputMMap, gsf_input_mmap,
	   gsf_input_mmap_class_init, gsf_input_mmap_init,
	   GSF_INPUT_MEMORY_TYPE)
#else
GType gsf_input_mmap_get_type (void) { return (GType) 0; }
GsfInput *
gsf_input_mmap_new (char const *filename, GError **err)
{
	if (err != NULL)
		*err = g_error_new (gsf_input_error (),
			"MMAP Unsupported");
	return NULL;
}
#endif
