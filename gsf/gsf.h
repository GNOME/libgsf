/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * libgsf.h: 
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

#ifndef LIBGSF_H
#define LIBGSF_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GsfInput	GsfInput;
typedef struct _GsfInfile 	GsfInfile;

typedef struct _GsfOutput	GsfOutput;
typedef struct _GsfOutfile 	GsfOutfile;

typedef enum {
	GSF_SEEK_SET,
	GSF_SEEK_CUR,
	GSF_SEEK_END
} GsfOff_t;

/* Some version checking */
extern int libgsf_major_version;
extern int libgsf_minor_version;
extern int libgsf_micro_version;

void gsf_init (void);
void gsf_shutdown (void);

G_END_DECLS

#endif /* LIBGSF_H */
