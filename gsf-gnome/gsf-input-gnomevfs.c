/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input-gnomevfs.c: 
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
#include <gsf-gnome/gsf-input-gnomevfs.h>
#include <gsf/gsf-input-impl.h>
#include <gsf/gsf-impl-utils.h>

#include <libgnomevfs/gnome-vfs.h>

struct _GsfInputGnomeVFS {
	GsfInput input;

	GnomeVFSHandle *handle;
        guint8   *buf;
        size_t   buf_size;
};

typedef struct {
    GsfInputClass input_class;
} GsfInputGnomeVFSClass;

/**
* gsf_input_gnomevfs_new :
 * @uri : uri you wish to open.
 * @err	     : optionally NULL.
 *
 * Returns a new file or NULL.
 **/
GsfInput *
gsf_input_gnomevfs_new (char const *uri, GError **error)
{
        GsfInputGnomeVFS *input;
	GnomeVFSHandle *handle;
        GnomeVFSFileInfo info;
        gsf_off_t size;
	GnomeVFSResult res = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
	
	if (res != GNOME_VFS_OK) {
		g_set_error (error, gsf_input_error (), (gint) res,
			gnome_vfs_result_to_string (res));
		return NULL;
	}

        res = gnome_vfs_get_file_info_from_handle (handle, &info, GNOME_VFS_FILE_INFO_DEFAULT);
        if (res != GNOME_VFS_OK) {
            g_set_error (error, gsf_input_error (), (gint) res,
                         gnome_vfs_result_to_string (res));
            return NULL;
        }        
        
        if (info.type != GNOME_VFS_FILE_TYPE_REGULAR) {
            g_set_error (error, gsf_input_error (), 0,
                         "%s: Is not a regular file", uri);
            gnome_vfs_close (handle);
            return NULL;
        }

        size = (gsf_off_t) info.size ;
        input = g_object_new (GSF_INPUT_GNOMEVFS_TYPE, NULL);
        input->handle = handle;
        input->buf  = NULL;
        input->buf_size = 0;
        gsf_input_set_size (GSF_INPUT (input), size);
        gsf_input_set_name (GSF_INPUT (input), uri);

        return GSF_INPUT (input);
}

static void
gsf_input_gnomevfs_finalize (GObject *obj)
{
    GObjectClass *parent_class;
    GsfInputGnomeVFS *input = (GsfInputGnomeVFS *)obj;

    if (input->handle != NULL) {
        gnome_vfs_close (input->handle);
        input->handle = NULL;
    }

    if (input->buf != NULL) {
        g_free (input->buf);
        input->buf  = NULL;
        input->buf_size = 0;
    }
    
    parent_class = g_type_class_peek (GSF_INPUT_TYPE);
    if (parent_class && parent_class->finalize)
        parent_class->finalize (obj);
}

static GsfInput *
gsf_input_gnomevfs_dup (GsfInput *src_input, GError **err)
{
    GsfInputGnomeVFS const *src = (GsfInputGnomeVFS *)src_input;
    return gsf_input_gnomevfs_new (src->input.name, err);
}

static guint8 const *
gsf_input_gnomevfs_read (GsfInput *input, size_t num_bytes,
                         guint8 *buffer)
{
    GsfInputGnomeVFS *vfs = GSF_INPUT_GNOMEVFS (input);
    GnomeVFSResult res;
    GnomeVFSFileSize nread = 0;

    g_return_val_if_fail (vfs != NULL, NULL);
    g_return_val_if_fail (vfs->handle != NULL, NULL);

    if (buffer == NULL) {
        if (vfs->buf_size < num_bytes) {
            vfs->buf_size = num_bytes;
            if (vfs->buf != NULL)
                g_free (vfs->buf);
            vfs->buf = g_malloc (vfs->buf_size);
        }
        buffer = vfs->buf;
    }

    res = gnome_vfs_read (vfs->handle, (gpointer)buffer,
			  (GnomeVFSFileSize) num_bytes, &nread);

    if (res != GNOME_VFS_OK)
        return NULL;
    
    if (nread < num_bytes)
        return NULL;

    return buffer;
}

static gboolean
gsf_input_gnomevfs_seek (GsfInput *input, gsf_off_t offset, GSeekType whence)
{
    GsfInputGnomeVFS const *vfs     = GSF_INPUT_GNOMEVFS (input);
    GnomeVFSSeekPosition vfs_whence = 0; /* make compiler shut up */    

    if (vfs->handle == NULL)
        return TRUE;

    switch (whence) {
    case G_SEEK_SET : vfs_whence = GNOME_VFS_SEEK_START;	break;
    case G_SEEK_CUR : vfs_whence = GNOME_VFS_SEEK_CURRENT;	break;
    case G_SEEK_END : vfs_whence = GNOME_VFS_SEEK_END;	break;
    default :
	    break;
    }

    if (GNOME_VFS_OK != gnome_vfs_seek (vfs->handle,vfs_whence,
					(GnomeVFSFileOffset) offset))
	    return FALSE;
    return TRUE;
}

static void
gsf_input_gnomevfs_init (GObject *obj)
{
    GsfInputGnomeVFS *vfs = GSF_INPUT_GNOMEVFS (obj);

    vfs->handle   = NULL;
    vfs->buf      = NULL;
    vfs->buf_size = 0;
}

static void
gsf_input_gnomevfs_class_init (GObjectClass *gobject_class)
{
    GsfInputClass *input_class = GSF_INPUT_CLASS (gobject_class);

    gobject_class->finalize = gsf_input_gnomevfs_finalize;
    input_class->Dup	    = gsf_input_gnomevfs_dup;
    input_class->Read	    = gsf_input_gnomevfs_read;
    input_class->Seek	    = gsf_input_gnomevfs_seek;
}

GSF_CLASS (GsfInputGnomeVFS, gsf_input_gnomevfs,
           gsf_input_gnomevfs_class_init, gsf_input_gnomevfs_init, GSF_INPUT_TYPE)
