/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input.c: interface for used by the ole layer to read raw data
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
#include <gsf/gsf-input.h>
#include <gsf/gsf.h>
#include <glib/gi18n-lib.h>

#include <string.h>


/*
 * FIXME!
 *
 * We cannot extend GsfInput, so for now we hang this on the object as
 * an attribute.
 */
#define MODTIME_ATTR "GsfInput::modtime"



#define GET_CLASS(instance) G_TYPE_INSTANCE_GET_CLASS (instance, GSF_INPUT_TYPE, GsfInputClass)

static GObjectClass *parent_class;

enum {
	PROP_0,
	PROP_NAME,
	PROP_SIZE,
	PROP_EOF,
	PROP_REMAINING,
	PROP_POS,
	PROP_MODTIME,
	PROP_CONTAINER
};

static void
gsf_input_get_property (GObject     *object,
			guint        property_id,
			GValue      *value,
			GParamSpec  *pspec)
{
	GsfInput *input = GSF_INPUT (object);

	/* gsf_off_t is typedef'd to gint64 */

	switch (property_id) {
	case PROP_NAME:
		g_value_set_string (value, gsf_input_name (input));
		break;
	case PROP_SIZE:
		g_value_set_int64 (value, gsf_input_size (input));
		break;
	case PROP_EOF:
		g_value_set_boolean (value, gsf_input_eof (input));
		break;
	case PROP_REMAINING:
		g_value_set_int64 (value, gsf_input_remaining (input));
		break;
	case PROP_POS:
		g_value_set_int64 (value, gsf_input_tell (input));
		break;
	case PROP_MODTIME:
		g_value_set_boxed (value, gsf_input_get_modtime (input));
		break;
	case PROP_CONTAINER:
		g_value_set_object (value, input->container);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gsf_input_dispose (GObject *obj)
{
	GsfInput *input = GSF_INPUT (obj);

	gsf_input_set_container (input, NULL);
	gsf_input_set_name (input, NULL);
	gsf_input_set_modtime (input, NULL);

	parent_class->dispose (obj);
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
	parent_class = g_type_class_peek_parent (gobject_class);

	gobject_class->dispose = gsf_input_dispose;
	gobject_class->get_property = gsf_input_get_property;

	g_object_class_install_property
		(gobject_class,
		 PROP_NAME,
		 g_param_spec_string ("name",
				      _("Name"),
				      _("The input's name"),
				      NULL,
				      G_PARAM_STATIC_STRINGS |
				      G_PARAM_READABLE));

	/**
	 * GsfInput:size:
	 *
	 * The total number of bytes in the file.
	 */
	g_object_class_install_property
		(gobject_class,
		 PROP_SIZE,
		 g_param_spec_int64 ("size",
				     _("Size"),
				     _("The input's size"),
				     0, G_MAXINT64, 0,
				     G_PARAM_STATIC_STRINGS |
				     G_PARAM_READABLE));

	/**
	 * GsfInput:eof:
	 *
	 * %TRUE if the end of the file has been reached.
	 */
	g_object_class_install_property
		(gobject_class,
		 PROP_EOF,
		 g_param_spec_boolean ("eof",
				       _("EOF"),
				       _("End of file"),
				       FALSE,
				       G_PARAM_STATIC_STRINGS |
				       G_PARAM_READABLE));

	/**
	 * GsfInput:remaining:
	 *
	 * The number of bytes remaining in the file.
	 */
	g_object_class_install_property
		(gobject_class,
		 PROP_REMAINING,
		 g_param_spec_int64 ("remaining",
				     _("Remaining"),
				     _("Amount of data remaining"),
				     0, G_MAXINT64, 0,
				     G_PARAM_STATIC_STRINGS |
				     G_PARAM_READABLE));

	/**
	 * GsfInput:position:
	 *
	 * The current position in the input.
	 */
	g_object_class_install_property
		(gobject_class,
		 PROP_POS,
		 g_param_spec_int64 ("position",
				     _("Position"),
				     _("The input's current position"),
				     0, G_MAXINT64, 0,
				     G_PARAM_STATIC_STRINGS |
				     G_PARAM_READABLE));

	/**
	 * GsfInput:modtime:
	 *
	 * The time the input was last updated.  This represents the
	 * timestamp from the originating file or @GsfInfile member.
	 * It is not supported by all derived classes.
	 */
	g_object_class_install_property
		(gobject_class,
		 PROP_MODTIME,
		 g_param_spec_boxed
		 ("modtime",
		  _("Modification time"),
		  _("An optional GDateTime representing the time the input was last changed"),
		  G_TYPE_DATE_TIME,
		  G_PARAM_STATIC_STRINGS |
		  G_PARAM_READABLE));

	/**
	 * GsfInput:container:
	 *
	 * The container, optionally %NULL, in which this input lives.
	 */
	g_object_class_install_property
		(gobject_class, PROP_CONTAINER,
		 g_param_spec_object ("container",
				      _("Container"),
				      _("The parent GsfInfile"),
				      GSF_INFILE_TYPE,
				      G_PARAM_STATIC_STRINGS |
				      G_PARAM_READABLE));
}

GSF_CLASS_ABSTRACT (GsfInput, gsf_input,
		    gsf_input_class_init, gsf_input_init,
		    G_TYPE_OBJECT)

/**
 * gsf_input_name:
 * @input: the input stream
 *
 * The name of the input stream.
 *
 * Returns: (transfer none): @input's name in utf8 form, or %NULL if it
 * has no name.
 **/
char const *
gsf_input_name (GsfInput *input)
{
	g_return_val_if_fail (GSF_IS_INPUT (input), NULL);
	return input->name;
}

/**
 * gsf_input_container:
 * @input: the input stream
 *
 * Returns: (transfer none) (nullable): @input's container
 **/
GsfInfile *
gsf_input_container (GsfInput *input)
{
	g_return_val_if_fail (GSF_IS_INPUT (input), NULL);
	return input->container;
}

/**
 * gsf_input_dup: (virtual Dup)
 * @input: The input to duplicate
 * @err: (allow-none): place to store a #GError if anything goes wrong
 *
 * Duplicates @input leaving the new one at the same offset.
 *
 * Returns: (transfer full) (nullable): the duplicate
 **/
GsfInput *
gsf_input_dup (GsfInput *input, GError **err)
{
	GsfInput *dst;

	g_return_val_if_fail (input != NULL, NULL);

	dst = GET_CLASS (input)->Dup (input, err);
	if (dst != NULL) {
		if (dst->size != input->size) {
			if (err != NULL)
				*err = g_error_new (gsf_input_error_id (), 0,
						    _("Duplicate size mismatch"));
			g_object_unref (dst);
			return NULL;
		}
		if (gsf_input_seek (dst, input->cur_offset, G_SEEK_SET)) {
			if (err != NULL)
				*err = g_error_new (gsf_input_error_id (), 0,
						    _("Seek failed"));
			g_object_unref (dst);
			return NULL;
		}

		gsf_input_set_name (dst, input->name);
		gsf_input_set_container (dst, input->container);
	}
	return dst;
}

/**
 * gsf_input_sibling: (virtual OpenSibling)
 * @input: The input
 * @name: name.
 * @err: (allow-none): place to store a #GError if anything goes wrong
 *
 * UNIMPLEMENTED BY ANY BACKEND
 * 	and it is probably unnecessary.   gsf_input_get_container provides
 * 	enough power to do what is necessary.
 *
 * Attempts to open a 'sibling' of @input.  The caller is responsible for
 * managing the resulting object.
 *
 * Returns: (transfer full): A related #GsfInput
 **/
GsfInput *
gsf_input_sibling (GsfInput const *input, char const *name, GError **err)
{
	g_return_val_if_fail (GET_CLASS (input)->OpenSibling, NULL);

	return GET_CLASS (input)->OpenSibling (input, name, err);
}

/**
 * gsf_input_size:
 * @input: The input
 *
 * Returns: the total number of bytes in the input or -1 on error
 **/
gsf_off_t
gsf_input_size (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, -1);
	return input->size;
}

/**
 * gsf_input_eof:
 * @input: the input
 *
 * Are we at the end of the file?
 *
 * Returns: %TRUE if the input is at the eof.
 **/
gboolean
gsf_input_eof (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, FALSE);

	return input->cur_offset >= input->size;
}

/**
 * gsf_input_read: (virtual Read) (skip)
 * @input: the input stream
 * @num_bytes: number of bytes to read
 * @optional_buffer: (array) (allow-none): Pointer to destination memory area
 *
 * Read at least @num_bytes.  Does not change the current position if there
 * is an error.  Will only read if the entire amount can be read.  Invalidates
 * the buffer associated with previous calls to gsf_input_read.
 *
 * Returns: (array) (nullable): pointer to the buffer or %NULL if there is
 * an error or 0 bytes are requested.
 **/

guint8 const *
gsf_input_read (GsfInput *input, size_t num_bytes, guint8 *optional_buffer)
{
	guint8 const *res;
	gsf_off_t newpos = input->cur_offset + num_bytes;

	g_return_val_if_fail (input != NULL, NULL);

	if (newpos <= input->cur_offset || newpos > input->size)
		return NULL;
	res = GET_CLASS (input)->Read (input, num_bytes, optional_buffer);
	if (res == NULL)
		return NULL;

	input->cur_offset = newpos;
	return res;
}

/**
 * gsf_input_read0: (rename-to gsf_input_read)
 * @input: the input stream
 * @num_bytes: (in): number of bytes to read
 * @bytes_read: (out): copy of @num_bytes
 *
 * Read @num_bytes.  Does not change the current position if there
 * is an error.  Will only read if the entire amount can be read.
 *
 * Returns: (array length=bytes_read) (element-type guint8) (transfer full):
 * the data read.
 **/

guint8 *
gsf_input_read0 (GsfInput *input, size_t num_bytes, size_t *bytes_read)
{
	guint8 *res;

	g_return_val_if_fail (input != NULL, NULL);
	g_return_val_if_fail (bytes_read != NULL, NULL);

	*bytes_read = num_bytes;

	if (num_bytes < 0 || (gsf_off_t)num_bytes > gsf_input_remaining (input))
		return NULL;

	res = g_new (guint8, num_bytes);
	if (gsf_input_read (input, num_bytes, res))
		return res;

	g_free (res);
	return NULL;
}

/**
 * gsf_input_remaining:
 * @input: the input stream
 *
 * Returns: the number of bytes left in the file.
 **/
gsf_off_t
gsf_input_remaining (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, 0);

	return input->size - input->cur_offset;
}

/**
 * gsf_input_tell:
 * @input: the input stream
 *
 * Returns: the current offset in the file.
 **/
gsf_off_t
gsf_input_tell (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, 0);

	return input->cur_offset;
}

/**
 * gsf_input_seek: (virtual Seek)
 * @input: the input stream
 * @offset: target offset
 * @whence: determines whether the offset is relative to the beginning or
 *          the end of the stream, or to the current location.
 *
 * Move the current location in the input stream.
 *
 * Returns: %TRUE on error.
 **/
gboolean
gsf_input_seek (GsfInput *input, gsf_off_t offset, GSeekType whence)
{
	gsf_off_t pos = offset;

	g_return_val_if_fail (input != NULL, TRUE);

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
 * gsf_input_set_name:
 * @input: the input stream
 * @name: (allow-none): the new name of the stream
 *
 * protected.
 *
 * Returns: %TRUE if the assignment was ok.
 **/
gboolean
gsf_input_set_name (GsfInput *input, char const *name)
{
	g_return_val_if_fail (input != NULL, FALSE);

	if (g_strcmp0 (name, input->name)) {
		g_free (input->name);
		input->name = g_strdup (name);
		g_object_notify (G_OBJECT (input), "name");
	}

	return TRUE;
}

/**
 * gsf_input_set_name_from_filename:
 * @input: the input stream
 * @filename: the (fs-sys encoded) filename
 *
 * protected.
 *
 * Returns: %TRUE if the assignment was ok.
 **/
gboolean
gsf_input_set_name_from_filename (GsfInput *input, char const *filename)
{
	g_return_val_if_fail (input != NULL, FALSE);

	g_free (input->name);
	input->name = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
	return TRUE;
}


/**
 * gsf_input_set_container:
 * @input: the input stream
 * @container: (allow-none)
 *
 * Returns: %TRUE if the assignment was ok.
 */
gboolean
gsf_input_set_container (GsfInput *input, GsfInfile *container)
{
	g_return_val_if_fail (input != NULL, FALSE);

	if (container != NULL)
		g_object_ref (container);
	if (input->container != NULL)
		g_object_unref (input->container);
	input->container = container;
	return TRUE;
}

/**
 * gsf_input_set_size:
 * @input: the input stream
 * @size: the size of the stream
 *
 * Returns: %TRUE if the assignment was ok.
 */
gboolean
gsf_input_set_size (GsfInput *input, gsf_off_t size)
{
	g_return_val_if_fail (input != NULL, FALSE);
	g_return_val_if_fail (size >= 0, FALSE);

	input->size = size;
	return TRUE;
}

/**
 * gsf_input_seek_emulate:
 * @input: stream to emulate seek for
 * @pos: absolute position to seek to
 *
 * Emulate forward seeks by reading.
 *
 * Returns: %TRUE if the emulation failed.
 */
gboolean
gsf_input_seek_emulate (GsfInput *input, gsf_off_t pos)
{
	if (pos < input->cur_offset)
		return TRUE;

	while (pos > input->cur_offset) {
		gsf_off_t readcount = MIN (pos - input->cur_offset, 8192);
		if (!gsf_input_read (input, readcount, NULL))
			return TRUE;
	}
	return FALSE;
}

/**
 * gsf_input_get_modtime:
 * @input: the input stream
 *
 * Returns: (transfer none): A #GDateTime representing when the input
 * was last modified, or %NULL if not known.
 */
GDateTime *
gsf_input_get_modtime (GsfInput *input)
{
	g_return_val_if_fail (GSF_IS_INPUT (input), NULL);

	return g_object_get_data (G_OBJECT (input), MODTIME_ATTR);
}

/**
 * gsf_input_set_modtime:
 * @input: the input stream
 * @modtime: (transfer none) (allow-none): the new modification time.
 *
 * protected.
 *
 * Returns: %TRUE if the assignment was ok.
 */
gboolean
gsf_input_set_modtime (GsfInput *input, GDateTime *modtime)
{
	g_return_val_if_fail (GSF_IS_INPUT (input), FALSE);

	if (modtime)
		modtime = g_date_time_add (modtime, 0); /* Copy */

	/* This actually also works for null modtime.  */
	g_object_set_data_full (G_OBJECT (input),
				MODTIME_ATTR, modtime,
				(GDestroyNotify)g_date_time_unref);

	return TRUE;
}

gboolean
gsf_input_set_modtime_from_stat (GsfInput *input,
				 const struct stat *st)
{
	GDateTime *modtime = NULL, *ut = NULL;
	gboolean res;
	gint64 sec, usec;

	if (st->st_mtime == (time_t)-1)
		return FALSE;

	sec = st->st_mtime;
#if defined (HAVE_STRUCT_STAT_ST_MTIMENSEC)
	usec = st->st_mtimensec / 1000;
#elif defined (HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
	usec =  st->st_mtim.tv_nsec / 1000;
#else
	usec = 0;
#endif

	ut = g_date_time_new_from_unix_utc (sec);
	modtime = g_date_time_add (ut, usec);
	res = gsf_input_set_modtime (GSF_INPUT (input), modtime);
	g_date_time_unref (ut);
	g_date_time_unref (modtime);

	return res;
}


/****************************************************************************/

/**
 * gsf_input_error_id:
 *
 * Returns: A utility quark to flag a #GError as being an input problem.
 */
GQuark
gsf_input_error_id (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gsf_input_error_id");
	return quark;
}

/**
 * gsf_input_error:
 *
 * Deprecated as of GSF 1.12.0; use gsf_input_error_id() instead.
 *
 * Returns: A utility quark to flag a #GError as being an input problem.
 */
GQuark
gsf_input_error (void)
{
	return gsf_input_error_id ();
}

/****************************************************************************/

#define GSF_READ_BUFSIZE (1024 * 4)

/**
 * gsf_input_copy:
 * @input: a non-null #GsfInput
 * @output: a non-null #GsfOutput
 *
 * Copy the contents from @input to @output from their respective
 * current positions. So if you want to be sure to copy *everything*,
 * make sure to call gsf_input_seek (input, 0, G_SEEK_SET) and
 * gsf_output_seek (output, 0, G_SEEK_SET) first, if applicable.
 *
 * Returns: %TRUE on success
 **/
gboolean
gsf_input_copy (GsfInput *input, GsfOutput *output)
{
	gboolean success = TRUE;
	gsf_off_t remaining;

	g_return_val_if_fail (input != NULL, FALSE);
	g_return_val_if_fail (output != NULL, FALSE);

	while (success && (remaining = gsf_input_remaining (input)) > 0) {
		gsf_off_t toread = MIN (remaining, GSF_READ_BUFSIZE);
		const guint8 * buffer = gsf_input_read (input, toread, NULL);
		if (buffer)
			success = gsf_output_write (output, toread, buffer);
		else
			success = FALSE;
	}

	return success;
}

/****************************************************************************/

/**
 * gsf_input_uncompress:
 * @src: (transfer full): stream to be uncompressed.
 *
 * This functions takes ownership of the incoming reference and yields a
 * new one as its output.
 *
 * Returns: (transfer full): A stream equivalent to the source stream,
 * but uncompressed if the source was compressed.
 **/
GsfInput *
gsf_input_uncompress (GsfInput *src)
{
	gsf_off_t cur_offset = src->cur_offset;
	guint8 header[4];

	if (gsf_input_seek (src, 0, G_SEEK_SET))
		goto error;

	/* Read header up front, so we avoid extra seeks in tests.  */
	if (!gsf_input_read (src, 4, header))
		goto error;

	/* Let's try gzip.  */
	{
		const unsigned char gzip_sig[2] = { 0x1f, 0x8b };

		if (memcmp (gzip_sig, header, sizeof (gzip_sig)) == 0) {
			GsfInput *res = gsf_input_gzip_new (src, NULL);
			if (res) {
				g_object_unref (src);
				return gsf_input_uncompress (res);
			}
		}
	}

	/* Let's try bzip.  */
	{
		guint8 const *bzip_sig = "BZh";

		if (memcmp (bzip_sig, header, strlen (bzip_sig)) == 0) {
			GsfInput *res = gsf_input_memory_new_from_bzip (src, NULL);
			if (res) {
				g_object_unref (src);
				return gsf_input_uncompress (res);
			}
		}
	}

	/* Other methods go here.  */

 error:
	(void)gsf_input_seek (src, cur_offset, G_SEEK_SET);
	return src;
}
