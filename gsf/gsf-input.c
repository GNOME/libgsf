/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input.c: interface for used by the ole layer to read raw data
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
#include <gsf/gsf-input-impl.h>
#include <gsf/gsf-input-gzip.h>
#include <gsf/gsf-impl-utils.h>
#include <string.h>

#define GET_CLASS(instance) G_TYPE_INSTANCE_GET_CLASS (instance, GSF_INPUT_TYPE, GsfInputClass)

static void
gsf_input_finalize (GObject *obj)
{
	GObjectClass *parent_class;

	GsfInput *input = GSF_INPUT (obj);
	if (input->name != NULL) {
		g_free (input->name);
		input->name = NULL;
	}
	if (input->container != NULL) {
		g_object_unref (G_OBJECT (input->container));
		input->container = NULL;
	}

	parent_class = g_type_class_peek (G_TYPE_OBJECT);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static void
gsf_input_init (GObject *obj)
{
	GsfInput *input = GSF_INPUT (obj);

	input->size = 0;
	input->cur_offset = 0;
	input->name = NULL;
	input->container = NULL;
}

static void
gsf_input_class_init (GObjectClass *gobject_class)
{
	gobject_class->finalize = gsf_input_finalize;
}

GSF_CLASS_ABSTRACT (GsfInput, gsf_input,
		    gsf_input_class_init, gsf_input_init,
		    G_TYPE_OBJECT)

/**
 * gsf_input_name :
 * @input : 
 *
 * Returns @input's name in utf8 form, DO NOT FREE THIS STRING
 **/
char const *
gsf_input_name (GsfInput *input)
{
	g_return_val_if_fail (GSF_IS_INPUT (input), NULL);
	return input->name;
}

/**
 * gsf_input_container :
 * @input : 
 *
 * Returns, but does not add a reference to @input's container.
 * Potentially NULL
 **/
GsfInfile *
gsf_input_container (GsfInput *input)
{
	g_return_val_if_fail (GSF_IS_INPUT (input), NULL);
	return input->container;
}

/**
 * gsf_input_dup :
 * @src : The input to duplicate
 * @err : optionally NULL
 *
 * Duplicates input @src leaving the new one at the same offset.
 *
 * Returns : the duplicate, or NULL on error
 **/
GsfInput *
gsf_input_dup (GsfInput *src, GError **err)
{
	GsfInput *dst;

	g_return_val_if_fail (src != NULL, NULL);

	dst = GET_CLASS (src)->Dup (src, err);
	if (dst != NULL) {
		dst->size = src->size;
		if (src->name != NULL)
			gsf_input_set_name (dst, src->name);
		dst->container = src->container;
		if (dst->container != NULL)
			g_object_ref (G_OBJECT (dst->container));
		gsf_input_seek (dst, (gsf_off_t)src->cur_offset, G_SEEK_SET);
	}
	return dst;
}

/**
 * gsf_input_size :
 * @input : The input
 *
 * Looks up and caches the number of bytes in the input
 *
 * Returns :  the size or -1 on error
 **/
gsf_off_t
gsf_input_size (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, -1);
	return input->size;
}

/**
 * gsf_input_eof :
 * @input : the input
 *
 * Are we at the end of the file ?
 *
 * Returns : TRUE if the input is at the eof.
 **/
gboolean
gsf_input_eof (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, FALSE);

	return input->cur_offset >= input->size;
}

/**
 * gsf_input_read :
 * @input :
 * @num_bytes :
 * @optional_buffer : If supplied copy the data into it
 *
 * Read at at least @num_bytes.  Does not change the current position if there
 * is an error.  Will only read if the entire amount can be read.  Invalidates
 * the buffer associated with previous calls to gsf_input_read.
 *
 * Returns : pointer to the buffer or NULL if there is an error or 0 bytes are
 * 	requested.
 **/
guint8 const *
gsf_input_read (GsfInput *input, size_t num_bytes, guint8 *optional_buffer)
{
	guint8 const *res;

	g_return_val_if_fail (input != NULL, NULL);

	if (num_bytes == 0 || (input->cur_offset + num_bytes) > input->size)
		return NULL;
	res = GET_CLASS (input)->Read (input, num_bytes, optional_buffer);
	if (res == NULL)
		return NULL;

	input->cur_offset += num_bytes;
	return res;

}

/**
 * gsf_input_remaining :
 * @input :
 *
 * Returns the number of bytes left in the file.
 **/
gsf_off_t
gsf_input_remaining (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, 0);

	return input->size - input->cur_offset;
}

/**
 * gsf_input_tell :
 * @input :
 *
 * Returns the current offset in the file.
 **/
gsf_off_t
gsf_input_tell (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, 0);

	return input->cur_offset;
}

/**
 * gsf_input_seek :
 * @input :
 * @offset :
 * @whence :
 *
 * Returns TRUE on error.
 **/
gboolean
gsf_input_seek (GsfInput *input, gsf_off_t offset, GSeekType whence)
{
	gsf_off_t pos = offset;

	g_return_val_if_fail (input != NULL, -1);

	switch (whence) {
	case G_SEEK_SET : break;
	case G_SEEK_CUR : pos += input->cur_offset;	break;
	case G_SEEK_END : pos += input->size;		break;
	default : return TRUE;
	}

	if (pos < 0 || pos > input->size)
		return TRUE;

	/*
	 * If we go nowhere, just return.  This in particular handles null
	 * seeks for streams with no seek method.
	 */
	if (pos == input->cur_offset)
		return FALSE;

	if (GET_CLASS (input)->Seek (input, offset, whence))
		return TRUE;
	input->cur_offset = pos;
	return FALSE;
}

/**
 * gsf_input_set_name :
 * @input :
 * @name :
 *
 * protected.
 *
 * Returns : TRUE if the assignment was ok.
 **/
gboolean
gsf_input_set_name (GsfInput *input, char const *name)
{
	char *buf;

	g_return_val_if_fail (input != NULL, FALSE);

	buf = g_strdup (name);
	if (input->name != NULL)
		g_free (input->name);
	input->name = buf;
	return TRUE;
}

/**
 * gsf_input_set_container :
 * @input :
 * @container :
 *
 * Returns : TRUE if the assignment was ok.
 */
gboolean
gsf_input_set_container (GsfInput *input, GsfInfile *container)
{
	g_return_val_if_fail (input != NULL, FALSE);

	if (container != NULL)
		g_object_ref (G_OBJECT (container));
	if (input->container != NULL)
		g_object_unref (G_OBJECT (input->container));
	input->container = container;
	return TRUE;
}

/**
 * gsf_input_set_size :
 * @input :
 * @size :
 *
 * Returns : TRUE if the assignment was ok.
 */
gboolean
gsf_input_set_size (GsfInput *input, gsf_off_t size)
{
	g_return_val_if_fail (input != NULL, FALSE);

	input->size = size;
	return TRUE;
}

/**
 * gsf_input_seek_emulate: Emulate forward seeks by reading.
 * @input :
 * @pos :
 *
 * Returns : TRUE if the emulation worked.
 */
gboolean
gsf_input_seek_emulate (GsfInput *input, gsf_off_t pos)
{
	if (pos < input->cur_offset)
		return TRUE;

	while (pos > input->cur_offset) {
		size_t readcount = MIN (pos - input->cur_offset, 8192);
		if (!gsf_input_read (input, readcount, NULL))
			return TRUE;
	}
	return FALSE;
}

/****************************************************************************/

/**
 * gsf_input_error :
 *
 * Returns : A utility quark to flag a GError as being an input problem.
 */
GQuark 
gsf_input_error (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gsf_input_error");
	return quark;
}

/****************************************************************************/

/**
 * gsf_input_uncompress: maybe uncompress stream.
 *
 * @src: stream to be uncompressed.
 *
 * Returns: A stream equivalent to the source stream, but uncompressed if
 * the source was compressed.
 *
 * This functions takes ownership of the incoming reference and yields a
 * new one as its output.
 */
GsfInput *
gsf_input_uncompress (GsfInput *src)
{
	gsf_off_t cur_offset = src->cur_offset;
	const char *data;

	if (gsf_input_seek (src, (gsf_off_t) 0, G_SEEK_SET))
		goto error;

	/* Read header up front, so we avoid extra seeks in tests.  */
	data = gsf_input_read (src, 4, NULL);
	if (!data)
		goto error;

	/* Let's try gzip.  */
	{
		const char gzip_sig[2] = { 0x1f, 0x8b };

		if (memcmp (gzip_sig, data, sizeof (gzip_sig)) == 0) {
			GsfInputGZip *res = gsf_input_gzip_new (src, NULL);
			if (res) {
				g_object_unref (G_OBJECT (src));
				return gsf_input_uncompress (GSF_INPUT (res));
			}
		}
	}

	/* Other methods go here.  */

 error:
	(void)gsf_input_seek (src, cur_offset, G_SEEK_SET);
	return src;
}
