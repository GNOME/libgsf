/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-stdio.c: stdio based output
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

#include <gsf-config.h>
#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-output-impl.h>
#include <gsf/gsf-impl-utils.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

struct _GsfOutputStdio {
	GsfOutput output;

	FILE     *file;
	char	 *real_filename, *temp_filename;
	gboolean  create_backup_copy;
	struct stat st;
};

typedef struct {
	GsfOutputClass output_class;
} GsfOutputStdioClass;

#define GSF_MAX_LINK_LEVEL 256
#ifdef MAXPATHLEN
#define GSF_MAX_PATH_LEN  MAXPATHLEN
#elif defined (PATH_MAX)
#define GSF_MAX_PATH_LEN  PATH_MAX
#else
#define GSF_MAX_PATH_LEN  2048
#endif

/* Does readlink() recursively until we find a real filename. */
static char *
follow_symlinks (char const *filename, GError **error)
{
	gchar *followed_filename;
	gint link_count;

	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (strlen (filename) + 1 <= GSF_MAX_PATH_LEN, NULL);

	followed_filename = g_strdup (filename);
	link_count = 0;

	while (link_count < GSF_MAX_LINK_LEVEL) {
		struct stat st;

		if (lstat (followed_filename, &st) != 0)
			/* We could not access the file, so perhaps it does not
			 * exist.  Return this as a real name so that we can
			 * attempt to create the file.
			 */
			return followed_filename;

		if (S_ISLNK (st.st_mode)) {
			gint len;
			gchar linkname[GSF_MAX_PATH_LEN];

			link_count++;

			len = readlink (followed_filename, linkname, GSF_MAX_PATH_LEN - 1);

			if (len == -1) {
				g_set_error (error, gsf_output_error_id (), errno,
					     "Could not read symbolic link information "
					       "for %s", followed_filename);
				g_free (followed_filename);
				return NULL;
			}

			linkname[len] = '\0';

			/* If the linkname is not an absolute path name, append
			 * it to the directory name of the followed filename.  E.g.
			 * we may have /foo/bar/baz.lnk -> eek.txt, which really
			 * is /foo/bar/eek.txt.
			 */

			if (linkname[0] != G_DIR_SEPARATOR)
			{
				gchar *slashpos;
				gchar *tmp;

				slashpos = strrchr (followed_filename, G_DIR_SEPARATOR);

				if (slashpos)
					*slashpos = '\0';
				else
				{
					tmp = g_strconcat ("./", followed_filename, NULL);
					g_free (followed_filename);
					followed_filename = tmp;
				}

				tmp = g_build_filename (followed_filename, linkname, NULL);
				g_free (followed_filename);
				followed_filename = tmp;
			}
			else
			{
				g_free (followed_filename);
				followed_filename = g_strdup (linkname);
			}
		} else
			return followed_filename;
	}

	/* Too many symlinks */
	g_set_error (error, gsf_output_error_id (), ELOOP,
		     "The file has too many symbolic links.");

	return NULL;
}

#if 0
		switch (errno)
		{
			case ENOSPC:
				msg = "There is not enough disk space to save the file.\n"
					"Please free some disk space and try again.";
				break;

			case EFBIG:
				msg = "The disk where you are trying to save the file has "
					"a limitation on file sizes.  Please try saving "
					"a smaller file or saving it to a disk that does not "
					"have this limitation.";
#endif
/**
 * gsf_output_stdio_new :
 * @filename : in utf8.
 * @err	     : optionally NULL.
 *
 * Returns a new file or NULL.
 **/
GsfOutput *
gsf_output_stdio_new (char const *filename, GError **err)
{
	GsfOutputStdio *stdio;
	FILE *file = NULL;
	char *slashpos, *dirname, *temp_filename = NULL;
	char *real_filename = follow_symlinks (filename, err);
	int fd;
	gboolean create_backup_copy = TRUE;
	mode_t saved_umask;
	struct stat st;
	
	if (real_filename == NULL)
		return NULL;

	/* Get the directory in which the real filename lives */
	slashpos = strrchr (real_filename, G_DIR_SEPARATOR);
	if (slashpos) {
		dirname = g_strdup (real_filename);
		dirname[slashpos - real_filename + 1] = '\0';
	} else
		dirname = g_strdup (".");

	/* If there is not an existing file with that name, compute the
	 * permissions and uid/gid that we will use for the newly-created file.
	 */
	if (stat (real_filename, &st) != 0) {
		struct stat dir_st;
		int result;

		/* File does not exist? */
		create_backup_copy = FALSE;

		/* Use default permissions */
		st.st_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		st.st_uid = getuid ();

		result = stat (dirname, &dir_st);

		if (result == 0 && (dir_st.st_mode & S_ISGID))
			st.st_gid = dir_st.st_gid;
		else
			st.st_gid = getgid ();
	}

	/* Save to a temporary file.  We set the umask because some (buggy)
	 * implementations of mkstemp() use permissions 0666 and we want 0600.
	 */
	temp_filename = g_build_filename (dirname, ".gsf-save-XXXXXX", NULL);
	g_free (dirname);
	saved_umask = umask (0077);
	fd = g_mkstemp (temp_filename); /* this modifies temp_filename to the used name */
	umask (saved_umask);

	if (fd < 0 || NULL == (file = fdopen (fd, "w"))) {
		if (err != NULL)
			*err = g_error_new (gsf_output_error_id (), errno,
				"%s: %s", temp_filename, g_strerror (errno));
		goto failure;
	}

	stdio = g_object_new (GSF_OUTPUT_STDIO_TYPE, NULL);
	stdio->file = file;
	stdio->st = st;
	stdio->create_backup_copy = create_backup_copy;
	stdio->real_filename = real_filename;
	stdio->temp_filename = temp_filename;

	return GSF_OUTPUT (stdio);

failure :
	g_free (temp_filename);
	g_free (real_filename);
	return NULL;
}

static gboolean
gsf_output_stdio_close (GsfOutput *output)
{
	GsfOutputStdio *stdio = GSF_OUTPUT_STDIO (output);
	gboolean res;
	char *backup_filename = NULL;

	if (stdio->file == NULL)
		return FALSE;

	res = (0 == fclose (stdio->file));
	stdio->file = NULL;
	if (!res) {
		gsf_output_set_error (output, errno, " ");
		unlink (stdio->temp_filename);
		return FALSE;
	}

	/* Move the original file to a backup */
	if (stdio->create_backup_copy) {
		gint result;
		backup_filename = g_strconcat (stdio->real_filename, ".bak", NULL);
		result = rename (stdio->real_filename, backup_filename);
		if (result != 0) {
			gsf_output_set_error (output, errno,
				"Could not backup the original as %s.",
				backup_filename);
			g_free (backup_filename);
			unlink (stdio->temp_filename);
			return FALSE;
		}
	}

	/* Move the temp file to the original file */
	if (rename (stdio->temp_filename, stdio->real_filename) != 0) {
		gint saved_errno = errno;
		if (backup_filename != NULL &&
		    rename (backup_filename, stdio->real_filename) != 0)
			saved_errno = errno;
		res = gsf_output_set_error (output, errno, g_strerror (errno));
	} else {
		/* Restore permissions.  There is not much error checking we
		 * can do here, I'm afraid.  The final data is saved anyways.
		 */
		chmod (stdio->real_filename, stdio->st.st_mode);
		chown (stdio->real_filename, stdio->st.st_uid, stdio->st.st_gid);
	}

	g_free (backup_filename);

	return res;
}

static void
gsf_output_stdio_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfOutput	*output = (GsfOutput *)obj;
	GsfOutputStdio	*stdio = GSF_OUTPUT_STDIO (output);

	if (!gsf_output_is_closed (output))
		gsf_output_close (output);

	g_free (stdio->real_filename);
	stdio->real_filename = NULL;
	g_free (stdio->temp_filename);
	stdio->temp_filename = NULL;

	parent_class = g_type_class_peek (GSF_OUTPUT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}


static gboolean
gsf_output_stdio_seek (GsfOutput *output, gsf_off_t offset, GSeekType whence)
{
	GsfOutputStdio const *stdio = GSF_OUTPUT_STDIO (output);
	int stdio_whence = 0;	/* make compiler shut up */
	long loffset;

	g_return_val_if_fail (stdio->file != NULL, 
		gsf_output_set_error (output, 0, "missing file"));

	loffset = offset;
	if ((gsf_off_t) loffset != offset) { /* Check for overflow */
		g_warning ("offset too large for fseek");
		return gsf_output_set_error (output, 0, "offset too large for fseek");
	}
	switch (whence) {
	case G_SEEK_SET : stdio_whence = SEEK_SET;	break;
	case G_SEEK_CUR : stdio_whence = SEEK_CUR;	break;
	case G_SEEK_END : stdio_whence = SEEK_END;	break;
	default :
		break; /*checked in GsfOutput wrapper */
	}

	if (0 == fseek (stdio->file, loffset, SEEK_END))
		return TRUE;
	return gsf_output_set_error (output, errno,
		g_strerror (errno));
}

static gboolean
gsf_output_stdio_write (GsfOutput *output,
			size_t num_bytes,
			guint8 const *buffer)
{
	GsfOutputStdio *stdio = GSF_OUTPUT_STDIO (output);
	size_t res;

	g_return_val_if_fail (stdio != NULL, FALSE);
	g_return_val_if_fail (stdio->file != NULL, FALSE);

	res = fwrite (buffer, 1, num_bytes, stdio->file);
	return res == num_bytes;
}

static void
gsf_output_stdio_init (GObject *obj)
{
	GsfOutputStdio *stdio = GSF_OUTPUT_STDIO (obj);

	stdio->file = NULL;
	stdio->create_backup_copy = FALSE;
}

static void
gsf_output_stdio_class_init (GObjectClass *gobject_class)
{
	GsfOutputClass *output_class = GSF_OUTPUT_CLASS (gobject_class);

	gobject_class->finalize = gsf_output_stdio_finalize;
	output_class->Close	= gsf_output_stdio_close;
	output_class->Seek	= gsf_output_stdio_seek;
	output_class->Write	= gsf_output_stdio_write;
}

GSF_CLASS (GsfOutputStdio, gsf_output_stdio,
	   gsf_output_stdio_class_init, gsf_output_stdio_init, GSF_OUTPUT_TYPE)
