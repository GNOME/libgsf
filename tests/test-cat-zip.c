/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-cat-zip.c: test program to cat a file from zip archive.
 *
 * Copyright (C) 2002	Tambet Ingo (tambet@ximian.com)
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
 *
 */

#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-zip.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static int
test (int argc, char *argv[])
{
	GsfInput  *input, *stream;
	GsfInfile *infile;
	GError    *err;
	int i, j;

	fprintf (stderr, "%s\n",argv[1]);

	input = gsf_input_mmap_new (argv[1], &err);
	if (input == NULL) {
		g_return_val_if_fail (err != NULL, 1);
		g_warning ("'%s' error: %s", argv[1], err->message);
		g_error_free (err);
		return 1;
	}

	input = gsf_input_uncompress (input);

	infile = gsf_infile_zip_new (input, &err);
	if (infile == NULL) {
		g_return_val_if_fail (err != NULL, 1);

		g_warning ("'%s' Not a zip file: %s", argv[1], err->message);
		g_error_free (err);
		g_object_unref (G_OBJECT (input));
		return 1;
	}

	for (i = 2 ; i < argc ; i++) {
		guint8 const *data;

		stream = gsf_infile_child_by_name (infile, argv[i]);
		if (stream == NULL) {
			g_warning ("Couldn't find file %s in archive.", argv[i]);
			continue;
		}

		fprintf (stderr, "%s\n",argv[i]);

		while (NULL != (data = gsf_input_read (stream, 4, NULL))) {
			for (j = 0; j < 4; j++)
				putchar (data[j]);
		}
		g_object_unref (G_OBJECT (stream));
	}

	g_object_unref (G_OBJECT (infile));
	g_object_unref (G_OBJECT (input));

	return 0;
}

int
main (int argc, char *argv[])
{
	int res;

	if (argc < 3) {
		fprintf (stderr, "%s : file.zip filename-inside-zip-file ...\n", argv [0]);
		return 1;
	}

	gsf_init ();
	res = test (argc, argv);
	gsf_shutdown ();

	return res;
}
