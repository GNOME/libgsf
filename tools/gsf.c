/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <gsf-config.h>
#include <gsf/gsf.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <locale.h>
#include <string.h>
#include <errno.h>

static gboolean show_version;
static int opt_zip64 = -1;

static GOptionEntry const gsf_options [] = {
	{
		"version", 'v',
		0, G_OPTION_ARG_NONE, &show_version,
		N_("Display program version"),
		NULL
	},

	/* All options below are for gsf testing only.  */
	{
		"zip64", 0,
		G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &opt_zip64,
		"",
		NULL
	},

	/* ---------------------------------------- */

	{ NULL, 0, 0, 0, NULL, NULL, NULL}
};

/* ------------------------------------------------------------------------- */

static GsfInfile *
open_archive (char const *filename)
{
	GsfInfile *infile;
	GError *error = NULL;
	GsfInput *src;
	char *display_name;

	src = gsf_input_stdio_new (filename, &error);
	if (error) {
		display_name = g_filename_display_name (filename);
		g_printerr (_("%s: Failed to open %s: %s\n"),
			    g_get_prgname (),
			    display_name,
			    error->message);
		g_free (display_name);
		return NULL;
	}

	infile = gsf_infile_zip_new (src, NULL);
	if (infile) {
		g_object_unref (src);
		return infile;
	}

	infile = gsf_infile_msole_new (src, NULL);
	if (infile) {
		g_object_unref (src);
		return infile;
	}

	infile = gsf_infile_tar_new (src, NULL);
	if (infile) {
		g_object_unref (src);
		return infile;
	}

	display_name = g_filename_display_name (filename);
	g_printerr (_("%s: Failed to recognize %s as an archive\n"),
		    g_get_prgname (),
		    display_name);
	g_free (display_name);

	g_object_unref (src);
	return NULL;
}

/* ------------------------------------------------------------------------- */

static GsfInput *
find_member (GsfInfile *arch, char const *name)
{
	char const *slash = strchr (name, '/');

	if (slash) {
		char *dirname = g_strndup (name, slash - name);
		GsfInput *member;
		GsfInfile *dir;

		member = gsf_infile_child_by_name (arch, dirname);
		g_free (dirname);
		if (!member)
			return NULL;
		dir = GSF_INFILE (member);
		member = find_member (dir, slash + 1);
		g_object_unref (dir);
		return member;
	} else {
		return gsf_infile_child_by_name (arch, name);
	}
}

/* ------------------------------------------------------------------------- */

static int
gsf_help (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	g_print (_("Available subcommands are...\n"));
	g_print (_("* cat        output one or more files in archive\n"));
	g_print (_("* dump       dump one or more files in archive as hex\n"));
	g_print (_("* help       list subcommands\n"));
	g_print (_("* list       list files in archive\n"));
	g_print (_("* listprops  list document properties in archive\n"));
	g_print (_("* props      print specified document properties\n"));
	g_print (_("* createole  create OLE archive\n"));
	g_print (_("* createzip  create ZIP archive\n"));
	return 0;
}

/* ------------------------------------------------------------------------- */

static void
ls_R (GsfInput *input, char const *prefix)
{
	char const *name = gsf_input_name (input);
	GsfInfile *infile = GSF_IS_INFILE (input) ? GSF_INFILE (input) : NULL;
	gboolean is_dir = infile && gsf_infile_num_children (infile) > 0;
	char *full_name;
	char *new_prefix;
	GDateTime *modtime = gsf_input_get_modtime (input);
	char *modtxt;

	if (prefix) {
		char *display_name = name ?
			g_filename_display_name (name)
			: g_strdup ("?");
		full_name = g_strconcat (prefix,
					 display_name,
					 NULL);
		new_prefix = g_strconcat (full_name, "/", NULL);
		g_free (display_name);
	} else {
		full_name = g_strdup ("*root*");
		new_prefix = g_strdup ("");
	}

	modtxt = modtime
		? g_date_time_format (modtime, "%F %H:%M:%S")
		: g_strdup ("                   ");

	g_print ("%c  %s  %10" GSF_OFF_T_FORMAT " %s\n",
		 (is_dir ? 'd' : 'f'),
		 modtxt,
		 gsf_input_size (input),
		 full_name);

	g_free (modtxt);

	if (is_dir) {
		int i;
		for (i = 0 ; i < gsf_infile_num_children (infile) ; i++) {
			GsfInput *child = gsf_infile_child_by_index (infile, i);
			/* We can get NULL here in case of file corruption.  */
			if (child) {
				ls_R (child, new_prefix);
				g_object_unref (child);
			}
		}
	}

	g_free (full_name);
	g_free (new_prefix);
}

static int
gsf_list (int argc, char **argv)
{
	int i;

	for (i = 0; i < argc; i++) {
		char const *filename = argv[i];
		char *display_name;
		GsfInfile *infile = open_archive (filename);
		if (!infile)
			return 1;

		if (i > 0)
			g_print ("\n");

		display_name = g_filename_display_name (filename);
		g_print ("%s:\n", display_name);
		g_free (display_name);

		ls_R (GSF_INPUT (infile), NULL);
		g_object_unref (infile);
	}

	return 0;
}

/* ------------------------------------------------------------------------- */

static int
gsf_dump (int argc, char **argv, gboolean hex)
{
	char const *filename;
	GsfInfile *infile;
	int i;
	int res = 0;

	if (argc < 2)
		return 1;

	filename = argv[0];
	infile = open_archive (filename);
	if (!infile)
		return 1;

	for (i = 1; i < argc; i++) {
		char const *name = argv[i];
		GsfInput *member = find_member (infile, name);
		if (!member) {
			char *display_name = g_filename_display_name (name);
			g_print ("%s: archive has no member %s\n",
				 g_get_prgname (), display_name);
			g_free (display_name);
			res = 1;
			break;
		}

		if (hex) {
			char *display_name = g_filename_display_name (name);
			g_print ("%s:\n", display_name);
			g_free (display_name);
		}
		gsf_input_dump (member, hex);
		g_object_unref (member);
	}

	g_object_unref (infile);
	return res;
}

static GsfDocMetaData *
get_meta_data (GsfInfile *infile, const char *filename)
{
	GsfDocMetaData *meta_data = gsf_doc_meta_data_new ();

	if (GSF_IS_INFILE_MSOLE (infile)) {
		GsfInput *in;
		GError *err;

		in = gsf_infile_child_by_name (infile, "\05SummaryInformation");
		if (NULL != in) {
			err = gsf_doc_meta_data_read_from_msole (meta_data, in);
			if (err != NULL) {
				g_warning ("'%s' error: %s", filename, err->message);
				g_error_free (err);
				err = NULL;
			}
			g_object_unref (G_OBJECT (in));
		}

		in = gsf_infile_child_by_name (infile, "\05DocumentSummaryInformation");
		if (NULL != in) {
			err = gsf_doc_meta_data_read_from_msole (meta_data, in);
			if (err != NULL) {
				g_warning ("'%s' error: %s", filename, err->message);
				g_error_free (err);
				err = NULL;
			}

			g_object_unref (G_OBJECT (in));
		}
	}

	return meta_data;
}

static int
gsf_dump_props (int argc, char **argv)
{
	GsfInfile *infile;
	GsfDocMetaData *meta_data;
	char const *filename;
	int res = 0;
	int i;

	if (argc < 2)
		return 1;

	filename = argv[0];
	infile = open_archive (filename);
	if (!infile)
		return 1;

	meta_data = get_meta_data (infile, filename);

	for (i = 1; i < argc; i++) {
		const char *name = argv[i];
		GsfDocProp const *prop =
			gsf_doc_meta_data_lookup (meta_data, name);
		if (prop) {
			if (argc > 2)
				g_print ("%s: ", name);
			gsf_doc_prop_dump (prop);
		} else {
			g_printerr (_("No property named %s\n"), name);
		}
	}

	g_object_unref (meta_data);
	g_object_unref (infile);
	return res;
}

static void
cb_collect_names (gpointer key,
		  G_GNUC_UNUSED gpointer value,
		  gpointer user)
{
	const char *name = key;
	GSList **names = user;

	*names = g_slist_prepend (*names, g_strdup (name));
}

static void
cb_print_names (const char *name)
{
	g_print ("%s\n", name);
}

static int
gsf_list_props (int argc, char **argv)
{
	GsfInfile *infile;
	GsfDocMetaData *meta_data;
	char const *filename;
	GSList *names = NULL;

	if (argc != 1)
		return 1;

	filename = argv[0];
	infile = open_archive (filename);
	if (!infile)
		return 1;

	meta_data = get_meta_data (infile, filename);
	gsf_doc_meta_data_foreach (meta_data, cb_collect_names, &names);
	names = g_slist_sort (names, (GCompareFunc)strcmp);
	g_slist_foreach (names, (GFunc)cb_print_names, NULL);
	g_slist_free (names);

	g_object_unref (meta_data);
	g_object_unref (infile);
	return 0;
}

/* ------------------------------------------------------------------------- */

static void
show_error (char const *name, GError *error)
{
	char *display_name;
	display_name = g_filename_display_name (name);
	g_printerr (_("%s: Error processing file %s: %s\n"),
		    g_get_prgname (),
		    display_name,
		    error->message);
	g_free (display_name);
}

/* Walks "path" directory structure while loading it in "outfile" */
static void
load_recursively (GsfOutfile *outfile, char const *path)
{
	GError *error = NULL;
	GStatBuf statbuf;

	if (g_stat (path, &statbuf) == -1) {
		g_printerr ("Failed to stat %s: %s\n",
			    path, g_strerror (errno));
		return;
	}

	if (S_ISDIR (statbuf.st_mode)) {
		GsfInfile *in = gsf_infile_stdio_new (path, &error);
		GsfOutfile *out;
		int i, n;
		char *base;

		if (!in) {
			show_error (path, error);
			return;
		}

		base = g_path_get_basename (path);
		out = GSF_OUTFILE (gsf_outfile_new_child (outfile, base, TRUE));
		g_free (base);

		n = gsf_infile_num_children (in);
		for (i = 0; i < n; i++) {
			char const *child = gsf_infile_name_by_index (in, i);
			char *name = g_build_filename (path, child, NULL);
			load_recursively (out, name);
			g_free (name);
		}

		g_object_unref (out);
		g_object_unref (in);
	} else if (S_ISREG (statbuf.st_mode)) {
		char *base;
		GsfInput *in;
		GsfOutput *out;
		gboolean ok;

		in = gsf_input_stdio_new (path, &error);
		if (!in) {
			show_error (path, error);
			return;
		}

		base = g_path_get_basename (path);
		out = gsf_outfile_new_child_full
			(outfile, base, FALSE,
			 "modtime", gsf_input_get_modtime (in),
			 NULL);
		g_printerr ("Adding %s\n", path);

		ok = gsf_input_copy (in, out);
		if (!ok)
			g_printerr ("Error in adding member.\n");

		ok = gsf_output_close (out);
		if (!ok)
			g_printerr ("Error in adding member (at close)\n");

		g_object_unref (out);
		g_free (base);


		g_object_unref (in);
	} else {
		g_printerr ("Ignoring %s\n", path);
	}
}

static int
gsf_create (int argc, char **argv, GType type)
{
	char const *filename;
	GError *error = NULL;
	GsfOutput *dest;
	GsfOutfile *outfile;
	int i;
	gboolean ok;

	if (argc < 2)
		return 1;

	filename = argv[0];
	if (strcmp (filename, "-") == 0)
		dest = gsf_output_stdio_new_FILE (filename, stdout, TRUE);
	else
		dest = gsf_output_stdio_new (filename, &error);
	if (error) {
		show_error (filename, error);
		return 1;
	}

	if (type == GSF_OUTFILE_MSOLE_TYPE)
		outfile = gsf_outfile_msole_new (dest);
	else if (type == GSF_OUTFILE_ZIP_TYPE) {
		outfile = g_object_new (GSF_OUTFILE_ZIP_TYPE,
					"sink", dest,
					"zip64", opt_zip64,
					NULL);
	} else
		g_assert_not_reached ();

	if (error) {
		show_error (filename, error);
		return 1;
	}

	for (i = 1; i < argc; i++) {
		GFile *file = g_file_new_for_commandline_arg (argv[i]);
		char *path = g_file_get_path (file);
		load_recursively (outfile, path);
		g_free (path);
		g_object_unref (file);
	}

	ok = gsf_output_close (GSF_OUTPUT (outfile));
	if (!ok)
		g_printerr ("Error while closing archive\n");

	g_object_unref (dest);
	g_object_unref (outfile);
	return 0;
}

/* ------------------------------------------------------------------------- */

int
main (int argc, char **argv)
{
	GOptionContext *ocontext;
	GError *error = NULL;
	char const *usage;
	char const *cmd;
	char const *me = (argv[0] ? argv[0] : "gsf");

	g_set_prgname (me);
	gsf_init ();

	bindtextdomain (GETTEXT_PACKAGE, GSFLOCALEDIR);
	textdomain (GETTEXT_PACKAGE);
	setlocale (LC_ALL, "");

	usage = _("SUBCOMMAND ARCHIVE...");
	ocontext = g_option_context_new (usage);
	g_option_context_add_main_entries (ocontext, gsf_options, GETTEXT_PACKAGE);
	g_option_context_parse (ocontext, &argc, &argv, &error);
	g_option_context_free (ocontext);

	if (error) {
		g_printerr (_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
			    error->message, me);
		g_error_free (error);
		return 1;
	}

	if (show_version) {
		g_print (_("gsf version %d.%d.%d\n"),
			 libgsf_major_version, libgsf_minor_version, libgsf_micro_version);
		return 0;
	}

	if (argc <= 1) {
		g_printerr (_("Usage: %s %s\n"), me, usage);
		return 1;
	}

	cmd = argv[1];

	if (strcmp (cmd, "help") == 0)
		return gsf_help (argc - 2, argv + 2);

	if (strcmp (cmd, "list") == 0 || strcmp (cmd, "l") == 0)
		return gsf_list (argc - 2, argv + 2);

	if (strcmp (cmd, "cat") == 0)
		return gsf_dump (argc - 2, argv + 2, FALSE);
	if (strcmp (cmd, "dump") == 0)
		return gsf_dump (argc - 2, argv + 2, TRUE);
	if (strcmp (cmd, "props") == 0)
		return gsf_dump_props (argc - 2, argv + 2);
	if (strcmp (cmd, "listprops") == 0)
		return gsf_list_props (argc - 2, argv + 2);
	if (strcmp (cmd, "createole") == 0)
		return gsf_create (argc - 2, argv + 2, GSF_OUTFILE_MSOLE_TYPE);
	if (strcmp (cmd, "createzip") == 0)
		return gsf_create (argc - 2, argv + 2, GSF_OUTFILE_ZIP_TYPE);

	g_printerr (_("Run '%s help' to see a list of subcommands.\n"), me);
	return 1;
}
