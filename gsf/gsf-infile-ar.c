/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-ar.c :
 *
 * Copyright (C) 2004 Dom Lachowicz (cinamod@hotmail.com)
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
 *
 * Code and ideas adapted from:
 *     GsfInfileZip (C) Jody Goldberg, Tambet Ingo
 *     LibComprex (http://www.gnupdate.org/components/libcomprex/index.xml)
 */

/*** TODO *** DOES COMPILE. DOES NOT WORK *** TODO ***/

#include <gsf-config.h>
#include <gsf/gsf-infile-impl.h>
#include <gsf/gsf-infile-ar.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-utils.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "libgsf:ar"

/***************************************************************/
/***************************************************************/

static GObjectClass *parent_class;

typedef struct {	
	char                 *name;
	size_t                usize;
	gsf_off_t             offset;
	gsf_off_t             data_offset;
} ArDirent;

typedef struct {
	char *name;
	gboolean is_directory;
	ArDirent *dirent;
	GSList *children;
} ArVDir;

typedef struct {
	GList  *dirent_list;
	gint    entries; /* short for g_list_length (dirent_list) */
	ArVDir *vdir;
	gint    ref_count;
} ArInfo;

#define AR_MAGIC          "!<arch>\n"
#define AR_FMAG           "`\n"
#define AR_MAGIC_LEN      8
#define AR_HEADER_LEN     60
#define AR_NAME_SEPARATOR '/'

typedef struct
{
	char name[16];
	char date[12];
	char uid[6];
	char gid[6];
	char mode[8];
	char size[10];
	char fmag[2];
} ArHeader;

typedef enum {
	AR_SUCCESS,
	AR_FAILURE,
	AR_EOF
} ArStatus;

/***************************************************************/
/***************************************************************/

struct _GsfInfileAr {
	GsfInfile parent;

	GsfInput  *input;
	ArInfo	  *info;

	ArVDir   *vdir;

	guint8   *buf;
	size_t    buf_size;
	gsf_off_t seek_skipped;
};

typedef struct {
	GsfInfileClass  parent_class;
} GsfInfileArClass;

/***************************************************************/
/***************************************************************/

/* Doesn't do much, but include for symmetry */
static ArDirent*
ar_dirent_new (void)
{
	return g_new0 (ArDirent, 1);
}

static void
ar_dirent_free (ArDirent *dirent)
{
	g_return_if_fail (dirent != NULL);

	if (dirent->name != NULL) {
		g_free (dirent->name);
		dirent->name = NULL;
	}
	g_free (dirent);
}

static ArVDir *
ar_vdir_new (char const *name, gboolean is_directory, ArDirent *dirent)
{
	ArVDir *vdir = g_new (ArVDir, 1);

	vdir->name = g_strdup (name);
	vdir->is_directory = is_directory;
	vdir->dirent = dirent;
	vdir->children = NULL;
	return vdir;
}

static void
ar_vdir_free (ArVDir *vdir, gboolean free_dirent)
{
	GSList *l;

	if (!vdir)
		return;

	for (l = vdir->children; l; l = l->next)
		ar_vdir_free ((ArVDir *)l->data, free_dirent);

	g_slist_free (vdir->children);
	g_free (vdir->name);
	if (free_dirent && vdir->dirent)
		ar_dirent_free (vdir->dirent);
	g_free (vdir);
}

/* Comparison doesn't have to be UTF-8 safe, as long as it is consistent */
static gint
ar_vdir_compare (gconstpointer ap, gconstpointer bp)
{
	ArVDir *a = (ArVDir *) ap;
	ArVDir *b = (ArVDir *) bp;

	if (!a || !b) {
		if (!a && !b)
			return 0;
		else
			return a ? -1 : 1;
	}
	return strcmp (a->name, b->name);
}

static void
ar_vdir_add_child (ArVDir *vdir, ArVDir *child)
{
	vdir->children = g_slist_insert_sorted (vdir->children,
						(gpointer) child,
						ar_vdir_compare);
}

static ArInfo *
ar_info_ref (ArInfo *info)
{
	info->ref_count++;
	return info;
}

static void
ar_info_unref (ArInfo *info)
{
	GList *p;

	if (info->ref_count-- != 1)
		return;

	vdir_free (info->vdir, FALSE);
	for (p = info->dirent_list; p != NULL; p = p->next)
		ar_dirent_free ((ArDirent *) p->data);

	g_list_free (info->dirent_list);

	g_free (info);
}

static ArVDir *
ar_vdir_child_by_name (ArVDir *vdir, char const * name)
{
	GSList *l;
	ArVDir *child;
	
	for (l = vdir->children; l; l = l->next) {
		child = (ArVDir *) l->data;
		if (strcmp (child->name, name) == 0)
			return child;
	}
	return NULL;
}

static ArVDir *
ar_vdir_child_by_index (ArVDir *vdir, int target)
{
	GSList *l;
	
	for (l = vdir->children; l; l = l->next)
		if (target-- <= 0)
			return (ArVDir *) l->data;
	return NULL;
}

static void
ar_vdir_insert (ArVDir *vdir, char const * name, ArDirent *dirent)
{
	const char *p;
	char *dirname;
	ArVDir *child;
	
	p = strchr (name, AR_NAME_SEPARATOR);
	if (p) {	/* A directory */
		dirname = g_strndup (name, (gsize) (p - name));
		child = ar_vdir_child_by_name (vdir, dirname);
		if (!child) {
			child = ar_vdir_new (dirname, TRUE, NULL);
			ar_vdir_add_child (vdir, child);
		}
		g_free (dirname);
		if (*(p+1) != '\0') {
			name = p+1;
			ar_vdir_insert (child, name, dirent);
		}
	} else { /* A simple file name */
		child = ar_vdir_new (name, FALSE, dirent);
		ar_vdir_add_child (vdir, child);
	}
}

/**
 * ar_dup :
 * @src :
 *
 * Return value: the partial duplicate.
 **/
static GsfInfileAr *
ar_dup (GsfInfileAr const *src)
{
	GsfInfileAr	*dst;

	g_return_val_if_fail (src != NULL, NULL);

	dst = g_object_new (GSF_INFILE_AR_TYPE, NULL);
	dst->input = gsf_input_dup (src->input, NULL);
	dst->info  = ar_info_ref (src->info);

	return dst;
}

/***************************************************************/

/* ASCII Decimal to Integer */
static gint
ar_dec_to_int(const gchar *dec)
{
	gint i;
	sscanf(dec, "%d", &i);
	return i;
}

static ArStatus
ar_validate_magic(GsfInput *fp)
{
	char magic[AR_MAGIC_LEN];

	if (gsf_input_remaining (fp) <= AR_MAGIC)
		return AR_EOF;

	gsf_input_read (fp, AR_MAGIC, magic);

	if (strncmp (magic, AR_MAGIC, AR_MAGIC_LEN))
		return AR_FAILURE;

	return AR_SUCCESS;
}

static ArStatus
ar_read_header(GsfInput *fp, ArHeader *header)
{
	size_t len;

	if (fp == NULL || header == NULL)
		return AR_FAILURE;
	
	memset(header, 0, AR_HEADER_LEN);

	len = gsf_input_remaining (fp);
	if (len == 0)
		return AR_EOF;
	else if (len < AR_HEADER_LEN)
		return AR_FAILURE;
	else {
		gsf_input_read (fp, AR_HEADER_LEN, header);
		if (strncmp (header->fmag, AR_FMAG, 2))
			return AR_FAILURE;
		return AR_SUCCESS;
	}
}

/***************************************************************/

static guint8 const *
gsf_infile_ar_read (GsfInput *input, size_t num_bytes, guint8 *buffer)
{
	return NULL;
}

static gboolean
gsf_infile_ar_seek (GsfInput *input, gsf_off_t offset, GSeekType whence)
{
	return FALSE;
}

static GsfInput *
gsf_infile_ar_dup (GsfInput *src_input, GError **err)
{
	GsfInfileAr const *src = GSF_INFILE_AR (src_input);
	GsfInfileAr *dst = ar_dup (src);

	if (dst == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
					    "Something went wrong in ar_dup.");
		return NULL;
	}

	dst->vdir = src->vdir;

	if (dst->vdir->dirent)
		if (ar_child_init (dst) != FALSE) {
			g_object_unref (dst);
			if (err != NULL)
				*err = g_error_new (gsf_input_error (), 0,
						    "Something went wrong in ar_child_init.");
			return NULL;
		}

	return GSF_INPUT (dst);
}

static GsfInput *
gsf_infile_ar_new_child (GsfInfileAr *parent, ArVDir *vdir, GError **err)
{
	return NULL;
}

static GsfInput *
gsf_infile_ar_child_by_index (GsfInfile *infile, int target, GError **err)
{
	GsfInfileAr *ar = GSF_INFILE_AR (infile);
	ArVDir *child_vdir = ar_vdir_child_by_index (ar->vdir, target);

	if (child_vdir)
		return gsf_infile_ar_new_child (ar, child_vdir, err);

	return NULL;
}

static char const *
gsf_infile_ar_name_by_index (GsfInfile *infile, int target)
{
	GsfInfileAr *ar = GSF_INFILE_AR (infile);
	ArVDir *child_vdir = ar_vdir_child_by_index (ar->vdir, target);

	if (child_vdir)
		return child_vdir->name;

	return NULL;
}

static GsfInput *
gsf_infile_ar_child_by_name (GsfInfile *infile, char const *name, GError **err)
{
	GsfInfileAr *ar = GSF_INFILE_AR (infile);
	ArVDir *child_vdir = ar_vdir_child_by_name (ar->vdir, name);

	if (child_vdir)
		return gsf_infile_ar_new_child (ar, child_vdir, err);

	return NULL;
}

static int
gsf_infile_ar_num_children (GsfInfile *infile)
{
	GsfInfileAr *ar = GSF_INFILE_AR (infile);

	g_return_val_if_fail (ar->vdir != NULL, -1);

	if (!ar->vdir->is_directory)
		return -1;
	return g_slist_length (ar->vdir->children);
}

static void
gsf_infile_ar_finalize (GObject *obj)
{
	GsfInfileAr *ar = GSF_INFILE_AR (obj);

	if (ar->input != NULL) {
		g_object_unref (G_OBJECT (ar->input));
		ar->input = NULL;
	}
	g_free (ar->buf);
	parent_class->finalize (obj);
}

static void
gsf_infile_ar_init (GObject *obj)
{
	GsfInfileAr *ar = (GsfInfileAr *)obj;
	ar->input       = NULL;
	ar->vdir        = NULL;
	ar->buf         = NULL;	
	ar->buf_size    = 0;
}

static void
gsf_infile_ar_class_init (GObjectClass *gobject_class)
{
	GsfInputClass  *input_class  = GSF_INPUT_CLASS (gobject_class);
	GsfInfileClass *infile_class = GSF_INFILE_CLASS (gobject_class);

	gobject_class->finalize		= gsf_infile_ar_finalize;
	input_class->Dup		= gsf_infile_ar_dup;
	input_class->Read		= gsf_infile_ar_read;
	input_class->Seek		= gsf_infile_ar_seek;
	infile_class->num_children	= gsf_infile_ar_num_children;
	infile_class->name_by_index	= gsf_infile_ar_name_by_index;
	infile_class->child_by_index	= gsf_infile_ar_child_by_index;
	infile_class->child_by_name	= gsf_infile_ar_child_by_name;

	parent_class = g_type_class_peek_parent (gobject_class);
}

GSF_CLASS (GsfInfileAr, gsf_infile_ar,
	   gsf_infile_ar_class_init, gsf_infile_ar_init,
	   GSF_INFILE_TYPE)

/**
 * gsf_infile_ar_new :
 * @source :
 * @err   :
 *
 * Opens the root directory of a Ar file.
 * NOTE : adds a reference to @source
 *
 * Returns : the new AR file handler
 **/
GsfInfile *
gsf_infile_ar_new (GsfInput *source, GError **err)
{
	GsfInfileAr *ar;

	g_return_val_if_fail (GSF_IS_INPUT (source), NULL);

	ar = g_object_new (GSF_INFILE_AR_TYPE, NULL);
	g_object_ref (G_OBJECT (source));
	ar->input = source;
	gsf_input_set_size (GSF_INPUT (ar), (gsf_off_t) 0);

	if (ar_init_info (ar, err)) {
		g_object_unref (G_OBJECT (ar));
		return NULL;
	}
	ar->vdir = ar->info->vdir;

	return ar;
}
