/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-msole1.c:
 *
 * Copyright (C) 2002	Jody Goldberg (jody@gnome.org)
 * 			Michael Meeks (michael@ximian.com)
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
 * Parts of this code are taken from libole2/test/test-ole.c
 */

#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-msole.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define BIFF_TYPES_FILE    "biff-types.h"

typedef struct {
	guint16 opcode;
	char *name;
} GENERIC_TYPE;

static GPtrArray *biff_types   = NULL;
static void
read_types (const char *fname, GPtrArray **types)
{
	FILE *file = fopen(fname, "r");
	unsigned char buffer[1024];
	*types = g_ptr_array_new ();
	if (!file) {
		char *newname = g_strconcat ("../", fname, NULL);
		file = fopen (newname, "r");
	}
	if (!file) {
		printf ("Can't find vital file '%s'\n", fname);
		return;
	}
	while (!feof(file)) {
		unsigned char *p;
		fgets (buffer, sizeof (buffer)-1, file);
		for (p=buffer;*p;p++)
			if (*p=='0' && *(p+1)=='x') {
				GENERIC_TYPE *bt = g_new (GENERIC_TYPE,1);
				char *name, *pt;
				bt->opcode=strtol(p+2,0,16);
				pt = buffer;
				while (*pt && *pt != '#') pt++;      /* # */
				while (*pt && !isspace(*pt))
					pt++;  /* define */
				while (*pt && isspace(*pt))
					pt++;  /* '   ' */
				while (*pt && *pt != '_') pt++;     /* BIFF_ */
				name = *pt?pt+1:pt;
				while (*pt && !isspace(*pt))
					pt++;
				bt->name = g_strndup (name, (unsigned)(pt - name));
				g_ptr_array_add (*types, bt);
				break;
			}
	}
	fclose (file);
}

static char const *
get_biff_opcode_name (unsigned opcode)
{
	int lp;
	if (!biff_types)
		read_types (BIFF_TYPES_FILE, &biff_types);
	/* Count backwars to give preference to non-filtered record types */
	for (lp=biff_types->len; --lp >= 0 ;) {
		GENERIC_TYPE *bt = g_ptr_array_index (biff_types, lp);
		if (bt->opcode>0xff) {
			if (bt->opcode == opcode)
				return bt->name;
		} else {
			if (bt->opcode == (opcode&0xff))
				return bt->name;
		}
	}
	return "Unknown";
}

static int
test (int argc, char *argv[])
{
	GsfInput  *input, *stream;
	GsfInfile *infile;
	GError    *err;
	int i;
	guint16 len, opcode;

	for (i = 1 ; i < argc ; i++) {
		fprintf( stderr, "%s\n",argv[i]);
		input = gsf_input_stdio_new (argv[i], &err);
		if (input == NULL) {

			g_return_val_if_fail (err != NULL, 1);

			g_warning ("'%s' error: %s\n", argv[i], err->message);
			g_error_free (err);
			continue;
		}

		infile = gsf_infile_msole_new (input, &err);
		if (infile == NULL) {

			g_return_val_if_fail (err != NULL, 1);

			g_warning ("'%s' Not an OLE file: %s\n", argv[i], err->message);
			g_error_free (err);
			continue;
		}

		stream = gsf_infile_child_by_name (infile, "Workbook");
		if (stream == NULL)
			stream = gsf_infile_child_by_name (infile, "Book");

		if (stream != NULL) {
			guint8 const *data;
			unsigned pos = gsf_input_tell (stream);

			while (NULL != (data = gsf_input_read (stream, 4))) {
				gboolean enable_dump = FALSE;

				opcode	= GSF_OLE_GET_GUINT16 (data);
				len	= GSF_OLE_GET_GUINT16 (data+2);

				if (len > 15000) {
					enable_dump = TRUE;
					g_warning ("Suspicious import of biff record > 15,000 (0x%x) for opcode 0x%hx",
						   len, opcode);
				} else if ((opcode & 0xff00) > 0x1000) {
					enable_dump = TRUE;
					g_warning ("Suspicious import of biff record with opcode 0x%hx",
						   opcode);
				}

				if (enable_dump)
					printf ("Opcode 0x%3hx : %15s, length 0x%hx (=%hd) @ pos = 0x%x (=%d)\n",
						opcode, get_biff_opcode_name (opcode),
						len, len, pos, pos);

				if (len > 0) {
					data = gsf_input_read (stream, len);
					if (data == NULL)
						break;
					if (enable_dump)
						gsf_mem_dump (data, MIN (len, 255));
				}
				pos = gsf_input_tell (stream);
			}
			g_object_unref (G_OBJECT (stream));
		}
		g_object_unref (G_OBJECT (infile));
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
