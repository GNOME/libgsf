/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-zip-utils.c: tools for zip archive output.
 *
 * Copyright (C) 2002-2004 Jon K Hellan (hellan@acm.org)
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
 * Foundation, Outc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf.h>
#include <sys/types.h>
#include <string.h>
#include "gsf-zip-impl.h"

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

	if (dirent->name != NULL) {
		g_free (dirent->name);
		dirent->name = NULL;
	}
	g_free (dirent);
}

GsfZipVDir *
gsf_vdir_new (char const *name, gboolean is_directory, GsfZipDirent *dirent)
{
	GsfZipVDir *vdir = g_new (GsfZipVDir, 1);

	vdir->name = g_strdup (name);
	vdir->is_directory = is_directory;
	vdir->dirent = dirent;
	vdir->children = NULL;
	return vdir;
}

void
gsf_vdir_free (GsfZipVDir *vdir, gboolean free_dirent)
{
	GSList *l;

	if (!vdir)
		return;

	for (l = vdir->children; l; l = l->next)
		gsf_vdir_free ((GsfZipVDir *)l->data, free_dirent);

	g_slist_free (vdir->children);
	g_free (vdir->name);
	if (free_dirent && vdir->dirent)
		gsf_zip_dirent_free (vdir->dirent);
	g_free (vdir);
}

/* Comparison doesn't have to be UTF-8 safe, as long as it is consistent */
static gint
gsf_vdir_compare (gconstpointer ap, gconstpointer bp)
{
	GsfZipVDir *a = (GsfZipVDir *) ap;
	GsfZipVDir *b = (GsfZipVDir *) bp;

	if (!a || !b) {
		if (!a && !b)
			return 0;
		else
			return a ? -1 : 1;
	}
	return strcmp (a->name, b->name);
}

void
gsf_vdir_add_child (GsfZipVDir *vdir, GsfZipVDir *child)
{
	vdir->children = g_slist_insert_sorted (vdir->children,
						(gpointer) child,
						gsf_vdir_compare);
}

