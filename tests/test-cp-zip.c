/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-cp-zip.c: Test gsf-outfile-zip by cloning a file the hard way
 *
 * Copyright (C) 2002-2003	Jody Goldberg (jody@gnome.org)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <gsf/gsf-utils.h>

#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-zip.h>

#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-outfile.h>
#include <gsf/gsf-outfile-zip.h>

#include <stdio.h>

static void
clone (GsfInfile *in, GsfOutfile *out)
{
	GsfInput *input = GSF_INPUT (in);
	GsfOutput *output = GSF_OUTPUT (out);
	guint8 const *data;
	size_t len;
	int i;

	if (gsf_input_size (input) > 0) {
		while ((len = gsf_input_remaining (input)) > 0) {
			/* copy in odd sized chunks to exercise system */
			if (len > 314)
				len = 314;
			if (NULL == (data = gsf_input_read (input, len, NULL))) {
				g_warning ("error reading ?");
				return;
			}
			if (!gsf_output_write (output, len, data)) {
				g_warning ("error writing ?");
				return;
			}
		}
	} else for (i = 0 ; i < gsf_infile_num_children (in) ; i++) {
		input = gsf_infile_child_by_index (in, i);
		output = gsf_outfile_new_child  (out,
			gsf_infile_name_by_index  (in, i),
			gsf_infile_num_children (GSF_INFILE (input)) >= 0);
		clone (GSF_INFILE (input), GSF_OUTFILE (output));
	}
	gsf_output_close (GSF_OUTPUT (out));
	g_object_unref (G_OBJECT (out));
	g_object_unref (G_OBJECT (in));
}

static int
test (char *argv[])
{
	GsfInput   *input;
	GsfInfile  *infile;
	GsfOutput  *output;
	GsfOutfile *outfile;
	GError    *err;

	fprintf (stderr, "%s\n", argv [1]);
	input = GSF_INPUT (gsf_input_stdio_new (argv[1], &err));
	if (input == NULL) {

		g_return_val_if_fail (err != NULL, 1);

		g_warning ("'%s' error: %s", argv[1], err->message);
		g_error_free (err);
		return 1;
	}

	infile = GSF_INFILE (gsf_infile_zip_new (input, &err));
	g_object_unref (G_OBJECT (input));

	if (infile == NULL) {
		g_return_val_if_fail (err != NULL, 1);

		g_warning ("'%s' Not a zip file: %s", argv[1], err->message);
		g_error_free (err);
		return 1;
	}

	output = GSF_OUTPUT (gsf_output_stdio_new (argv[2], &err));
	if (output == NULL) {

		g_return_val_if_fail (err != NULL, 1);

		g_warning ("'%s' error: %s", argv[2], err->message);
		g_error_free (err);
		g_object_unref (G_OBJECT (infile));
		return 1;
	}

	outfile = GSF_OUTFILE (gsf_outfile_zip_new (output, &err));
	g_object_unref (G_OBJECT (output));
	clone (infile, outfile);

	return 0;
}

int
main (int argc, char *argv[])
{
	int res;

	if (argc != 3) {
		fprintf (stderr, "%s : infile outfile\n", argv [0]);
		return 1;
	}

	gsf_init ();
	res = test (argv);
	gsf_shutdown ();

	return res;
}
