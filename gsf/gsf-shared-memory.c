/*
 * gsf-shared-memory.c: 
 *
 * Copyright (C) 2002 Morten Welinder (terra@diku.dk)
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
#include <gsf/gsf-shared-memory.h>
#include <gsf/gsf-impl-utils.h>

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif

typedef struct {
	GObjectClass g_object_class;
} GsfSharedMemoryClass;

GsfSharedMemory *
gsf_shared_memory_new (void *buf, gsf_off_t size, gboolean needs_free)
{
	GsfSharedMemory *mem = g_object_new (GSF_SHARED_MEMORY_TYPE, NULL);
	mem->buf = buf;
	mem->size = size;
	mem->needs_free = needs_free;
	mem->needs_unmap = FALSE;
	return mem;
}

GsfSharedMemory *
gsf_shared_memory_mmapped_new (void *buf, gsf_off_t size)
{
	GsfSharedMemory *mem = gsf_shared_memory_new (buf, size, FALSE);
	mem->needs_unmap = TRUE;
	return mem;
}

static void
gsf_shared_memory_finalize (GObject *obj)
{
	GsfSharedMemory *mem = (GsfSharedMemory *) (obj);
	size_t msize;
	
	if (mem->buf != NULL) {
		if (mem->needs_free)
			g_free (mem->buf);
		else if (mem->needs_unmap) {
#ifdef HAVE_MMAP
			msize = mem->size;
			if ((gsf_off_t) msize != mem->size) {
				/* Check for overflow */
				g_warning ("memory buffer size too large");
			}
			munmap (mem->buf, msize);
#else
			g_assert_not_reached ();
#endif
		}
	}
}

static void
gsf_shared_memory_init (GObject *obj)
{
	GsfSharedMemory *mem = (GsfSharedMemory *) (obj);
	mem->buf = NULL;
}

static void
gsf_shared_memory_class_init (GObjectClass *gobject_class)
{
	gobject_class->finalize = gsf_shared_memory_finalize;
}

GSF_CLASS (GsfSharedMemory, gsf_shared_memory,
	   gsf_shared_memory_class_init, gsf_shared_memory_init,
	   G_TYPE_OBJECT)
