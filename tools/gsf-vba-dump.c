/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-vba-dump.c:
 *
 * Copyright (C) 2002-2008	Jody Goldberg (jody@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-infile-msvba.h>
#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf-open-pkg-utils.h>

#include <stdio.h>

static void
dump_vba (GsfInput *vba, char const *filename, GError **err)
{
	GsfInfile *vba_wrapper;

	fprintf (stderr, "%s\n", filename);

	vba_wrapper = gsf_infile_msvba_new (GSF_INFILE (vba), err);
	if (vba_wrapper != NULL)
		g_object_unref (G_OBJECT (vba_wrapper));
}

static int
test (int argc, char *argv[])
{
	GsfInput  *input, *vba;
	GsfInfile *infile;
	GError    *err = NULL;
	int i;

	for (i = 1 ; i < argc ; i++) {
		input = gsf_input_mmap_new (argv[i], NULL);
		if (input == NULL)	/* Only report error if stdio fails too */
			input = gsf_input_stdio_new (argv[i], &err);
		if (input != NULL) {
			if (NULL != (infile = gsf_infile_msole_new (input, &err))) {
				// Try XLS first
				vba = gsf_infile_child_by_vname (infile, "_VBA_PROJECT_CUR", "VBA", NULL);

				// Try DOC next
				if (NULL == vba)
					vba = gsf_infile_child_by_vname (infile, "Macros", "VBA", NULL);

				// TODO : PPT is more complex

				if (vba != NULL) {
					dump_vba (vba, argv[1], &err);
					g_object_unref (G_OBJECT (vba));
				}
				g_object_unref (G_OBJECT (infile));
			} else if (NULL != (infile = gsf_infile_zip_new (input, NULL))) {
				GsfInput *main_part = gsf_open_pkg_get_rel_by_type (GSF_INPUT (infile),
					"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");
				if (NULL != err)  {
					g_error_free (err);
					err = NULL;
				}

				if (NULL != main_part) {
					GsfInput *vba_stream = gsf_open_pkg_get_rel_by_type (main_part,
						"http://schemas.microsoft.com/office/2006/relationships/vbaProject");
					if (NULL != vba_stream) {
						GsfInfile *ole = gsf_infile_msole_new (vba_stream, &err);
						if (NULL != ole) {
							vba = gsf_infile_child_by_vname (ole, "VBA", NULL);
							if (NULL != vba) {
								dump_vba (vba, argv[1], &err);
								g_object_unref (G_OBJECT (vba));
							}
							g_object_unref (G_OBJECT (ole));
						}
						g_object_unref (G_OBJECT (vba_stream));
					}
					g_object_unref (G_OBJECT (main_part));
				}
				g_object_unref (G_OBJECT (infile));
			}

			g_object_unref (G_OBJECT (input));
		}

		if (err != NULL) {
			g_warning ("'%s' error: %s", argv[i], err->message);
			g_error_free (err);
		}
	}

	return 0;
}

int
main (int argc, char *argv[])
{
	int res;

	gsf_init ();
	res = test (argc, argv);
	gsf_shutdown ();

	return res;
}
