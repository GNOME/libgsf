/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-gnomevfs.c: gnomevfs based output
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
#include <gsf/gsf-output-gnomevfs.h>
#include <gsf/gsf-output-impl.h>
#include <gsf/gsf-impl-utils.h>

#include <libgnomevfs/gnome-vfs.h>

struct _GsfOutputGnomeVFS {
    GsfOutput output;

    GnomeVFSHandle *handle;
};

typedef struct {
    GsfOutputClass output_class;
} GsfOutputGnomeVFSClass;

/**
* gsf_output_gnomevfs_new :
 * @filename : in utf8.
 * @err	     : optionally NULL.
 *
 * Returns a new file or NULL.
 **/
GsfOutput *
gsf_output_gnomevfs_new (char const *filename, GError **err)
{
    GsfOutputGnomeVFS *output;
    GnomeVFSHandle *handle;
    GnomeVFSResult res;

    if (filename == NULL) {
        g_set_error (err, gsf_output_error (), 0,
                     "Filename/URI cannot be NULL");
        return NULL;
    } else
       res = gnome_vfs_open (&handle, filename, GNOME_VFS_OPEN_WRITE);

    if (res != GNOME_VFS_OK) {
        g_set_error (err, gsf_output_error (), (gint) res,
                     gnome_vfs_result_to_string (res));
        return NULL;
    }

    output = g_object_new (GSF_OUTPUT_GNOMEVFS_TYPE, NULL);
    output->handle = handle;

    return GSF_OUTPUT (output);
}

static gboolean
gsf_output_gnomevfs_close (GsfOutput *output)
{
    GsfOutputGnomeVFS *vfs = GSF_OUTPUT_GNOMEVFS (output);
    gboolean res = FALSE;

    if (vfs->handle != NULL) {
        res = (GNOME_VFS_OK == gnome_vfs_close (vfs->handle));
        vfs->handle = NULL;
    }

    return res;
}

static void
gsf_output_gnomevfs_finalize (GObject *obj)
{
    GObjectClass *parent_class;
    GsfOutput *output = (GsfOutput *)obj;

    gsf_output_gnomevfs_close (output);

    parent_class = g_type_class_peek (GSF_OUTPUT_TYPE);
    if (parent_class && parent_class->finalize)
        parent_class->finalize (obj);
}

static gboolean
gsf_output_gnomevfs_seek (GsfOutput *output, gsf_off_t offset,
			  GsfSeekType whence)
{
    GsfOutputGnomeVFS const *vfs = GSF_OUTPUT_GNOMEVFS (output);

    if (vfs->handle == NULL)
        return TRUE;

    switch (whence) {
        case GSF_SEEK_SET :
            if (GNOME_VFS_OK != gnome_vfs_seek (vfs->handle,
						GNOME_VFS_SEEK_START,
						(GnomeVFSFileOffset) offset))
                return FALSE;
            break;
        case GSF_SEEK_CUR :
            if (GNOME_VFS_OK != gnome_vfs_seek (vfs->handle,
						GNOME_VFS_SEEK_CURRENT,
						(GnomeVFSFileOffset) offset))
                return FALSE;
            break;
        case GSF_SEEK_END :
            if (GNOME_VFS_OK != gnome_vfs_seek (vfs->handle,
						GNOME_VFS_SEEK_END,
						(GnomeVFSFileOffset) offset))
                return FALSE;
            break;
    }

    return TRUE;
}

static gboolean
gsf_output_gnomevfs_write (GsfOutput *output,
			   size_t num_bytes,
			   guint8 const *buffer)
{
    GsfOutputGnomeVFS *vfs = GSF_OUTPUT_GNOMEVFS (output);
    GnomeVFSFileSize nwritten;
    GnomeVFSResult res;

    g_return_val_if_fail (vfs != NULL, FALSE);
    g_return_val_if_fail (vfs->handle != NULL, FALSE);

    res = gnome_vfs_write (vfs->handle, (gconstpointer)buffer,
			   (GnomeVFSFileSize) num_bytes, &nwritten);
    if (GNOME_VFS_OK != res)
        return FALSE;
    return nwritten == num_bytes;
}

static void
gsf_output_gnomevfs_init (GObject *obj)
{
    GsfOutputGnomeVFS *vfs = GSF_OUTPUT_GNOMEVFS (obj);

    vfs->handle = NULL;
}

static void
gsf_output_gnomevfs_class_init (GObjectClass *gobject_class)
{
    GsfOutputClass *output_class = GSF_OUTPUT_CLASS (gobject_class);

    gobject_class->finalize = gsf_output_gnomevfs_finalize;
    output_class->Close	= gsf_output_gnomevfs_close;
    output_class->Seek	= gsf_output_gnomevfs_seek;
    output_class->Write	= gsf_output_gnomevfs_write;
}

GSF_CLASS (GsfOutputGnomeVFS, gsf_output_gnomevfs,
           gsf_output_gnomevfs_class_init, gsf_output_gnomevfs_init, GSF_OUTPUT_TYPE)
