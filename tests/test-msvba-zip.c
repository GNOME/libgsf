#include <stdio.h>
#include <string.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-output-stdio.h>

gboolean compress = FALSE;

#define HEADER_SIZE 3

static void
do_compress (GsfInput *input, GsfOutput *output)
{
	guint i;
	guint blocks, slack;
	gboolean err = FALSE;
	guint8 data[HEADER_SIZE];

	err |= !gsf_input_read (input, HEADER_SIZE, data);
	err |= !gsf_output_write (output, HEADER_SIZE, data);

	blocks = (gsf_input_size (input) - HEADER_SIZE) / 8;
	slack  = gsf_input_size (input) - HEADER_SIZE - blocks * 8;

	for (i = 0; i < blocks; i++) {
		guint8 data[9];
		memset (data, 0, sizeof (data));
		err |= !gsf_input_read (input, 8, &data[1]);
		err |= !gsf_output_write (output, 9, data);
	}
	if (slack) {
		guint8 data[9];
		memset (data, 0, sizeof (data));
		err |= !gsf_input_read (input, slack, &data[1]);
		err |= !gsf_output_write (output, slack + 1, data);
	}
	if (err)
		fprintf (stderr, "I/O error\n");
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
