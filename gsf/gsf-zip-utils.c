/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-zip-utils.c: tools for zip archive output.
 *
 * Copyright (C) 2002-2006 Jon K Hellan (hellan@acm.org)
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
 * Foundation, Outc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf-zip-impl.h>
#include <gsf/gsf.h>

#include <sys/types.h>
#include <string.h>

/**
 * SECTION:zip
 * @Short_description: Utilities for reading and writing ZIP/JAR files
 * @Title: Zip files
 *
 * #GsfInfile and #GsfOutfile support for zip files.
 **/

/* Doesn't do much, but include for symmetry */
GsfZipDirent*
gsf_zip_dirent_new (void)
{
	return g_new0 (GsfZipDirent, 1);
}

void
gsf_zip_dirent_free (GsfZipDirent *dirent)
{
	g_return_if_fail (dirent != NULL);

	g_free (dirent->name);
	dirent->name = NULL;

	g_free (dirent);
}

static GsfZipDirent *
gsf_zip_dirent_copy (GsfZipDirent *dirent)
{
	GsfZipDirent *res = g_new0 (GsfZipDirent, 1);
	memcpy (res, dirent, sizeof (GsfZipDirent));
	if (dirent->name)
		res->name = g_strdup (dirent->name);
	return res;
}

GType
gsf_zip_dirent_get_type (void)
{
    static GType type = 0;

    if (type == 0)
	type = g_boxed_type_register_static
	    ("GsfZipDirent",
	     (GBoxedCopyFunc) gsf_zip_dirent_copy,
	     (GBoxedFreeFunc) gsf_zip_dirent_free);

    return type;
}

/**
 * gsf_zip_vdir_new:
 * @name:
 * @is_directory:
 * @dirent:
 *
 * Since: 1.14.24
 *
 * Returns: the newly created #GsfZipVDir.
 */
GsfZipVDir *
gsf_zip_vdir_new (char const *name, gboolean is_directory, GsfZipDirent *dirent)
{
	GsfZipVDir *vdir = g_new (GsfZipVDir, 1);

	vdir->name = g_strdup (name);
	vdir->is_directory = is_directory;
	vdir->dirent = dirent;
	vdir->children = g_ptr_array_new ();
	return vdir;
}

/**
 * gsf_vdir_new: (skip)
 * @name:
 * @is_directory:
 * @dirent:
 *
 * Deprecated: 1.14.24
 * Returns: the newly created #GsfZipVDir.
 */
GsfZipVDir *
gsf_vdir_new (char const *name, gboolean is_directory, GsfZipDirent *dirent)
{
	return gsf_zip_vdir_new (name, is_directory, dirent);
}

void
gsf_vdir_free (GsfZipVDir *vdir, gboolean free_dirent)
{
	gsf_zip_vdir_free (vdir, free_dirent);
}

void
gsf_zip_vdir_free (GsfZipVDir *vdir, gboolean free_dirent)
{
	unsigned ui;

	if (!vdir)
		return;

	for (ui = 0; ui < vdir->children->len; ui++) {
		GsfZipVDir *c = g_ptr_array_index (vdir->children, ui);
		gsf_zip_vdir_free (c, free_dirent);
	}
	g_ptr_array_free (vdir->children, TRUE);

	g_free (vdir->name);
	if (free_dirent && vdir->dirent)
		gsf_zip_dirent_free (vdir->dirent);
	g_free (vdir);
}

static GsfZipVDir *
gsf_zip_vdir_copy (GsfZipVDir *vdir)
{
	GsfZipVDir *res = g_new0 (GsfZipVDir, 1);
	unsigned ui;

	/* it is not possible to add a ref_count without breaking the API,
	 * so we need to really copy everything */
	if (vdir->name)
		res->name = g_strdup (vdir->name);
	res->is_directory = vdir->is_directory;
	if (vdir->dirent)
		res->dirent = gsf_zip_dirent_copy (vdir->dirent);
	for (ui = 0; ui < vdir->children->len; ui++) {
		GsfZipVDir *c = g_ptr_array_index (vdir->children, ui);
		gsf_zip_vdir_add_child (res, gsf_zip_vdir_copy (c));
	}
	return res;
}

static void
gsf_zip_vdir_destroy (GsfZipVDir *vdir)
{
	gsf_zip_vdir_free (vdir, TRUE);
}

GType
gsf_zip_vdir_get_type (void)
{
    static GType type = 0;

    if (type == 0)
	type = g_boxed_type_register_static
	    ("GsfZipVDir",
	     (GBoxedCopyFunc) gsf_zip_vdir_copy,
	     (GBoxedFreeFunc) gsf_zip_vdir_destroy);

    return type;
}

void
gsf_vdir_add_child (GsfZipVDir *vdir, GsfZipVDir *child)
{
	gsf_zip_vdir_add_child(vdir, child);
}

void
gsf_zip_vdir_add_child (GsfZipVDir *vdir, GsfZipVDir *child)
{
	g_ptr_array_add (vdir->children, child);
}
