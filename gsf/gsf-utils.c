/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-utils.c: 
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
#include <gsf/gsf-utils.h>
#include <gsf/gsf-input.h>

#include <ctype.h>
#include <stdio.h>

void
gsf_init (void)
{
	g_type_init ();
}

void
gsf_shutdown (void)
{
}

/**
 * gsf_mem_dump :
 * @ptr: memory area to be dumped.
 * @len: how many bytes will be dumped.
 *
 * Dump @len bytes from the memory location given by @ptr.
 **/
void
gsf_mem_dump (guint8 const *ptr, size_t len)
{
	size_t lp, lp2, off;

	for (lp = 0;lp<(len+15)/16;lp++)
	{
		g_print ("%8x | ", lp*16);
		for (lp2 = 0;lp2<16;lp2++) {
			off = lp2 + (lp<<4);
			off<len?g_print("%2x ", ptr[off]):g_print("XX ");
		}
		g_print ("| ");
		for (lp2 = 0;lp2<16;lp2++) {
			off = lp2 + (lp<<4);
			g_print ("%c", off < len ? (ptr[off] >= '!' && ptr[off] < 127 ? ptr[off] : '.') : '*');
		}
		g_print ("\n");
	}
}

void
gsf_input_dump (GsfInput *input)
{
	size_t size, count;
	guint8 const *data;

	/* read in small blocks to excercise things */
	size = gsf_input_size (GSF_INPUT (input));
	while (size > 0) {
		count = size;
		if (count > 0x100)
			count = 0x100;
		data = gsf_input_read (GSF_INPUT (input), count, NULL);
		g_return_if_fail (data != NULL);
		fwrite (data, 1, count, stdout);
		size -= count;
	}
}


double
gsf_le_get_double (const void *p)
{
#if G_BYTE_ORDER == G_BIG_ENDIAN
	if (sizeof (double) == 8) {
		double  d;
		int     i;
		guint8 *t  = (guint8 *)&d;
		guint8 *p2 = (guint8 *)p;
		int     sd = sizeof (d);

		for (i = 0; i < sd; i++)
			t[i] = p2[sd - 1 - i];

		return d;
	} else {
		g_error ("Big endian machine, but weird size of doubles");
	}
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
	if (sizeof (double) == 8) {
		/*
		 * On i86, we could access directly, but Alphas require
		 * aligned access.
		 */
		double data;
		memcpy (&data, p, sizeof (data));
		return data;
	} else {
		g_error ("Little endian machine, but weird size of doubles");
	}
#else
#error "Byte order not recognised -- out of luck"
#endif
}

void
gsf_le_set_double (void *p, double d)
{
#if G_BYTE_ORDER == G_BIG_ENDIAN
	if (sizeof (double) == 8) {
		int     i;
		guint8 *t  = (guint8 *)&d;
		guint8 *p2 = (guint8 *)p;
		int     sd = sizeof (d);

		for (i = 0; i < sd; i++)
			p2[sd - 1 - i] = t[i];
	} else {
		g_error ("Big endian machine, but weird size of doubles");
	}
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
	if (sizeof (double) == 8) {
		/*
		 * On i86, we could access directly, but Alphas require
		 * aligned access.
		 */
		memcpy (p, &d, sizeof (d));
	} else {
		g_error ("Little endian machine, but weird size of doubles");
	}
#else
#error "Byte order not recognised -- out of luck"
#endif
}
