/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-msole-utils.h: various tools for handling MS OLE files
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

#ifndef GSF_MSOLE_UTILS_H
#define GSF_MSOLE_UTILS_H

#include <gsf/gsf.h>

G_BEGIN_DECLS

gboolean gsf_msole_metadata_read  (GsfInput *in,   GError **err);
gboolean gsf_msole_metadata_write (GsfOutput *out, gboolean doc_not_component,
				   GError **err);

guint   gsf_msole_lid_for_language	(char const *lang);
guint   gsf_msole_codepage_to_lid	(guint codepage);
guint   gsf_msole_lid_to_codepage	(guint lid);
gchar * gsf_msole_lid_to_codepage_str	(guint lid);
char const* gsf_msole_language_for_lid	(guint lid);

guint	gsf_msole_iconv_win_codepage    (void) ;
GIConv	gsf_msole_iconv_open_for_import (guint codepage) ;
GIConv	gsf_msole_iconv_open_for_export (void) ;

GIConv gsf_msole_iconv_open_codepage_for_import  (char const *to, guint codepage) ;
GIConv gsf_msole_iconv_open_codepages_for_export (guint codepage_to, char const *from);
GIConv gsf_msole_iconv_open_codepage_for_export  (guint codepage_to);

G_END_DECLS

#endif /* GSF_MSOLE_UTILS_H */
