/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-trans.c
 *
 * Copyright (C) 2003 Dom Lachowicz (cinamod@hotmail.com)
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

#include <stdio.h>
#include <string.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-output-transaction.h>

#define STRING_1 "This is my very first string!\n"
#define STRING_2 "This is the second string in the file\n"
#define STRING_3 "This should never appear in the file\n"

static void
updated_cb (GsfOutput * output)
{
	g_print ("Updated: %s\n", gsf_output_name (output));
}

static void
aborted_cb (GsfOutput * output)
{
	g_print ("Aborted: %s\n", gsf_output_name (output));
}

static void
committed_cb (GsfOutput * output)
{
	g_print ("Committed: %s\n", gsf_output_name (output));
}

static void
connect_signals (GsfOutput * output)
{
	g_signal_connect (G_OBJECT (output), "updated",
			  G_CALLBACK (updated_cb), NULL);
	g_signal_connect (G_OBJECT (output), "aborted",
			  G_CALLBACK (aborted_cb), NULL);
	g_signal_connect (G_OBJECT (output), "committed",
			  G_CALLBACK (committed_cb), NULL);
}

static int
test (int argc, char *argv[])
{
	GsfOutput *output;
	GsfOutput *trans;
	GError   *err;

	if (argc != 2) {
		fprintf (stderr, "Usage : %s outfile\n", argv[0]);
		return 1;
	}

	output = GSF_OUTPUT (gsf_output_stdio_new (argv[1], &err));
	if (output == NULL) {
		g_return_val_if_fail (err != NULL, 1);

		g_warning ("'%s' error: %s", argv[1], err->message);
		g_error_free (err);
		return 1;
	}

	/* should write STRING_1 to the file */
	trans = GSF_OUTPUT (gsf_output_transaction_new_named (output, "Trans 1"));
	connect_signals (trans);
	gsf_output_write (trans, strlen (STRING_1), STRING_1);
	gsf_output_transaction_commit (GSF_OUTPUT_TRANSACTION (trans));
	g_object_unref (G_OBJECT (trans));

	/* should write strings 1&2 to the file */
	trans = GSF_OUTPUT (gsf_output_transaction_new_named (output, "Trans 2"));
	connect_signals (trans);
	gsf_output_write (trans, strlen (STRING_1), STRING_1);
	gsf_output_write (trans, strlen (STRING_2), STRING_2);
	gsf_output_transaction_commit (GSF_OUTPUT_TRANSACTION (trans));
	g_object_unref (G_OBJECT (trans));

	/* should not write anything to the file */
	trans = GSF_OUTPUT (gsf_output_transaction_new_named (output, "Trans 3"));
	connect_signals (trans);
	gsf_output_write (trans, strlen (STRING_1), STRING_1);
	gsf_output_transaction_abort (GSF_OUTPUT_TRANSACTION (trans));
	g_object_unref (G_OBJECT (trans));

	gsf_output_close (output);
	g_object_unref (G_OBJECT (output));

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
