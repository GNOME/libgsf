/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-msole.h: 
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

#ifndef GSF_INFILE_MSOLE_H
#define GSF_INFILE_MSOLE_H

#include <gsf/gsf.h>

G_BEGIN_DECLS

typedef struct _GsfInfileMSOle GsfInfileMSOle;

#define GSF_INFILE_MSOLE_TYPE        (gsf_infile_msole_get_type ())
#define GSF_INFILE_MSOLE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_INFILE_MSOLE_TYPE, GsfInfileMSOle))
#define IS_GSF_INFILE_MSOLE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_INFILE_MSOLE_TYPE))

typedef enum {
	GSF_MSOLE_PROP_STRING_MASK	= 0x10000,
	GSF_MSOLE_PROP_TIME_MASK	= 0x20000,
	GSF_MSOLE_PROP_WORD_MASK	= 0x30000,
	GSF_MSOLE_PROP_DWORD_MASK	= 0x40000,
	GSF_MSOLE_PROP_CLIPBOARD_MASK	= 0x50000,
	GSF_MSOLE_PROP_TYPE_MASK	= 0xf0000,
	GSF_MSOLE_PROP_INDEX_MASK	= 0x0ffff,

	GSF_MSOLE_PROP_CODEPAGE		= 0x30001,
	GSF_MSOLE_PROP_TITLE		= 0x10002,
	GSF_MSOLE_PROP_SUBJECT		= 0x10003,
	GSF_MSOLE_PROP_AUTHOR		= 0x10004,
	GSF_MSOLE_PROP_KEYWORDS		= 0x10005,
	GSF_MSOLE_PROP_COMMENTS		= 0x10006,
	GSF_MSOLE_PROP_TEMPLATE		= 0x10007,
	GSF_MSOLE_PROP_LASTAUTHOR	= 0x10008,
	GSF_MSOLE_PROP_REVNUMBER	= 0x10009,
	GSF_MSOLE_PROP_EDITTIME		= 0x2000A,
	GSF_MSOLE_PROP_LASTPRINTED	= 0x2000B,
	GSF_MSOLE_PROP_CREATED		= 0x2000C,
	GSF_MSOLE_PROP_LASTSAVED	= 0x2000D,
	GSF_MSOLE_PROP_PAGECOUNT	= 0x4000E,
	GSF_MSOLE_PROP_WORDCOUNT	= 0x4000F,
	GSF_MSOLE_PROP_CHARCOUNT	= 0x40010,
	GSF_MSOLE_PROP_THUMBNAIL	= 0x50011,
	GSF_MSOLE_PROP_APPNAME		= 0x10012,
	GSF_MSOLE_PROP_SECURITY		= 0x40013,
} GsfMSOleProperty;

GType gsf_infile_msole_get_type (void);

GsfInfile *gsf_infile_msole_new (GsfInput *input, GError **err);

G_END_DECLS

#endif /* GSF_INFILE_MSOLE_H */
