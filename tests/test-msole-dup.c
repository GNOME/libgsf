#include <stdbool.h>
#include <unistd.h>
#include <gsf/gsf.h>
#include <gsf/gsf-input.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-output.h>
#include <gsf/gsf-outfile.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-outfile-msole.h>

int
main (int argc, char **argv)
{
	GsfOutput *out, *outchild;
	GsfOutfile *outf;
	GsfInput *in, *inchild, *indup;
	GsfInfile *inf;
	gsize size;

	(void)argc;
	(void)argv;

	out = gsf_output_stdio_new("teststg", NULL);
	outf = gsf_outfile_msole_new(out);
	outchild = gsf_outfile_new_child(outf, "small", false);
	gsf_output_puts(outchild, "test\n");
	gsf_output_close(outchild);
	g_object_unref(G_OBJECT(outchild));
	gsf_output_close(GSF_OUTPUT(outf));
	g_object_unref(G_OBJECT(outf));
	g_object_unref(G_OBJECT(out));

	in = gsf_input_stdio_new("teststg", NULL);
	inf = gsf_infile_msole_new(in, NULL);
	inchild = gsf_infile_child_by_name(inf, "small");
	indup = gsf_input_dup(inchild, NULL);
	size = gsf_input_size(inchild);

	write(1, gsf_input_read(indup, size, NULL), size);
	g_object_unref(G_OBJECT(indup));
	g_object_unref(G_OBJECT(inchild));
	g_object_unref(G_OBJECT(inf));
	g_object_unref(G_OBJECT(in));

	return 0;
}
