/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-stdio.c: stdio based output
 *
 * Copyright (C) 2002-2004 Jody Goldberg (jody@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-output-impl.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-utils.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>


#ifdef G_OS_WIN32

#ifndef S_IRUSR
#define S_IRUSR 04
#define S_IWUSR 02
#define W_OK    00  /* existance mode only */
typedef int mode_t;
#endif

#define S_IRGRP S_IRUSR
#define S_IROTH S_IRUSR
#define S_ISGID 0

#define getuid() 0
#define getgid() 0
#define chown(filename, uid, gid) 0

#ifdef HAVE_IO_H
#include <io.h>
#endif

#endif /* G_OS_WIN32 */

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
#ifdef HAVE_READLINK
	gchar *followed_filename;
	gint link_count;

	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (strlen (filename) + 1 <= GSF_MAX_PATH_LEN, NULL);

	followed_filename = g_strdup (filename);
	link_count = 0;

	while (link_count++ < GSF_MAX_LINK_LEVEL) {
		char linkname[GSF_MAX_PATH_LEN];
		int len = readlink (followed_filename, linkname, GSF_MAX_PATH_LEN - 1);

		if (len == -1) {
			switch (errno) {
			case EINVAL: /* Not a symlink.  */
			case ENOSYS: /* Surely not a symlink.  */
			case ENOENT: /* No such file.  */
				return followed_filename;

			default:
				if (error)
					*error = g_error_new (gsf_output_error_id (), errno,
							      g_strerror (errno));
				g_free (followed_filename);
				return NULL;
			}
		}

		linkname[len] = '\0';

		if (g_path_is_absolute (linkname)) {
			g_free (followed_filename);
			followed_filename = g_strdup (linkname);
		} else {
			/*
			 * If the linkname is not an absolute path name, append
			 * it to the directory name of the followed filename.  E.g.
			 * we may have /foo/bar/baz.lnk -> eek.txt, which really
			 * is /foo/bar/eek.txt.
			 */

			char *dir = g_path_get_dirname (followed_filename);

			g_free (followed_filename);
			followed_filename = g_build_filename (dir, linkname, NULL);
			g_free (dir);
		}
	}

	/* Too many symlinks */
	if (error)
		*error = g_error_new (gsf_output_error_id (), ELOOP,
				      g_strerror (ELOOP));

	return NULL;
#else  /* !HAVE_READLINK */

	return g_strdup (filename);

#endif /* !HAVE_READLINK */
}

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
	char *dirname = NULL;
	char *temp_filename = NULL;
	char *real_filename = follow_symlinks (filename, err);
	int fd;
	mode_t saved_umask;
	struct stat st;

	if (real_filename == NULL)
		goto failure;

	/* Get the directory in which the real filename lives */
	dirname = g_path_get_dirname (real_filename);

	if (stat (real_filename, &st) == 0) {
		/* FIXME: use eaccess if available.  */
		/* FIXME? Race conditions en masse.  */
		if (access (real_filename, W_OK) != 0) {
			if (err != NULL)
				*err = g_error_new (gsf_output_error_id (), errno,
						    g_strerror (errno));
			goto failure;
		}
	} else {
		/*
		 * File does not exist.  Compute the permissions and uid/gid
		 * that we will use for the newly-created file.
		 */

		struct stat dir_st;
		int result;

		/* Use default permissions */
		st.st_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		st.st_uid = getuid ();

		/* Used to set create_backup_copy = FALSE, but
		   creating backup copies on this level is a 
		   horrible mistake. -DAL- */

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
	/* Oh, joy.  What about threads?  --MW */
	saved_umask = umask (0077);
	fd = g_mkstemp (temp_filename); /* this modifies temp_filename to the used name */
	umask (saved_umask);

	if (fd < 0 || NULL == (file = fdopen (fd, "wb"))) {
		if (err != NULL)
			*err = g_error_new (gsf_output_error_id (), errno,
					    g_strerror (errno));
		goto failure;
	}

	stdio = g_object_new (GSF_OUTPUT_STDIO_TYPE, NULL);
	stdio->file = file;
	stdio->st = st;
	stdio->create_backup_copy = FALSE;
	stdio->real_filename = real_filename;
	stdio->temp_filename = temp_filename;
	gsf_output_set_name (GSF_OUTPUT (stdio), filename);

	g_free (dirname);

	return GSF_OUTPUT (stdio);

failure :
	g_free (temp_filename);
	g_free (real_filename);
	g_free (dirname);
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
		gsf_output_set_error (output, errno,
				      "Failed to close temporary file.");
		unlink (stdio->temp_filename);
		return FALSE;
	}

	/* Move the original file to a backup */
	if (stdio->create_backup_copy) {
		gint result;
		backup_filename = g_strconcat (stdio->real_filename, ".bak", NULL);
		result = rename (stdio->real_filename, backup_filename);
		if (result != 0) {
			char *utf8name = gsf_filename_to_utf8 (backup_filename, TRUE);
			gsf_output_set_error (output, errno,
				"Could not backup the original as %s.",
				utf8name);
			g_free (utf8name);
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
		 * Note the order: mode, uid+gid, gid, uid, mode.
		 */
		chmod (stdio->real_filename, stdio->st.st_mode);
		if (chown (stdio->real_filename,
			   stdio->st.st_uid,
			   stdio->st.st_gid)) {
			/* We cannot set both.  Maybe we can set one.  */
			chown (stdio->real_filename, -1, stdio->st.st_gid);
			chown (stdio->real_filename, stdio->st.st_uid, -1);
		}
		chmod (stdio->real_filename, stdio->st.st_mode);
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

#ifndef HAVE_FSEEKO
	long loffset;
#else
	off_t loffset;
#endif

	g_return_val_if_fail (stdio->file != NULL, 
			      gsf_output_set_error (output, 0, "missing file"));

	loffset = offset;
	if ((gsf_off_t) loffset != offset) { /* Check for overflow */
#ifdef HAVE_FSEEKO
		g_warning ("offset too large for fseeko");
		return gsf_output_set_error (output, 0, "offset too large for fseeko");
#else
		g_warning ("offset too large for fseek");
		return gsf_output_set_error (output, 0, "offset too large for fseek");
#endif
	}
	switch (whence) {
	case G_SEEK_SET : stdio_whence = SEEK_SET;	break;
	case G_SEEK_CUR : stdio_whence = SEEK_CUR;	break;
	case G_SEEK_END : stdio_whence = SEEK_END;	break;
	default :
		break; /*checked in GsfOutput wrapper */
	}

	errno = 0;
#ifdef HAVE_FSEEKO
	if (0 == fseeko (stdio->file, loffset, stdio_whence))
		return TRUE;
#else
	if (0 == fseek (stdio->file, loffset, stdio_whence))
		return TRUE;
#endif
	return gsf_output_set_error (output, errno,
		g_strerror (errno));
}

static gboolean
gsf_output_stdio_write (GsfOutput *output,
			size_t num_bytes,
			guint8 const *buffer)
{
	GsfOutputStdio *stdio = GSF_OUTPUT_STDIO (output);
	size_t written, remaining;

	g_return_val_if_fail (stdio != NULL, FALSE);
	g_return_val_if_fail (stdio->file != NULL, FALSE);

	remaining = num_bytes;

	while (remaining > 0) {
		written = fwrite (buffer + (num_bytes - remaining), 1, 
				  remaining, stdio->file);
		if ((written < remaining) && ferror (stdio->file) != 0)
			return gsf_output_set_error (output, errno, g_strerror (errno));

		remaining -= written;
	}
	return TRUE;
}

static gboolean
gsf_output_stdio_vprintf (GsfOutput *output, char const *fmt, va_list args)
{
	GsfOutputStdio *stdio = (GsfOutputStdio *)output;
	return vfprintf (stdio->file, fmt, args) >= 0;
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
	output_class->Vprintf	= gsf_output_stdio_vprintf;
}

GSF_CLASS (GsfOutputStdio, gsf_output_stdio,
	   gsf_output_stdio_class_init, gsf_output_stdio_init, GSF_OUTPUT_TYPE)
