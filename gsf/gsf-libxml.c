/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-libxml.c :
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
#include <gsf/gsf-libxml.h>
#include <gsf/gsf-input.h>
#include <gsf/gsf-input-gzip.h>

/* Note: libxml erroneously declares the length argument as int.  */
static int
gsf_libxml_read (void * context, char * buffer, int len)
{
	int remaining = gsf_input_remaining ((GsfInput *)context);
	if (len > remaining)
		len = remaining;
	if (NULL == gsf_input_read ((GsfInput *)context, (size_t)len, buffer))
		return -1;
	return len;
}

static int
gsf_libxml_close_read (void * context)
{
	g_object_unref (G_OBJECT (context));
	return TRUE;
}

/**
 * gsf_xml_parser_context :
 * @source :
 *
 * Create a libxml2 pull style parser context wrapper around a gsf input.
 * This signature will probably change to supply a SAX structure.
 *
 * NOTE : adds a reference to @source
 *
 * Returns : A parser context or NULL
 **/
xmlParserCtxtPtr
gsf_xml_parser_context (GsfInput *source)
{
	GsfInputGZip *gzip;

	g_return_val_if_fail (IS_GSF_INPUT (source), NULL);

	gzip = gsf_input_gzip_new (source, NULL);
	if (gzip != NULL)
		source = GSF_INPUT (gzip);
	else
		g_object_ref (G_OBJECT (source));

	return xmlCreateIOParserCtxt (
		NULL, NULL,
		(xmlInputReadCallback) gsf_libxml_read, 
		(xmlInputCloseCallback) gsf_libxml_close_read,
		source, XML_CHAR_ENCODING_NONE);
}
