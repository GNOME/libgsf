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
gsf_mem_dump (guint8 const *ptr, int len)
{
	int lp,lp2;
	int off;

	for (lp = 0;lp<(len+15)/16;lp++)
	{
		g_print ("%8x | ", lp*16);
		for (lp2 = 0;lp2<16;lp2++) {
			off = lp2 + (lp<<4);
			off<len?g_print("%2x ", ptr[off]):g_print("XX ");
		}
		printf ("| ");
		for (lp2 = 0;lp2<16;lp2++) {
			off = lp2 + (lp<<4);
			g_print ("%c", off<len?(ptr[off]>'!'&&ptr[off]<127?ptr[off]:'.'):'*');
		}
		g_print ("\n");
	}
}
