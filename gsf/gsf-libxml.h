/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-libxml.h: Utility wrappers for using gsf with libxml
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

#ifndef GSF_LIBXML_H
#define GSF_LIBXML_H

#include <gsf/gsf.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

xmlParserCtxtPtr	gsf_xml_parser_context	(GsfInput  *input);
int			gsf_xmlDocFormatDump	(GsfOutput *output,
						 xmlDocPtr cur, gboolean format);

G_END_DECLS

#endif /* GSF_LIBXML_H */
