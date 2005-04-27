/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-docprop-vector.c: A type implementing OLE Document Property vectors using a GValueArray
 *
 * Copyright (C) 2004-2005 Frank Chiulli (fc-linux@cox.net)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf-docprop-vector.h>
#include <stdio.h>

struct _GsfDocPropVector {
	GObject      parent;

	GValueArray *gva;
};
typedef GObjectClass  GsfDocPropVectorClass;

#define GSF_DOCPROP_VECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GSF_DOCPROP_VECTOR_TYPE, GsfDocPropVectorClass))
#define GSF_DOCPROP_VECTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GSF_DOCPROP_VECTOR_TYPE, GsfDocPropVectorClass))
#define IS_GSF_DOCPROP_VECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GSF_DOCPROP_VECTOR_TYPE))

/*
 * Local function prototypes.
 */
static void     gsf_docprop_vector_init (GsfDocPropVector *vector);
static void     gsf_docprop_vector_value_dup (GValue const *src_value,
					      GValue *dest_value);
static void     gsf_docprop_vector_value_free (GValue *value);
static void     gsf_docprop_vector_value_init (GValue *value);
static gpointer gsf_docprop_vector_value_peek_pointer (GValue const *value);

GValueArray *
gsf_value_get_docprop_varray (GValue const *value)
{
	GsfDocPropVector *v = gsf_value_get_docprop_vector (value);
	return v ? v->gva : NULL;
}

/**
 * gsf_docprop_value_get_vector
 * @value: A GValue of type #GsfDocPropVector.
 *
 * This function returns a pointer to the GsfDocPropVector structure in @value.
 * No additional references are created.
 *
 * Returns: A pointer to the #GsfDocPropVector structure in @value
 **/
GsfDocPropVector *
gsf_value_get_docprop_vector (GValue const *value)
{
	g_return_val_if_fail (VAL_IS_GSF_DOCPROP_VECTOR (value), NULL);
	
	return (GsfDocPropVector *) g_value_get_object (value);
}

/**
 * gsf_docprop_vector_append
 * @vector: The vector to which the GValue will be added
 * @value:  The GValue to add to @vector
 *
 * Insert a copy of @value as the last element of @vector.
 **/
void
gsf_docprop_vector_append (GsfDocPropVector *vector, GValue *value)
{
	g_return_if_fail (vector != NULL);
	g_return_if_fail (value != NULL);

	if (G_IS_VALUE (value))
		vector->gva = g_value_array_append (vector->gva, value);
}

/**
 * gsf_docprop_vector_as_string
 * @vector: The #GsfDocPropVector from which GValues will be extracted.
 *
 * This function returns a string which represents all the GValues in @vector.
 * The caller is responsible for freeing the result.
 *
 * Returns: a string of comma-separated values
 **/
gchar*
gsf_docprop_vector_as_string (GsfDocPropVector *vector)
{
	gchar		*rstring;

	guint		 i;
	guint		 num_values;

	g_return_val_if_fail (vector != NULL, NULL);
	g_return_val_if_fail (vector->gva != NULL, NULL);

	rstring    = g_new0 (gchar, 1);
	num_values = vector->gva->n_values;
	
	for (i = 0; i < num_values; i++) {
		char    *str;
		GValue	*v;
		
		v = g_value_array_get_nth (vector->gva, i);
		str = g_strdup_value_contents (v);
		rstring = g_strconcat (rstring, str, ",", NULL);
		g_free (str);
		g_value_unset (v);
	}

	return rstring;
}

GType
gsf_docprop_vector_get_type (void)
{
	static GTypeValueTable const value_info = {
		&gsf_docprop_vector_value_init,		/* value initialization function */
		&gsf_docprop_vector_value_free,		/* value free function */
		&gsf_docprop_vector_value_dup,		/* value copy function */
		&gsf_docprop_vector_value_peek_pointer,	/* value peek function */
		NULL,					/* collect format */
		NULL,					/* collect value function */
		NULL,					/* lcopy format */
		NULL					/* lcopy value function */
	};

	/*
	 * GTypeInfo
	 * Do this manually so that we can specify a value_info field.
	 */
	static GTypeInfo const type_info = {
		sizeof (GsfDocPropVectorClass),			/* Class size */
		(GBaseInitFunc) NULL,				/* Base initialization function */
		(GBaseFinalizeFunc) NULL,			/* Base finalization function */
		(GClassInitFunc) NULL,				/* Class initialization function */
		(GClassFinalizeFunc) NULL,			/* Class finalization function */
		NULL,						/* Class data */
		sizeof (GsfDocPropVector),			/* Instance size */
		0,						/* Number of pre-allocated instances */
		(GInstanceInitFunc) gsf_docprop_vector_init,	/* Location of the instance initialization function */
		&value_info					/* A GTypeValueTable function table for
								   generic handling of GValues of this
								   type */
	};
	static GType my_type = 0;

	if (my_type == 0) {
		my_type = g_type_register_static (
				G_TYPE_OBJECT,			/* Parent type */
				"GsfDocPropVector",		/* Type name */
				&type_info,			/* GTypeInfo structure */
				0);				/* GTypeFlags */
	}

	return my_type;
}

static void
gsf_docprop_vector_init (GsfDocPropVector *vector)
{
	vector->gva = g_value_array_new (0);
}

/**
 * gsf_docprop_vector_new
 *
 * This function creates a new gsf_docprop_vector object.
 *
 * Returns: GsfDocPropVector*
 **/
GsfDocPropVector*
gsf_docprop_vector_new (void)
{
	return g_object_new (GSF_DOCPROP_VECTOR_TYPE, NULL);
}

/**
 * gsf_docprop_vector_value_dup
 * @src_value:  A properly setup GValue of same or derived type.
 * @dest_value: A GValue with zero-filled data sectione 
 *
 * The purpose of this function is to duplicate/copy the contents of @src_value into
 * @dest_value in a way, that even after src_value has been freed, the contents
 * of @dest_value remain valid.
 **/
static void
gsf_docprop_vector_value_dup (const GValue *src_value, GValue *dest_value)
{
	GsfDocPropVector	*vector;
	GsfDocPropVector	*new_vector;

	vector = (GsfDocPropVector *)src_value->data[0].v_pointer;

	new_vector = gsf_docprop_vector_new ();
	dest_value->data[0].v_pointer = new_vector;

	g_value_array_free (new_vector->gva);
	new_vector->gva = g_value_array_copy (vector->gva);
}

/**
 * gsf_docprop_vector_value_free
 * @value: The GValue to be free'd.
 *
 * This function frees any old contents that might be left in the data array of
 * the passed in @value.  No resources may remain allocated through the GValue
 * contents after this function returns.
 **/
static void
gsf_docprop_vector_value_free (GValue *value)
{
	GValueArray *gva = gsf_value_get_docprop_varray (value);
	if (gva != NULL)
		g_value_array_free (gva);
}

/**
 * gsf_docprop_vector_value_init
 * @value: The GValue to be initialized
 * 
 * Initialize @value's contents by poking values directly into the @value->data
 * array.
 **/
static void
gsf_docprop_vector_value_init (GValue *value)
{
	value->data[0].v_pointer = gsf_docprop_vector_new ();
}

/**
 * gsf_docprop_vector_value_peek_pointer
 * @value: A valid GsfDocPropVector GValue 
 *
 * If the value contents fit into a pointer, such as objects or strings,
 * return this pointer, so the caller can peek at the current contents.
 *
 * Returns: a gpointer to the GsfDocPropVector structure
 **/
static gpointer
gsf_docprop_vector_value_peek_pointer (const GValue *value)
{
	return value->data[0].v_pointer;
}
