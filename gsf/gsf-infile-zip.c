/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-zip.c :
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
#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf-infile-impl.h>
#include <gsf/gsf-impl-utils.h>

struct _GsfInfileZip {
	GsfInfile parent;

	GsfInput *input;
};

typedef struct {
	GsfInfileClass  parent_class;
} GsfInfileZipClass;

#define GSF_INFILE_ZIP_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GSF_INFILE_ZIP_TYPE, GsfInfileZipClass))
#define IS_GSF_INFILE_ZIP_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GSF_INFILE_ZIP_TYPE))
static void
gsf_infile_zip_finalize (GObject *obj)
{
	GObjectClass *parent_class;

	parent_class = g_type_class_peek (GSF_INFILE_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static void
gsf_infile_zip_init (GObject *obj)
{
}

static void
gsf_infile_zip_class_init (GObjectClass *gobject_class)
{
	gobject_class->finalize = gsf_infile_zip_finalize;
}

GSF_CLASS (GsfInfileZip, gsf_infile_zip,
	   gsf_infile_zip_class_init, gsf_infile_zip_init,
	   GSF_INFILE_TYPE)

GsfInfile *
gsf_infile_zip_new (GsfInput *input, GError **err)
{
	GsfInfileZip *zip;

	zip = g_object_new (GSF_INFILE_ZIP_TYPE, NULL);
	zip->input = input; /* absorb reference */

	if (0) {
		g_object_unref (G_OBJECT (zip));
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"Not a zip file");
		return NULL;
	}
	return GSF_INFILE (zip);
}

char const *
gsf_infile_zip_get_ooname (GsfInput *input)
{
	return NULL;
}
