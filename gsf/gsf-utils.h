/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-utils.h: 
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

#ifndef GSF_UTILS_H
#define GSF_UTILS_H

#include <gsf/gsf.h>

G_BEGIN_DECLS

/* Do this the ugly way so that we don't have to worry about alignment */
#define GSF_LE_GET_GUINT8(p) (*(guint8 const *)(p))
#define GSF_LE_GET_GUINT16(p)				\
	(guint16)((((guint8 const *)(p))[0] << 0)  |	\
		  (((guint8 const *)(p))[1] << 8))
#define GSF_LE_GET_GUINT32(p)				\
	(guint32)((((guint8 const *)(p))[0] << 0)  |	\
		  (((guint8 const *)(p))[1] << 8)  |	\
		  (((guint8 const *)(p))[2] << 16) |	\
		  (((guint8 const *)(p))[3] << 24))

#define GSF_LE_SET_GUINT8(p, dat)			\
	(*((guint8 *)(p))      = ((dat)        & 0xff))
#define GSF_LE_SET_GUINT16(p, dat)			\
	((*((guint8 *)(p) + 0) = ((dat)        & 0xff)),\
	 (*((guint8 *)(p) + 1) = ((dat) >>  8) & 0xff))
#define GSF_LE_SET_GUINT32(p, dat)				\
	((*((guint8 *)(p) + 0) = ((dat))       & 0xff),	\
	 (*((guint8 *)(p) + 1) = ((dat) >>  8) & 0xff),	\
	 (*((guint8 *)(p) + 2) = ((dat) >> 16) & 0xff),	\
	 (*((guint8 *)(p) + 3) = ((dat) >> 24) & 0xff))

void gsf_init (void);
void gsf_shutdown (void);

/* Debugging utilities */
void gsf_mem_dump   (guint8 const *ptr, size_t len);
void gsf_input_dump (GsfInput *input);

/* Some version checking */
extern int libgsf_major_version;
extern int libgsf_minor_version;
extern int libgsf_micro_version;

G_END_DECLS

#endif /* GSF_UTILS_H */
