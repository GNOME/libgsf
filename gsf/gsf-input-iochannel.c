/*
 * gsf-input-iochannel.c: GIOChannel based input
 *
 * Copyright (C) 2003 Rodrigo Moya (rodrigo@gnome-db.org)
 * Copyright (C) 2003 Dom Lachowicz (cinamod@hotmail.com)
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
#include <gsf-input-memory.h>

struct _GsfInputIOChannel {
	GsfInput    input;

        GsfInputMemory * memory;
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
	gchar * buf;
	gsize len;

	g_return_val_if_fail (channel != NULL, NULL);

	if (G_IO_STATUS_NORMAL != g_io_channel_read_to_end (channel, &buf, &len, NULL))
	        return NULL;

	input = g_object_new (GSF_INPUT_IOCHANNEL_TYPE, NULL);
	input->memory = gsf_input_memory_new (buf, len, TRUE);
	gsf_input_set_size (GSF_INPUT (input), gsf_input_get_size (GSF_INPUT (input->memory)));

	return input;
}

static void
gsf_input_iochannel_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInputIOChannel *input = (GsfInputIOChannel *) obj;

	g_object_unref (G_OBJECT (input->memory));

	parent_class = g_type_class_peek (GSF_INPUT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static GsfInput *
gsf_input_iochannel_dup (GsfInput *src_input, GError **err)
{
	GsfInputIOChannel *input = (GsfInputIOChannel *) src_input;
	GsfInputIOChannel *dup = g_object_new (GSF_INPUT_IOCHANNEL_TYPE, NULL);
	dup->memory = input->memory;

	g_object_ref (G_OBJECT (input->memory));
	return (GsfInput *) dup;
}

static guint8 const *
gsf_input_iochannel_read (GsfInput *input, size_t num_bytes, guint8 *buffer)
{
        GsfInputIOChannel *io = GSF_INPUT_IOCHANNEL (input);
        return gsf_input_read (GSF_INPUT (io->memory), num_bytes, buffer);
}

static gboolean
gsf_input_iochannel_seek (GsfInput *input, gsf_off_t offset, GSeekType whence)
{
        GsfInputIOChannel *io = GSF_INPUT_IOCHANNEL (input);
	return gsf_input_seek (GSF_INPUT (io->memory), offset, whence);
}

static void
gsf_input_iochannel_init (GObject *obj)
{
	GsfInputIOChannel *io = GSF_INPUT_IOCHANNEL (obj);
	io->memory = NULL;
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
