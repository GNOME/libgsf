/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input-file.c: 
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
#include <gsf/gsf-input-gnomevfs.h>
#include <gsf/gsf-input-impl.h>
#include <gsf/gsf-impl-utils.h>

#include <libgnomevfs/gnome-vfs.h>

struct _GsfInputGnomeVFS {
	GsfInput input;

	GnomeVFSHandle *handle;
}

static guint8 const *
gsf_input_gnomevfs_get_data (GsfInput *input,
			     int       num_bytes)
{
	return NULL;
}

GsfInput *
gsf_input_gnomevfs_new (char const *uri, GError **error)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult res = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
	
	if (res != GNOME_VFS_OK) {
		g_set_error (error, gsf_input_error (), res,
			gnome_vfs_result_to_string (res));
		return NULL;
	}
}
