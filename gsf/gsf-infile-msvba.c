/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-msole.c :
 *
 * Copyright (C) 2002 Michael Meeks (michael@ximian.com)
 *		      Jody Goldberg (jody@gnome.org)
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

/* Info extracted from
	svx/source/msfilter/msvbasic.cxx
	http://calgary.virusbtn.com/vb2000/Programme/papers/bontchev.pdf
 */
#include <gsf-config.h>
#include <gsf/gsf-infile-impl.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-infile-msvba.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-utils.h>

#include <stdio.h>

typedef struct {
	char	 *name;
	guint32   offset;
} MSVBADirent;

struct _GsfInfileMSVBA {
	GsfInfile parent;

	GsfInfile *source;
	GList *children;
};

typedef struct {
	GsfInfileClass  parent_class;
} GsfInfileMSVBAClass;

#define GSF_INFILE_MSVBA_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GSF_INFILE_MSVBA_TYPE, GsfInfileMSVBAClass))
#define GSF_IS_INFILE_MSVBA_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GSF_INFILE_MSVBA_TYPE))

#define VBA_DIR_HEADER_SIZE	(2 + /* magic */	\
				 4 + /* ? version ? */	\
				 2 + /* 0x00 0xff */	\
				 28) /* misc */
#define VBA_COMPRESSION_WINDOW 4096

static gboolean
vba_inflate (GsfInput *input, off_t offset)
{
	unsigned	i, win_pos, pos = 0;
	unsigned	mask, shift, distance;
	guint8		flag, buffer [VBA_COMPRESSION_WINDOW];
	guint8 const   *tmp;
	guint16		token, len;
	gboolean	clean = TRUE;

	if (gsf_input_seek (input, offset+3, GSF_SEEK_SET))
		return TRUE;

	/* The first byte is a flag byte.  Each bit in this byte
	 * determines what the next byte is.  If the bit is zero,
	 * the next byte is a character.  Otherwise the  next two
	 * bytes contain the number of characters to copy from the
	 * umcompresed buffer and where to copy them from (offset,
	 * length).
	 */
	while (NULL != gsf_input_read (input, 1, &flag))
		for (mask = 1; mask < 0x100 ; mask <<= 1)
			if (flag & mask) {
				if (NULL == (tmp = gsf_input_read (input, 2, NULL)))
					break;
				token = GSF_OLE_GET_GUINT16 (tmp);

				clean = TRUE;

				win_pos = pos % VBA_COMPRESSION_WINDOW;
				if (win_pos <= 0x10)
					shift = 12;
				else if (win_pos <= 0x20)
					shift = 11;
				else if (win_pos <= 0x40)
					shift = 10;
				else if (win_pos <= 0x80)
					shift = 9;
				else if (win_pos <= 0x100)
					shift = 8;
				else if (win_pos <= 0x200)
					shift = 7;
				else if (win_pos <= 0x400)
					shift = 6;
				else if (win_pos <= 0x800)
					shift = 5;
				else
					shift = 4;

				len = (token & ((1 << shift) - 1)) + 3;
				distance = token >> shift;

				/*
				 * read the len of data from the history, wrapping around the
				 * VBA_COMPRESSION_WINDOW boundary if necessary
				 * data read from the history is also copied into the recent
				 * part of the history as well.
				 */
				for (i = 0; i < len; i++) {
					unsigned srcpos = (pos - distance - 1) % VBA_COMPRESSION_WINDOW;
					guint8 c = buffer [srcpos];
					buffer [pos++ % VBA_COMPRESSION_WINDOW] = c;
				}
			} else {
				if ((pos != 0) && ((pos % VBA_COMPRESSION_WINDOW) == 0) && clean) {
					(void) gsf_input_read (input, 2, NULL);
					clean = FALSE;
					fwrite (buffer, 1, VBA_COMPRESSION_WINDOW, stdout);
					break;
				}
				if (NULL != gsf_input_read (input, 1, buffer + (pos % VBA_COMPRESSION_WINDOW)))
					pos++;
				clean = TRUE;
			}

	if (pos%VBA_COMPRESSION_WINDOW)
		fwrite (buffer, 1, pos % VBA_COMPRESSION_WINDOW, stdout);
	return pos;
}

/**
 * vba_init_info :
 * @vba :
 * @err : optionally NULL
 *
 * Read an VBA dirctory and its project file.
 * along the way.
 *
 * Return value: TRUE on error setting @err if it is supplied.
 **/
static gboolean
vba_init_info (GsfInfileMSVBA *vba, GError **err)
{
	/* NOTE : This seems constant, find some confirmation */
	static guint8 const signature[]	  = { 0xcc, 0x61 };
	static struct {
		guint8 const signature[4];
		char const * const name;
		int const vba_version;
		gboolean const is_mac;
	} const  versions [] = {
		{ { 0x5e, 0x00, 0x00, 0x01 }, "Office 97",		5, FALSE },
		{ { 0x65, 0x00, 0x00, 0x01 }, "Office 2000 alpha?",	6, FALSE },
		{ { 0x6b, 0x00, 0x00, 0x01 }, "Office 2000 beta?",	6, FALSE },
		{ { 0x6d, 0x00, 0x00, 0x01 }, "Office 2000",		6, FALSE },
		{ { 0x70, 0x00, 0x00, 0x01 }, "Office XP beta 1/2",	6, FALSE },
		{ { 0x73, 0x00, 0x00, 0x01 }, "Office XP",		6, FALSE },
		{ { 0x60, 0x00, 0x00, 0x0e }, "MacOffice 98",		5, TRUE },
		{ { 0x62, 0x00, 0x00, 0x0e }, "MacOffice 2001",		5, TRUE }
	};

	guint8 const *header;
	GsfInput *dir;
	unsigned i;

	dir = gsf_infile_child_by_name (vba->source, "dir");
	if (dir == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"Can't find the VBA directory stream.");
		return TRUE;
	}

	vba_inflate (dir, 0);

	dir = gsf_infile_child_by_name (vba->source, "_VBA_PROJECT");
	if (dir == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"Can't find the VBA directory stream.");
		return TRUE;
	}
	if (NULL == (header = gsf_input_read (dir, VBA_DIR_HEADER_SIZE, NULL)) ||
	    0 != memcmp (header, signature, sizeof (signature))) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"No VBA signature");
		return TRUE;
	}

	for (i = 0 ; i < G_N_ELEMENTS (versions); i++)
		if (!memcmp (header+2, versions[i].signature, 4))
			break;

	if (i >= G_N_ELEMENTS (versions)) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"Unknown VBA version signature 0x%x%x%x%x",
				header[2], header[3], header[4], header[5]);
		return TRUE;
	}

	return FALSE;
}

static void
vba_dirent_free (MSVBADirent *data, gpointer ignore)
{
	(void) ignore;
	g_free (data->name);
	g_free (data);
}

static void
gsf_infile_msvba_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (obj);

	if (vba->source != NULL) {
		g_object_unref (G_OBJECT (vba->source));
		vba->source = NULL;
	}
	if (vba->children != NULL) {
		g_list_foreach (vba->children, (GFunc) vba_dirent_free, NULL);
		g_list_free (vba->children);
		vba->children = NULL;
	}

	parent_class = g_type_class_peek (GSF_INFILE_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static GsfInput *
gsf_infile_msvba_dup (GsfInput *src_input, GError **err)
{
	GsfInfileMSVBA const *src = GSF_INFILE_MSVBA (src_input);
	GsfInfileMSVBA *dst = NULL;

	return GSF_INPUT (dst);
}

static guint8 const *
gsf_infile_msvba_read (GsfInput *input, size_t num_bytes, guint8 *buffer)
{
	return NULL;
}

static gboolean
gsf_infile_msvba_seek (GsfInput *input, off_t offset, GsfOff_t whence)
{
	(void)input; (void)offset; (void)whence;
	return FALSE;
}

static GsfInput *
gsf_infile_msvba_new_child (GsfInfileMSVBA *parent, MSVBADirent *dirent)
{
	GsfInputMemory *child = NULL;

	return GSF_INPUT (child);
}

static GsfInput *
gsf_infile_msvba_child_by_index (GsfInfile *infile, int target)
{
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (infile);
	GList *p;

	for (p = vba->children; p != NULL ; p = p->next)
		if (target-- <= 0)
			return gsf_infile_msvba_new_child (vba, p->data);
	return NULL;
}

static char const *
gsf_infile_msvba_name_by_index (GsfInfile *infile, int target)
{
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (infile);
	GList *p;

	for (p = vba->children; p != NULL ; p = p->next)
		if (target-- <= 0)
			return ((MSVBADirent *)p->data)->name;
	return NULL;
}

static GsfInput *
gsf_infile_msvba_child_by_name (GsfInfile *infile, char const *name)
{
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (infile);
	GList *p;

	for (p = vba->children; p != NULL ; p = p->next) {
		MSVBADirent *dirent = p->data;
		if (dirent->name != NULL && !strcmp (name, dirent->name))
			return gsf_infile_msvba_new_child (vba, dirent);
	}
	return NULL;
}

static int
gsf_infile_msvba_num_children (GsfInfile *infile)
{
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (infile);
	g_return_val_if_fail (vba != NULL, 0);
	return g_list_length (vba->children);
}

static void
gsf_infile_msvba_init (GObject *obj)
{
	GsfInfileMSVBA *vba = GSF_INFILE_MSVBA (obj);

	vba->source		= NULL;
	vba->children		= NULL;
}

static void
gsf_infile_msvba_class_init (GObjectClass *gobject_class)
{
	GsfInputClass  *input_class  = GSF_INPUT_CLASS (gobject_class);
	GsfInfileClass *infile_class = GSF_INFILE_CLASS (gobject_class);

	gobject_class->finalize		= gsf_infile_msvba_finalize;
	input_class->Dup		= gsf_infile_msvba_dup;
	input_class->Read		= gsf_infile_msvba_read;
	input_class->Seek		= gsf_infile_msvba_seek;
	infile_class->num_children	= gsf_infile_msvba_num_children;
	infile_class->name_by_index	= gsf_infile_msvba_name_by_index;
	infile_class->child_by_index	= gsf_infile_msvba_child_by_index;
	infile_class->child_by_name	= gsf_infile_msvba_child_by_name;
}

GSF_CLASS (GsfInfileMSVBA, gsf_infile_msvba,
	   gsf_infile_msvba_class_init, gsf_infile_msvba_init,
	   GSF_INFILE_TYPE)

GsfInfile *
gsf_infile_msvba_new (GsfInfile *source, GError **err)
{
	GsfInfileMSVBA *vba;

	g_return_val_if_fail (GSF_IS_INFILE (source), NULL);

	vba = g_object_new (GSF_INFILE_MSVBA_TYPE, NULL);
	g_object_ref (G_OBJECT (source));
	vba->source = source;
	gsf_input_set_size (GSF_INPUT (vba), 0);

	if (vba_init_info (vba, err)) {
		g_object_unref (G_OBJECT (vba));
		return NULL;
	}

	return GSF_INFILE (vba);
}
