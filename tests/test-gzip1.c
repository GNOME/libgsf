/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-gzip1.c:
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
#include <gsf/gsf-input-gzip.h>

#include <stdio.h>

static int
test (int argc, char *argv[])
{
	GsfInput     *input;
	GsfInputGZip *gzip;
	GError       *err;
	int i;
	size_t size, count;
	guint8 const *data;

	for (i = 1 ; i < argc ; i++) {
		puts (argv[i]);
		input = gsf_input_stdio_new (argv[i], &err);
		if (input == NULL) {

			g_return_val_if_fail (err != NULL, 1);

			g_warning ("'%s' error: %s\n", argv[i], err->message);
			g_error_free (err);
			continue;
		}

		gzip = gsf_input_gzip_new (input, &err);
		if (gzip == NULL) {

			g_return_val_if_fail (err != NULL, 1);

			g_warning ("'%s' Not a GZip file: %s\n", argv[i], err->message);
			g_error_free (err);
			continue;
		}

		/* read in small blocks to excercise things */
		size = gsf_input_size (GSF_INPUT (gzip));
		while (size > 0) {
			count = size;
			if (count > 0x100)
				count = 0x100;
			data = gsf_input_read (GSF_INPUT (gzip), count, NULL);
			g_return_val_if_fail (data != NULL, 1);
			fwrite (data, 1, count, stdout);
			size -= count;
		}
		g_object_unref (G_OBJECT (gzip));
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
