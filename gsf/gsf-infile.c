/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile.c :
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
#include <gsf/gsf-infile-impl.h>
#include <gsf/gsf-impl-utils.h>

#include <stdarg.h>

#define GET_CLASS(instance) G_TYPE_INSTANCE_GET_CLASS (instance, GSF_INFILE_TYPE, GsfInfileClass)

/**
 * gsf_infile_num_children :
 * @infile : the structured storage
 *
 * Returns the number of children the storage has, or -1 if the storage can not
 * 	have children.
 **/
int
gsf_infile_num_children (GsfInfile *infile)
{
	g_return_val_if_fail (infile != NULL, FALSE);

	return GET_CLASS (infile)->num_children (infile);
}

/**
 * gsf_infile_name_by_index :
 * @infile :
 * @i      :
 *
 * Returns the utf8 encoded name of the @i-th child
 * NOTE : DO NOT FREE THE STRING
 **/
char const *
gsf_infile_name_by_index (GsfInfile *infile, int i)
{
	g_return_val_if_fail (infile != NULL, FALSE);

	return GET_CLASS (infile)->name_by_index (infile, i);
}

/**
 * gsf_infile_child_by_index :
 * @infile :
 * @i :
 *
 * Returns a newly created child which must be unrefed.
 **/
GsfInput *
gsf_infile_child_by_index (GsfInfile *infile, int i)
{
	g_return_val_if_fail (infile != NULL, FALSE);

	return GET_CLASS (infile)->child_by_index (infile, i);
}

/**
 * gsf_infile_child_by_name :
 * @infile :
 * @name :
 *
 * Returns a newly created child which must be unrefed.
 **/
GsfInput *
gsf_infile_child_by_name (GsfInfile *infile, char const *name)
{
	g_return_val_if_fail (infile != NULL, FALSE);

	return GET_CLASS (infile)->child_by_name (infile, name);
}

/**
 * gsf_infile_child_by_vname :
 * @infile :
 * @name : A NULL terminated list of names
 *
 * A returns a newly created child which must be unrefed.
 **/
GsfInput *
gsf_infile_child_by_vname (GsfInfile *infile, char const *name, ...)
{
	va_list   ap;
	GsfInput  *child = GSF_INPUT (infile);
	GsfInfile *tmp = NULL;

	g_return_val_if_fail (GSF_IS_INFILE (infile), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	va_start (ap, name);
	while (1) {
		child = gsf_infile_child_by_name (infile, name);

		g_return_val_if_fail (child != NULL, NULL);

		name = va_arg (ap, char *);
		if (tmp != NULL)
			g_object_unref (G_OBJECT (tmp));
		if (name == NULL)
			break;
		g_return_val_if_fail (GSF_IS_INFILE (child), NULL);

		infile = tmp = GSF_INFILE (child);
	}
	va_end (ap);

	return child;
}

GSF_CLASS_ABSTRACT (GsfInfile, gsf_infile, NULL, NULL, GSF_INPUT_TYPE)
