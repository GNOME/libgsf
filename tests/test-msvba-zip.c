#include <stdio.h>
#include <string.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-output-stdio.h>

gboolean compress = FALSE;

#define VBA_COMPRESSION_WINDOW 4096

#define HEADER_SIZE 3

static void
copy_header (GsfInput *input, GsfOutput *output)
{
	gboolean err = FALSE;
	guint8 data[HEADER_SIZE];

	err |= !gsf_input_read (input, HEADER_SIZE, data);
	err |= !gsf_output_write (output, HEADER_SIZE, data);
}

/* Brute force and ugliness ! */
typedef struct {
	guint8  inblock[VBA_COMPRESSION_WINDOW];
	guint   length;
	guint   pos;
	guint8  shift;

	guint8     mask;
	GString   *outstr;

	GsfOutput *output;
} CompressBuf;

#define noDEBUG

static gint
find_match (CompressBuf *buf, guint pos, guint *len)
{
	guint i;
	
	for (i = 0; i < pos; i++) {
		guint j;

		for (j = 0; j < buf->length - pos && j < pos; j++)
			if (buf->inblock[pos + j] != buf->inblock[i + j])
				break;

		if (j >= 3) {
			*len = j;
#ifdef DEBUG		
			fprintf (stderr, "Match at %d\n", i);
#endif
			return i;
		}
	}
	return -1;
}

static void
output_data (CompressBuf *buf, guint8 *data, gboolean compressed)
{
	if (compressed) {
		buf->mask |= 1 << buf->shift;
		g_string_append_c (buf->outstr, data [0]);
		g_string_append_c (buf->outstr, data [1]);
	} else
		g_string_append_c (buf->outstr, data [0]);

	buf->shift++;
	if (buf->shift == 8) {
		guint i;

		gsf_output_write (buf->output, 1, &buf->mask);
		gsf_output_write (buf->output, buf->outstr->len, buf->outstr->str);

#ifdef DEBUG		
		fprintf (stderr, "Block: 0x%x\n'", buf->mask);
		for (i = 0; i < buf->outstr->len; i++)
			fprintf (stderr, "%c", buf->outstr->str[i] >= 0x20 ? buf->outstr->str[i] : '.');
		fprintf (stderr, "'\n");
#endif

		buf->mask = 0;
		buf->shift = 0;
		g_string_set_size (buf->outstr, 0);
	}
}

static void
output_match (CompressBuf *buf, guint cur_pos, guint pos, guint len)
{
	int shift, token, distance;
	guint8 data[2];

	if (cur_pos <= 0x80) {
		if (cur_pos <= 0x20)
			shift = (cur_pos <= 0x10) ? 12 : 11;
		else
			shift = (cur_pos <= 0x40) ? 10 : 9;
	} else {
		if (cur_pos <= 0x200)
			shift = (cur_pos <= 0x100) ? 8 : 7;
		else if (cur_pos <= 0x800)
			shift = (cur_pos <= 0x400) ? 6 : 5;
		else
			shift = 4;
	}

	distance = cur_pos - pos - 1;

#ifdef DEBUG		
	fprintf (stderr, "distance %d [0x%x - %d], len %d\n", distance, cur_pos, pos, len);
	if (cur_pos + len >= (1<<shift))
		fprintf (stderr, "Overlaps boundary\n");
#endif
	
	token = (distance << shift) + ((len - 3) & ((1<<shift)-1));
	data[0] = token & 0xff;
	data[1] = token >> 8;

	output_data (buf, data, TRUE);
}

static void
compress_block (CompressBuf *buf)
{
	guint pos, len;
	gint  match;

	for (pos = 0; pos < buf->length;) {
		if ((match = find_match (buf, pos, &len)) >= 0) {
			output_match (buf, pos, match, len);
			pos += len;
		} else
			output_data (buf, &(buf->inblock[pos++]), FALSE);
	}
}

static void
do_compress (GsfInput *input, GsfOutput *output)
{
	CompressBuf real_buf, *buf;
	GString *string;

	copy_header (input, output);

	string = g_string_sized_new (64);

	buf = &real_buf;
	memset (buf, 0, sizeof (CompressBuf));
	buf->output = output;
	buf->outstr = g_string_sized_new (20);

	while (gsf_input_remaining (input) > 0) {
		buf->length = MIN (gsf_input_remaining (input), VBA_COMPRESSION_WINDOW);
		if (!gsf_input_read (input, buf->length, buf->inblock))
			g_error ("Failed to read %d bytes\n", buf->length);
		compress_block (buf);
	}

       	if (buf->outstr->len) {
		gsf_output_write (buf->output, 1, &buf->mask);
		gsf_output_write (buf->output, buf->outstr->len, buf->outstr->str);
	}
}

static void
do_decompress (GsfInput *input, GsfOutput *output)
{
	gboolean err = FALSE;
	guint8   data[HEADER_SIZE];
	guint8  *decompressed;
	int      size;

	err |= !gsf_input_read (input, HEADER_SIZE, data);
	err |= !gsf_output_write (output, HEADER_SIZE, data);

	decompressed = gsf_msole_inflate (input, 3, &size);
	if (!decompressed)
		fprintf (stderr, "Failed to decompress\n");

	err |= !gsf_output_write (output, size, decompressed);

	if (err)
		fprintf (stderr, "I/O error\n");
}

int
main (int argc, char *argv[])
{
	int i;
	const char *src = NULL;
	const char *dest = NULL;
	GsfInput *input;
	GsfOutput *output;
	GError   *error = NULL;

	gsf_init ();

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-' &&
		    argv[i][1] == 'c')
			compress = TRUE;
		else if (!src)
			src = argv[i];
		else
			dest = argv[i];
	}
	if (!src || !dest) {
		fprintf (stderr, "%s: [-c(ompress)] <infile> <outfile>\n", argv[0]);
		return 1;
	}

	input = GSF_INPUT (gsf_input_stdio_new (src, &error));
	output = GSF_OUTPUT (gsf_output_stdio_new (dest, &error));
	if (!input || !output) {
		fprintf (stderr, "Failed to open input(%p)/output(%p): '%s'\n",
			 input, output, error ? error->message : "<NoMsg>");
		return 1;
	}

	if (compress)
		do_compress (input, output);
	else
		do_decompress (input, output);

	g_object_unref (input);
	g_object_unref (output);

	gsf_shutdown ();

	return 0;
}
