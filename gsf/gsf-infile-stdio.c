/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-stdio.c: read a directory tree
 *
 * Copyright (C) 2004-2006 Novell, Inc.
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
#include <gsf/gsf-infile.h>
#include <gsf/gsf.h>

#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

static GObjectClass *parent_class;

struct _GsfInfileStdio {
	GsfInfile parent;
	char *root;
	GPtrArray *children;
};

typedef GsfInfileClass GsfInfileStdioClass;

static void
gsf_infile_stdio_finalize (GObject *obj)
{
	GsfInfileStdio *ifs = GSF_INFILE_STDIO (obj);

	g_free (ifs->root);
	g_ptr_array_free (ifs->children, TRUE);
	parent_class->finalize (obj);
}

static GsfInput *
gsf_infile_stdio_dup (GsfInput *src_input, G_GNUC_UNUSED GError **err)
{
	GsfInfileStdio *src = GSF_INFILE_STDIO (src_input);
	unsigned ui;

	GsfInfileStdio *dst = g_object_new (GSF_INFILE_STDIO_TYPE, NULL);
	dst->root = g_strdup (src->root);

	for (ui = 0; ui < src->children->len; ui++) {
		const char *child = g_ptr_array_index (src->children, ui);
		g_ptr_array_add (dst->children, g_strdup (child));
	}

	return GSF_INPUT (dst);
}

static guint8 const *
gsf_infile_stdio_read (G_GNUC_UNUSED GsfInput *input, G_GNUC_UNUSED size_t num_bytes,
		       G_GNUC_UNUSED guint8 *buffer)
{
	return NULL;
}

static GsfInput *
open_child (GsfInfileStdio *ifs, char const *name, GError **err)
{
	GsfInput *child;
	char *path = g_build_filename (ifs->root, name, NULL);

	if (g_file_test (path, G_FILE_TEST_IS_DIR))
		child = (GsfInput *) gsf_infile_stdio_new (path, err);
	else
		child = gsf_input_stdio_new (path, err);
	g_free (path);

	return child;
}

static char const *
gsf_infile_stdio_name_by_index (GsfInfile *infile, int target)
{
	GsfInfileStdio *ifs = GSF_INFILE_STDIO (infile);
	return (unsigned)target < ifs->children->len
		? g_ptr_array_index (ifs->children, target)
		: NULL;
}

static GsfInput *
gsf_infile_stdio_child_by_index (GsfInfile *infile, int target, GError **err)
{
	GsfInfileStdio *ifs = GSF_INFILE_STDIO (infile);
	const char *name = gsf_infile_stdio_name_by_index (infile, target);
	return name ? open_child (ifs, name, err) : NULL;
}

static GsfInput *
gsf_infile_stdio_child_by_name (GsfInfile *infile, char const *name, GError **err)
{
	GsfInfileStdio *ifs = GSF_INFILE_STDIO (infile);
	unsigned ui;

	for (ui = 0; ui < ifs->children->len; ui++) {
		const char *child = g_ptr_array_index (ifs->children, ui);
		if (!strcmp (child, name))
			return open_child (ifs, name, err);
	}

	return NULL;
}

static int
gsf_infile_stdio_num_children (GsfInfile *infile)
{
	GsfInfileStdio *ifs = GSF_INFILE_STDIO (infile);
	return ifs->children->len;
}

static void
gsf_infile_stdio_init (GsfInfileStdio *ifs)
{
	ifs->root = NULL;
	ifs->children = g_ptr_array_new_with_free_func (g_free);
}

static void
gsf_infile_stdio_class_init (GObjectClass *gobject_class)
{
	GsfInputClass  *input_class  = GSF_INPUT_CLASS (gobject_class);
	GsfInfileClass *infile_class = GSF_INFILE_CLASS (gobject_class);

	parent_class = g_type_class_peek (GSF_INFILE_TYPE);

	gobject_class->finalize		= gsf_infile_stdio_finalize;
	input_class->Dup		= gsf_infile_stdio_dup;
	input_class->Read		= gsf_infile_stdio_read;
	input_class->Seek		= NULL;
	infile_class->num_children	= gsf_infile_stdio_num_children;
	infile_class->name_by_index	= gsf_infile_stdio_name_by_index;
	infile_class->child_by_index	= gsf_infile_stdio_child_by_index;
	infile_class->child_by_name	= gsf_infile_stdio_child_by_name;
}

GSF_CLASS (GsfInfileStdio, gsf_infile_stdio,
	   gsf_infile_stdio_class_init, gsf_infile_stdio_init,
	   GSF_INFILE_TYPE)

/**
 * gsf_infile_stdio_new:
 * @root: in locale dependent encoding
 * @err: optionally %NULL.
 *
 * Returns: a new file or %NULL.
 **/
GsfInfile *
gsf_infile_stdio_new (char const *root, GError **err)
{
	GsfInfileStdio *ifs;
	GDir *dir;
	char const *child;

	dir = g_dir_open (root, 0, err);
	if (dir == NULL)
		return NULL;

	ifs = g_object_new (GSF_INFILE_STDIO_TYPE, NULL);
	ifs->root = g_strdup (root);

	while ((child = g_dir_read_name (dir)))
		g_ptr_array_add (ifs->children, g_strdup (child));

	g_dir_close (dir);

	gsf_input_set_name_from_filename (GSF_INPUT (ifs), root);

	return GSF_INFILE (ifs);
}
