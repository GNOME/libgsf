/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * libgsf-regress.c: shared helpers for the libgsf regression tests
 */

#include "libgsf-regress.h"

#include <gsf/gsf-infile-tar.h>
#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf-outfile-zip.h>
#include <gsf/gsf-output-memory.h>

#include <stdio.h>
#include <stdlib.h>

static int nfail;

void
check (gboolean ok, char const *what, ...)
{
	va_list args;

	if (ok) {
		printf ("ok: ");
		va_start (args, what);
		vprintf (what, args);
		va_end (args);
		printf ("\n");
	} else {
		printf ("FAIL: ");
		va_start (args, what);
		vprintf (what, args);
		va_end (args);
		printf ("\n");
		nfail++;
	}
}

int
regress_exit_status (void)
{
	if (nfail) {
		printf ("*** %d check(s) failed\n", nfail);
		return 1;
	}
	printf ("all checks passed\n");
	return 0;
}

GsfInput *
regress_make_named_memory (char const *bytes, size_t len, char const *name)
{
	GsfInput *in = gsf_input_memory_new_clone ((guint8 const *)bytes, len);

	if (name != NULL)
		gsf_input_set_name (in, name);
	return in;
}

static GsfInput *
regress_make_zip_bytes (char const *name1, char const *content1,
			char const *name2, char const *content2)
{
	GsfOutput *mem = gsf_output_memory_new ();
	GsfOutfile *zip = gsf_outfile_zip_new (mem, NULL);
	GsfOutput *child;
	guint8 const *bytes;
	gsf_off_t size;

	child = gsf_outfile_new_child (zip, name1, FALSE);
	gsf_output_write (child, strlen (content1), (guint8 const *)content1);
	gsf_output_close (child);
	g_object_unref (child);

	child = gsf_outfile_new_child (zip, name2, FALSE);
	gsf_output_write (child, strlen (content2), (guint8 const *)content2);
	gsf_output_close (child);
	g_object_unref (child);

	gsf_output_close (GSF_OUTPUT (zip));
	g_object_unref (zip);

	bytes = gsf_output_memory_get_bytes (GSF_OUTPUT_MEMORY (mem));
	size = gsf_output_size (mem);
	{
		GsfInput *in = gsf_input_memory_new_clone (bytes, size);
		g_object_unref (mem);
		return in;
	}
}

GsfInput *
regress_make_zip_input (void)
{
	return regress_make_zip_bytes ("aaa", "hello", "bbb", "world");
}

/* Header field offsets and sizes from the ustar layout. */
#define REGRESS_TAR_BLOCK 512

static void
regress_tar_add_full (GByteArray *b, char const *name, char typeflag,
		      char const *content)
{
	guint8 h[REGRESS_TAR_BLOCK];
	unsigned i, sum = 0;
	size_t len = strlen (content);

	memset (h, 0, sizeof h);
	strncpy ((char *)h, name, 99);
	strcpy ((char *)h + 100, "0000644");		/* mode */
	strcpy ((char *)h + 108, "0000000");		/* uid */
	strcpy ((char *)h + 116, "0000000");		/* gid */
	sprintf ((char *)h + 124, "%011o", (unsigned)len);
	h[135] = 0;
	strcpy ((char *)h + 136, "00000000000");	/* mtime */
	h[147] = 0;
	memset (h + 148, ' ', 8);			/* chksum placeholder */
	h[156] = typeflag;
	strcpy ((char *)h + 257, "ustar");
	h[263] = '0';
	h[264] = '0';
	for (i = 0; i < REGRESS_TAR_BLOCK; i++)
		sum += h[i];
	sprintf ((char *)h + 148, "%06o", sum);
	h[154] = 0;
	h[155] = ' ';

	g_byte_array_append (b, h, sizeof h);
	g_byte_array_append (b, (guint8 const *)content, len);
	while (len % REGRESS_TAR_BLOCK != 0) {
		guint8 pad = 0;
		g_byte_array_append (b, &pad, 1);
		len++;
	}
}

void
regress_tar_add (GByteArray *b, char const *name, char const *content)
{
	regress_tar_add_full (b, name, '0', content);
}

void
regress_tar_add_longname (GByteArray *b, char const *realname,
			  char const *content)
{
	/* The 'L' entry's name is the magic marker; its data holds the
	 * real name. */
	regress_tar_add_full (b, "././@LongLink", 'L', realname);
	regress_tar_add (b, "short-name-placeholder", content);
}

GsfInput *
regress_make_tar_input (void)
{
	GByteArray *b = g_byte_array_new ();
	guint8 zeros[1024];
	GsfInput *input;

	regress_tar_add (b, "aaa", "hello");
	regress_tar_add (b, "bbb", "world");
	memset (zeros, 0, sizeof zeros);
	g_byte_array_append (b, zeros, sizeof zeros);

	input = gsf_input_memory_new_clone (b->data, b->len);
	g_byte_array_free (b, TRUE);
	return input;
}

void
regress_fill_pattern (guint8 *buf, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		buf[i] = (guint8)((i * 31 + (i >> 9)) & 0xFF);
}
