/*
 * gsf-input-iochannel.c: GIOChannel based input
 *
 * Copyright (C) 2002 Rodrigo Moya (rodrigo@gnome-db.org)
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
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-input-impl.h>
#include <gsf-input-iochannel.h>

struct _GsfInputIOChannel {
	GsfInput    input;

	GIOChannel *channel;
	guint8     *buffer;
	size_t      allocated;
};

typedef struct {
	GsfInputClass input_class;
} GsfInputIOChannelClass;

/**
 * gsf_input_iochannel_new :
 * @channel : a #GIOChannel.
 *
 * Returns a new #GsfInputIOChannel or NULL.
 */
GsfInputIOChannel *
gsf_input_iochannel_new (GIOChannel *channel)
{
	GsfInputIOChannel *input;

	g_return_val_if_fail (channel != NULL, NULL);

	input = g_object_new (GSF_INPUT_IOCHANNEL_TYPE, NULL);
	input->channel = channel;

	return input;
}

static void
gsf_input_iochannel_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInputIOChannel *input = (GsfInputIOChannel *) obj;

	g_io_channel_unref (input->channel);
	input->channel = NULL;

	if (input->buffer) {
		g_free (input->buffer);
		input->buffer = NULL;
		input->allocated = 0;
	}

	parent_class = g_type_class_peek (GSF_INPUT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static GsfInput *
gsf_input_iochannel_dup (GsfInput *src_input, GError **err)
{
	GsfInputIOChannel *input = (GsfInputIOChannel *) src_input;

	g_io_channel_ref (input->channel);
	return (GsfInput *) gsf_input_iochannel_new (input->channel);
}

static guint8 const *
gsf_input_iochannel_read (GsfInput *input, size_t num_bytes, guint8 *buffer)
{
	size_t nread = 0, total_read = 0;
	GIOStatus status = G_IO_STATUS_NORMAL;
	GError *err;
	GsfInputIOChannel *io = (GsfInputIOChannel *) input;

	g_return_val_if_fail (GSF_IS_INPUT_IOCHANNEL (io), NULL);
	g_return_val_if_fail (io->channel != NULL, NULL);

	if (buffer == NULL) {
		if (io->allocated < num_bytes) {
			io->allocated = num_bytes;
			if (io->buffer != NULL)
				g_free (io->buffer);
			io->buffer = g_new0 (guint8, io->allocated);
		}

		buffer = io->buffer;
	}

	while ((status == G_IO_STATUS_NORMAL) && (total_read < num_bytes)) {
		err = NULL;

		status = g_io_channel_read_chars (io->channel, (gchar *)(buffer + total_read),
						  num_bytes - total_read, &nread, &err);
		if (err != NULL) {
			g_message ("gsf_input_iochannel_read: %s", err->message);
			g_error_free (err);
			return NULL;
		}

		total_read += nread;
	}

	return buffer;
}

static gboolean
gsf_input_iochannel_seek (GsfInput *input, gsf_off_t offset, GSeekType whence)
{
        GsfInputIOChannel *io = GSF_INPUT_IOCHANNEL (input);
        GIOStatus status = G_IO_STATUS_NORMAL;
                                                                                             
        status = g_io_channel_seek_position (io->channel, offset, whence, NULL);
        if (status == G_IO_STATUS_NORMAL)
                return TRUE;

        return FALSE;
}

static void
gsf_input_iochannel_init (GObject *obj)
{
	GsfInputIOChannel *io = GSF_INPUT_IOCHANNEL (obj);

	io->channel = NULL;
	io->buffer = NULL;
	io->allocated = 0;
}

static void
gsf_input_iochannel_class_init (GObjectClass *gobject_class)
{
	GsfInputClass *input_class = GSF_INPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_input_iochannel_finalize;
	input_class->Dup	= gsf_input_iochannel_dup;
	input_class->Read	= gsf_input_iochannel_read;
	input_class->Seek	= gsf_input_iochannel_seek;
}

GSF_CLASS (GsfInputIOChannel, gsf_input_iochannel,
	   gsf_input_iochannel_class_init, gsf_input_iochannel_init, GSF_INPUT_TYPE)
