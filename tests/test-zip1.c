/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-zip1.c:
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

#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-zip.h>

#include <stdio.h>

static void
ls_R (GsfInput *input)
{
	int i;
	gboolean is_dir = GSF_IS_INFILE (input) &&
		(gsf_infile_num_children (GSF_INFILE (input)) >= 0);
	
	printf ("%c '%s' %" GSF_OFF_T_FORMAT "\n",
		(is_dir ? 'd' : ' '),
		gsf_input_name (GSF_INPUT (input)),
		gsf_input_size (GSF_INPUT (input)));

	if (!is_dir)
		return;

	puts ("{");
	for (i = 0 ; i < gsf_infile_num_children (GSF_INFILE (input)) ; i++)
		ls_R (gsf_infile_child_by_index (GSF_INFILE (input), i));
	puts ("}");
}

static int
test (int argc, char *argv[])
{
	GsfInput  *input;
	GsfInfile *infile;
	GError    *err;
	int i;

	for (i = 1 ; i < argc ; i++) {
		puts (argv[i]);
		input = GSF_INPUT (gsf_input_stdio_new (argv[i], &err));
		if (input == NULL) {

			g_return_val_if_fail (err != NULL, 1);

			g_warning ("'%s' error: %s", argv[i], err->message);
			g_error_free (err);
			continue;
		}

		infile = GSF_INFILE (gsf_infile_zip_new (input, &err));
		if (infile == NULL) {

			g_return_val_if_fail (err != NULL, 1);

			g_warning ("'%s' Not a Zip file: %s", argv[i], err->message);
			g_error_free (err);
			continue;
		}

		ls_R (GSF_INPUT (infile));
		g_object_unref (G_OBJECT (infile));
		g_object_unref (G_OBJECT (input));
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
