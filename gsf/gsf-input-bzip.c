/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-input-iochannel.c: BZ2 based input
 *
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
#include <gsf-input-bzip.h>

#ifdef HAVE_BZ2
#include <bzlib.h>
#endif

/**
 * gsf_input_memory_new_from_bzip :
 * @source : a #GsfInput
 * @error : a #GError
 *
 * Returns a new #GsfInputMemory or NULL.
 */
GsfInputMemory * 
gsf_input_memory_new_from_bzip (GsfInput *source, GError **err)
{
#ifndef HAVE_BZ2
#warning Building without BZ2 support
	if (err)
		*err = g_error_new (gsf_input_error (), 0,
				    "BZ2 support not enabled");
	return NULL;
#else
	g_return_val_if_fail (source != NULL, NULL);

	/* TODO */

	return NULL;
#endif
}
