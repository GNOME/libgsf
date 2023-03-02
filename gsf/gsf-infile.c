/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile.c :
 *
 * Copyright (C) 2002-2006 Jody Goldberg (jody@gnome.org)
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

#include <stdarg.h>

#define GET_CLASS(instance) G_TYPE_INSTANCE_GET_CLASS (instance, GSF_INFILE_TYPE, GsfInfileClass)

/**
 * gsf_infile_num_children:
 * @infile: the structured storage
 *
 * Returns: the number of children the storage has, or -1 if the storage can not
 * 	have children.
 **/
int
gsf_infile_num_children (GsfInfile *infile)
{
	g_return_val_if_fail (infile != NULL, -1);

	return GET_CLASS (infile)->num_children (infile);
}

/**
 * gsf_infile_name_by_index:
 * @infile: An #GsfInfile
 * @i: zero-based index of child to find.
 *
 * Returns: (transfer none) (nullable): the utf8 encoded name of the @i-th child
 **/
const gchar *
gsf_infile_name_by_index (GsfInfile *infile, int i)
{
	g_return_val_if_fail (infile != NULL, NULL);

	return GET_CLASS (infile)->name_by_index (infile, i);
}

/**
 * gsf_infile_child_by_index:
 * @infile: #GsfInfile
 * @i: target index
 *
 * TODO : For 2.0 api will change to include a #GError.
 * Returns: (transfer full): a newly created child which must be unrefed.
 **/
GsfInput *
gsf_infile_child_by_index (GsfInfile *infile, int i)
{
	GError *err = NULL;
	GsfInput *res;

	g_return_val_if_fail (GSF_INFILE (infile) != NULL, NULL);

	res = GET_CLASS (infile)->child_by_index (infile, i, &err);

	if (err != NULL) {
		char const *iname = gsf_input_name (GSF_INPUT (infile));
		g_warning ("Unable to get child[%d] for infile '%s' because : %s",
			   i, iname ? iname : "?", err->message);
		g_error_free (err);
		g_return_val_if_fail (res == NULL, NULL); /* be anal */
	}

	return res;
}

/**
 * gsf_infile_child_by_name:
 * @infile: #GsfInfile
 * @name: target name
 *
 * The function returns a named child of the given infile.  This only
 * works for an immediate child.  If you need to go several levels
 * down use gsf_infile_child_by_aname, for example.
 *
 * TODO : For 2.0 api will change to include a #GError.
 *
 * Returns: (transfer full): a newly created child which must be
 * unrefed.
 **/
GsfInput *
gsf_infile_child_by_name (GsfInfile *infile, char const *name)
{
	GError *err = NULL;
	GsfInput *res;

	g_return_val_if_fail (GSF_INFILE (infile) != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	res = GET_CLASS (infile)->child_by_name (infile, name, &err);

	if (err != NULL) {
		char const *iname = gsf_input_name (GSF_INPUT (infile));
		g_warning ("Unable to get child['%s'] for infile '%s' because : %s",
			   name, iname ? iname : "?", err->message);
		g_error_free (err);
		g_return_val_if_fail (res == NULL, NULL); /* be anal */
	}

	return res;
}

/**
 * gsf_infile_child_by_vname:
 * @infile: a #GsfInfile
 * @...: A %NULL terminated list of names
 *
 * Apart from the way arguments are specified, this is the same as
 * gsf_infile_child_by_aname.  Please see the documentation there.
 *
 * Returns: (transfer full): a newly created child which must be unrefed.
 **/
GsfInput *
gsf_infile_child_by_vname (GsfInfile *infile,  ...)
{
	GsfInput *res;
	va_list   names;

	va_start (names, infile);
	res = gsf_infile_child_by_vaname (infile, names);
	va_end (names);

	return res;
}

/**
 * gsf_infile_child_by_vaname:
 * @infile: #GsfInfile
 * @names: A %NULL terminated array of names (e.g. from g_strsplit)
 *
 * Apart from argument types, this is the same as gsf_infile_child_by_aname.
 * Please see the documentation there.
 * New in 1.14.9.
 *
 * Returns: (transfer full): a newly created child which must be unrefed.
 **/
GsfInput *
gsf_infile_child_by_vaname (GsfInfile *infile, va_list names)
{
	GsfInput  *child = GSF_INPUT (infile);
	GsfInfile *tmp = NULL;
	char const *name;

	g_return_val_if_fail (GSF_IS_INFILE (infile), NULL);

	while (NULL != (name = va_arg (names, char const *))) {
		child = gsf_infile_child_by_name (infile, name);
		if (tmp != NULL)
			g_object_unref (tmp);
		if (child == NULL)
			break;

		g_return_val_if_fail (GSF_IS_INFILE (child), NULL);

		infile = tmp = GSF_INFILE (child);
	}

	return child;
}

/**
 * gsf_infile_child_by_aname:
 * @infile: #GsfInfile
 * @names: (array zero-terminated=1): A %NULL terminated array of names (e.g. from g_strsplit)
 *
 * This function finds a child that is several directory levels down
 * the tree.  If, for example, the names "foo", "bar", and "baz" are
 * given, then this function first finds the "foo" directory in the
 * root infile, then locates "bar" within that directory, and finally
 * locates "baz" within that and returns the "baz" child.  In other
 * words, this function finds the "foo/bar/baz" child.
 *
 * New in 1.14.9.
 *
 * Returns: (transfer full): a newly created child which must be unrefed.
 **/
GsfInput *
gsf_infile_child_by_aname (GsfInfile *infile, char const *names[])
{
	GsfInput  *child = GSF_INPUT (infile);
	GsfInfile *tmp = NULL;

	g_return_val_if_fail (GSF_IS_INFILE (infile), NULL);
	g_return_val_if_fail (names != NULL, NULL);

	for (;*names ; names++) {
		child = gsf_infile_child_by_name (infile, *names);
		if (tmp != NULL)
			g_object_unref (tmp);
		if (child == NULL)
			break;

		g_return_val_if_fail (GSF_IS_INFILE (child), NULL);

		infile = tmp = GSF_INFILE (child);
	}

	return child;
}

GSF_CLASS_ABSTRACT (GsfInfile, gsf_infile, NULL, NULL, GSF_INPUT_TYPE)
