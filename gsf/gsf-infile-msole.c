/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-msole.c :
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
#include <gsf/gsf-infile-impl.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-utils.h>

#include <string.h>
#include <stdio.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "libgsf:msole"

typedef struct {
	char	 *name;
	char	 *collation_name;
	int	  index;
	int	  size;
	gboolean  use_sb;
	guint32   first_block;
	gboolean  is_directory;
	GList	 *children;
} MSOleDirent;

typedef struct {
	guint32 *bat, num_bat;

	struct {
		unsigned shift;
		unsigned filter;
		unsigned size;
	} bb, sb;
	int max_block;
	guint32 threshold; /* transition between small and big blocks */
        guint32 sbat_start;
	guint32 rootdir_start;

	MSOleDirent *root_dir;
	GsfInfileMSOle *sb_file;

	int ref_count;
} MSOleInfo;

struct _GsfInfileMSOle {
	GsfInfile parent;

	GsfInput  *input;
	MSOleInfo *info;

	MSOleDirent *dirent;

	guint32	*bat, num_blocks;

	struct {
		guint8  *buf;
		int      buf_size;
	} stream;
};

typedef struct {
	GsfInfileClass  parent_class;
} GsfInfileMSOleClass;

#define GSF_INFILE_MSOLE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GSF_INFILE_MSOLE_TYPE, GsfInfileMSOleClass))
#define IS_GSF_INFILE_MSOLE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GSF_INFILE_MSOLE_TYPE))

#define OLE_HEADER_SIZE		0x200	/* always 0x200 no mater what the big block size is */
#define OLE_HEADER_START_BAT	0x4c
#define BAT_INDEX_SIZE		4

#define DIRENTRY_SIZE		0x80
#define DIRENTRY_MAX_NAME_SIZE	0x40
#define DIRENTRY_TYPE_DIR	1
#define DIRENTRY_TYPE_FILE	2
#define DIRENTRY_TYPE_ROOTDIR	5
#define DIRENT_MAGIC_END	0xffffffff

/* flags in the block allocation list to denote special blocks */
#define BAT_MAGIC_UNUSED	0xffffffff	/*		 -1 */
#define BAT_MAGIC_END_OF_CHAIN	0xfffffffe	/*		 -2 */
#define BAT_MAGIC_BAT		0xfffffffd	/* a bat block,  -3 */
#define BAT_MAGIC_XBAT		0xfffffffc	/* an xbat block -4 */

/* utility macros */
#define OLE_BIG_BLOCK(index, ole)	((index) >> ole->info->bb.shift)
#define OLE_SMALL_BLOCK(index, ole)	((index) >> ole->info->sb_shift)

/**
 * ole_get_block :
 * @ole : the infile
 *
 * Read a block of data from the underlying input.
 * Be really anal.
 **/
static guint8 const *
ole_get_block (GsfInfileMSOle *ole, int block)
{
	g_return_val_if_fail (block >= 0, NULL);
	g_return_val_if_fail (block < ole->info->max_block, NULL);

	if (gsf_input_seek (ole->input,
		OLE_HEADER_SIZE + (block << ole->info->bb.shift),
		GSF_SEEK_SET) < 0)
		return NULL;

	return gsf_input_read (ole->input, ole->info->bb.size);
}

/**
 * ole_info_make_bat :
 *
 * Walk the linked list of the global block allocation table and build up a
 * table for just the supplied file.
 */
static guint32 *
ole_info_make_bat (MSOleInfo const *info, guint32 block, guint32 size, guint32 *num_blocks)
{
	/* NOTE : Only use size as a suggestion, some times it is wrong */
	GArray *bat = g_array_sized_new (FALSE, FALSE, sizeof (guint32),
		1 + (size >> info->bb.shift));

	do {
		g_array_append_val (bat, block);
		block = info->bat [block];
	} while (block < BAT_MAGIC_XBAT);

	g_return_val_if_fail (block == BAT_MAGIC_END_OF_CHAIN, NULL);

	*num_blocks = bat->len;
	return (guint32 *) g_array_free (bat, FALSE);
}

static gint
ole_dirent_cmp (MSOleDirent const *a, MSOleDirent const *b)
{
	g_return_val_if_fail (a, 0);
	g_return_val_if_fail (b, 0);

	g_return_val_if_fail (a->collation_name, 0);
	g_return_val_if_fail (b->collation_name, 0);

	return strcmp (b->collation_name, a->collation_name);
}

/**
 * ole_dirent_new :
 * @ole    :
 * @entry  :
 * @parent : optional
 *
 * Parse dirent number @entry and recursively handle its siblings and children.
 **/
static MSOleDirent *
ole_dirent_new (GsfInfileMSOle *ole, guint32 entry, MSOleDirent *parent)
{
	MSOleDirent *dirent;
	guint32 block, item, next, prev, child, size;
	guint8 const *data;
	guint8 type;
	guint16 name_len;

	if (entry >= DIRENT_MAGIC_END)
		return NULL;

	/* we need to bootstrap the root entry.
	 * first load it to get the size, then build the bat list
	 * so that we can easily read the rest
	 */
	if (entry == 0) {
		block = ole->info->rootdir_start;
		item = 0;
	} else {
		block = OLE_BIG_BLOCK (entry * DIRENTRY_SIZE, ole);

		g_return_val_if_fail (block >= 0, NULL);
		g_return_val_if_fail (block < ole->num_blocks, NULL);

		block = ole->bat [block];
		item = entry % (ole->info->bb.size / DIRENTRY_SIZE);
	};

	data = ole_get_block (ole, block);
	if (data == NULL)
		return NULL;
	data += DIRENTRY_SIZE * item;

	size = GSF_GET_GUINT32 (data + 0x78);
	g_return_val_if_fail (size <= (guint32)ole->input->size, NULL);

	type = GSF_GET_GUINT8 (data + 0x42);
	if (type == DIRENTRY_TYPE_ROOTDIR)
		ole->bat = ole_info_make_bat (ole->info, block, size, &ole->num_blocks);
	else if (type != DIRENTRY_TYPE_DIR && type != DIRENTRY_TYPE_FILE) {
		g_warning ("Unknown stream type 0x%x", type);
		return NULL;
	}

	dirent = g_new0 (MSOleDirent, 1);
	dirent->index	     = entry;
	dirent->size	     = size;
	/* root dir is always big block */
	dirent->use_sb	     = parent && (size < ole->info->threshold);
	dirent->first_block  = (GSF_GET_GUINT32 (data + 0x74));
	dirent->is_directory = (type != DIRENTRY_TYPE_FILE);
	dirent->children     = NULL;
	prev  = GSF_GET_GUINT32 (data + 0x44),
	next  = GSF_GET_GUINT32 (data + 0x48),
	child = GSF_GET_GUINT32 (data + 0x4c);
	name_len = GSF_GET_GUINT16 (data + 0x40);
	if (0 < name_len && name_len <= DIRENTRY_MAX_NAME_SIZE) {
		gunichar2 uni_name [DIRENTRY_MAX_NAME_SIZE];
		int i;

		/* be wary about endianness */
		for (i = 0 ; i < name_len ; i += 2)
			uni_name [i/2] = GSF_GET_GUINT16 (data + i);

		dirent->name = g_utf16_to_utf8 (uni_name, -1, NULL, NULL, NULL);
	} else
		dirent->name = g_strdup ("");
	dirent->collation_name = g_utf8_collate_key (dirent->name, -1);

#if 1
	printf ("%c '%s' :\tsize = %d\tfirst_block = 0x%x\n",
		dirent->is_directory ? 'd' : ' ',
		dirent->name, dirent->size, dirent->first_block);
#endif

	if (parent != NULL)
		parent->children = g_list_insert_sorted (parent->children,
			dirent, (GCompareFunc)ole_dirent_cmp);

	/* NOTE : These links are a tree, not a linked list */
	ole_dirent_new (ole, prev, parent); 
	ole_dirent_new (ole, next, parent); 

	if (dirent->is_directory)
		ole_dirent_new (ole, child, dirent);
	else if (child != DIRENT_MAGIC_END)
		g_warning ("A non directory stream with children ?");

	return dirent;
}

static void
ole_dirent_free (MSOleDirent *dirent)
{
	g_return_if_fail (dirent != NULL);

	g_free (dirent->name);
	g_free (dirent->collation_name);

	g_list_foreach (dirent->children, (GFunc)ole_dirent_free, NULL);
}

/*****************************************************************************/
static void
ole_info_unref (MSOleInfo *info)
{
	if (info->ref_count-- != 1)
		return;

	if (info->bat != NULL) {
		g_free (info->bat);
		info->bat = NULL;
	}
	if (info->root_dir != NULL) {
		ole_dirent_free (info->root_dir);
		info->root_dir = NULL;
	}
	if (info->sb_file != NULL)  {
		g_object_unref (G_OBJECT (info->sb_file));
		info->sb_file = NULL;
	}
	g_free (info);
}

static MSOleInfo *
ole_info_ref (MSOleInfo *info)
{
	info->ref_count++;
	return info;
}

/**
 * ole_info_read_xbat :
 *
 * A small utility routine to read a set of references to bat blocks
 * either from the OLE header, or an xbat block.
 **/
static guint32 *
ole_info_read_xbat (GsfInfileMSOle *ole, guint32 *bats,
		    guint8 const *xbat, guint8 const *xbat_end)
{
	int i = 0;
	guint8 const *bat;

	for (; xbat < xbat_end; xbat += BAT_INDEX_SIZE, i++) {
		bat = ole_get_block (ole, GSF_GET_GUINT32 (xbat));
		if (bat == NULL)
			return NULL;
		memcpy (bats, bat, ole->info->bb.size);
		bats += (ole->info->bb.size / BAT_INDEX_SIZE);
	}
	return bats;
}

/**
 * ole_init_info :
 * @ole :
 * @err : optionally NULL
 *
 * Read an OLE header and do some sanity checking
 * along the way.
 *
 * Return value: TRUE on error setting @err if it is supplied.
 **/
static gboolean
ole_init_info (GsfInfileMSOle *ole, GError **err)
{
	static guint8 const signature[] =
		{ 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };
	guint8 const *header, *tmp;
	guint8 *xbat;
	MSOleInfo *info;
	int bb_shift, num_bat, num_xbat, last;
	guint32 xbat_block, *ptr;

	/* check the header */
	if (gsf_input_seek (ole->input, 0, GSF_SEEK_SET) ||
	    NULL == (header = gsf_input_read (ole->input, OLE_HEADER_SIZE)) ||
	    0 != memcmp (header, signature, sizeof (signature))) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"No OLE2 signature");
		return TRUE;
	}

	bb_shift   = GSF_GET_GUINT16 (header + 0x1e);
	num_bat    = GSF_GET_GUINT32 (header + 0x2c);
        xbat_block = GSF_GET_GUINT32 (header + 0x44);
	num_xbat   = GSF_GET_GUINT32 (header + 0x48);

	/* Some sanity checks
	 * 1) There should always be at least 1 BAT block
	 * 2) It makes no sense to have a black larger than 2^31 for now.
	 *    Maybe relax this later, but not much.
	 */
	if (num_bat <= 0 || 6 > bb_shift || bb_shift >= 31 ||
	    GSF_GET_GUINT16 (header + 0x20) >= 31) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"Unreasonable block sizes");
		return TRUE;
	}

	info = g_new0 (MSOleInfo, 1);
	ole->info = info;

	info->ref_count	     = 1;
	info->bb.shift	     = bb_shift;
	info->bb.size	     = 1 << info->bb.shift;
	info->bb.filter	     = info->bb.size - 1;
	info->sb.shift	     = GSF_GET_GUINT16 (header + 0x20);
	info->sb.size	     = 1 << info->sb.shift;
	info->sb.filter	     = info->sb.size - 1;
	info->threshold	     = GSF_GET_GUINT32 (header + 0x38);
        info->sbat_start     = GSF_GET_GUINT32 (header + 0x3c);
	info->max_block	     = (gsf_input_size (ole->input) - OLE_HEADER_SIZE) / info->bb.size;
	info->rootdir_start  = GSF_GET_GUINT32 (header + 0x30);
	info->sb_file	     = NULL;

	info->num_bat 	= num_bat * (info->bb.size / BAT_INDEX_SIZE);
	info->bat	= g_new0 (guint32, info->num_bat);

	xbat = g_alloca (MAX (info->bb.size, OLE_HEADER_SIZE));

	/* Read Block allocation references in the header */
	memcpy (xbat, header + OLE_HEADER_START_BAT,
		OLE_HEADER_SIZE - OLE_HEADER_START_BAT);
	last = num_bat;
	if (last > (OLE_HEADER_SIZE - OLE_HEADER_START_BAT)/BAT_INDEX_SIZE)
		last = (OLE_HEADER_SIZE - OLE_HEADER_START_BAT)/BAT_INDEX_SIZE;

	ptr = ole_info_read_xbat (ole, info->bat,
		xbat, xbat + last*BAT_INDEX_SIZE);
	num_bat -= last;

	last = (info->bb.size - BAT_INDEX_SIZE) / BAT_INDEX_SIZE;
	while (ptr != NULL && num_xbat-- > 0) {
		tmp = ole_get_block (ole, xbat_block);
		if (tmp == NULL) {
			ptr = NULL;
			break;
		}
		memcpy (xbat, tmp, info->bb.size);

		if (num_xbat == 0) {
			if (last < num_bat) {
				/* there should be less that a full xbat block
				 * remaining */
				ptr = NULL;
				break;
			}
			last = num_bat;
		} else if (num_xbat > 0) {
			xbat_block = GSF_GET_GUINT32 (xbat + last * BAT_INDEX_SIZE);
			num_bat -= last;
		}

		ptr = ole_info_read_xbat (ole, ptr,
			xbat, xbat + last*BAT_INDEX_SIZE);
	}

	if (ptr == NULL) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"Inconsistent block allocation table");
		return TRUE;
	}

	ole->dirent = info->root_dir = ole_dirent_new (ole, 0, NULL);
	return FALSE;
}

static void
gsf_infile_msole_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInfileMSOle *ole = GSF_INFILE_MSOLE (obj);

	if (ole->input != NULL) {
		g_object_unref (G_OBJECT (ole->input));
		ole->input = NULL;
	}
	if (ole->info != NULL) {
		ole_info_unref (ole->info);
		ole->info = NULL;
	}
	if (ole->bat != NULL) {
		g_free (ole->bat);
		ole->bat = NULL;
	}

	parent_class = g_type_class_peek (GSF_INFILE_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

/**
 * ole_dup :
 * @src :
 *
 * Utility routine to _partially_ replicate a file.  It does NOT copy the bat
 * blocks, or init the dirent.
 *
 * Return value: the partial duplicate.
 **/
static GsfInfileMSOle *
ole_dup (GsfInfileMSOle const *src)
{
	GsfInfileMSOle	*dst;

	g_return_val_if_fail (src != NULL, NULL);

	dst = g_object_new (GSF_INFILE_MSOLE_TYPE, NULL);
	dst->input = gsf_input_dup (src->input);
	dst->info  = ole_info_ref (src->info);

	/* buf and buf_size are initialized to NULL */

	return dst;
}

static GsfInput *
gsf_infile_msole_dup (GsfInput *src_input)
{
	GsfInfileMSOle const *src = GSF_INFILE_MSOLE (src_input);
	GsfInfileMSOle *dst = ole_dup (src);

	if (src->bat != NULL) {
		dst->bat = g_new (guint32, src->num_blocks),
		memcpy (dst->bat, src->bat, sizeof (guint32) * src->num_blocks);
	}
	dst->num_blocks = src->num_blocks;
	dst->dirent = src->dirent;

	return GSF_INPUT (dst);
}

static gboolean
gsf_infile_msole_eof (GsfInput *input)
{
	return input->cur_offset >= input->size;
}

static guint8 const *
gsf_infile_msole_read (GsfInput *input, int num_bytes)
{
	GsfInfileMSOle *ole = GSF_INFILE_MSOLE (input);
	guint32 first_block, last_block, raw_block, offset, i;
	guint8 const *data;
	guint8 *ptr;
	int count, end;

	end = input->cur_offset + num_bytes;
	if (end > input->size)
		return NULL; /* attempt to read past the end */

	/* small block files are preload */
	if (ole->dirent != NULL && ole->dirent->use_sb)
		return ole->stream.buf + input->cur_offset;

	first_block = OLE_BIG_BLOCK (input->cur_offset, ole);
	last_block = OLE_BIG_BLOCK (end, ole);

	/* optimization : are all the raw blocks contiguous */
	i = first_block;
	raw_block = ole->bat [i];
	while (++i <= last_block && ++raw_block == ole->bat [i])
		;
	if (i > last_block)
		return gsf_input_read (ole->input, num_bytes);

	/* damn, we need to copy it block by block */
	if (ole->stream.buf_size < num_bytes) {
		if (ole->stream.buf != NULL)
			g_free (ole->stream.buf);
		ole->stream.buf_size = num_bytes;
		ole->stream.buf = g_malloc (num_bytes);
	}

	ptr = ole->stream.buf;
	offset = input->cur_offset & ole->info->bb.filter;
	for (i = first_block ; i <= last_block ; i++ , ptr += count, num_bytes -= count) {
		count = ole->info->bb.size - offset;
		if (count > num_bytes)
			count = num_bytes;
		data = ole_get_block (ole, ole->bat [i]);
		if (data == NULL)
			return NULL;
		memcpy (ptr, data + offset, count);
		offset = 0;
	}

	return ole->stream.buf;
}

static int
gsf_infile_msole_seek (GsfInput *input, int offset, GsfOff_t whence)
{
	switch (whence) {
	case GSF_SEEK_SET : return offset;
	case GSF_SEEK_CUR : return input->cur_offset + offset;
	case GSF_SEEK_END : return input->size - offset;
	default : return -1;
	}
}

static GsfInput *
gsf_infile_msole_new_child (GsfInfileMSOle *parent, MSOleDirent *dirent)
{
	GsfInfileMSOle *child = ole_dup (parent);

	child->dirent = dirent;
	gsf_input_set_size (GSF_INPUT (child), dirent->size);
	gsf_input_set_name (GSF_INPUT (child), dirent->name);
	gsf_input_set_container (GSF_INPUT (child), GSF_INFILE (parent));

	if (dirent->is_directory)
		return GSF_INPUT (child);

	child->bat = ole_info_make_bat (parent->info,
		dirent->first_block, dirent->size, &child->num_blocks);

	if (dirent->use_sb) {
		unsigned i;
		guint8 const *data;

		if (NULL == child->info->sb_file) {
			GsfInfileMSOle *sb_file = ole_dup (parent);
			sb_file->bat = ole_info_make_bat (parent->info,
				sb_file->info->sbat_start, 0,
				&sb_file->num_blocks);
			gsf_input_set_size (GSF_INPUT (sb_file),
				sb_file->num_blocks << sb_file->info->bb.shift);
			sb_file->info->sb_file = sb_file;
		}
		child->stream.buf_size = child->info->threshold;
		child->stream.buf = g_malloc (child->info->threshold);

		for (i = 0 ; i < child->num_blocks; i++) {
			data = gsf_input_read (GSF_INPUT (child->info->sb_file),
				child->info->sb.size);
			if (data == NULL) {
				g_object_unref (G_OBJECT (child));
				return NULL;
			}
			memcpy (child->stream.buf + (i << child->info->sb.shift),
				data,  child->info->sb.size);
		}
	} else if (gsf_input_seek (child->input,
		OLE_HEADER_SIZE + (child->bat [0] << child->info->bb.shift),
		GSF_SEEK_SET) < 0)
		return NULL;

	return GSF_INPUT (child);
}

static GsfInput *
gsf_infile_msole_child_by_index (GsfInfile *infile, int target)
{
	GsfInfileMSOle *ole = GSF_INFILE_MSOLE (infile);
	GList *p;

	for (p = ole->dirent->children; p != NULL ; p = p->next)
		if (target-- <= 0)
			return gsf_infile_msole_new_child (ole, p->data);
	return NULL;
}

static char const *
gsf_infile_msole_name_by_index (GsfInfile *infile, int target)
{
	GsfInfileMSOle *ole = GSF_INFILE_MSOLE (infile);
	GList *p;

	for (p = ole->dirent->children; p != NULL ; p = p->next)
		if (target-- <= 0)
			return ((MSOleDirent *)p->data)->name;
	return NULL;
}

static GsfInput *
gsf_infile_msole_child_by_name (GsfInfile *infile, char const *name)
{
	GsfInfileMSOle *ole = GSF_INFILE_MSOLE (infile);
	GList *p;

	for (p = ole->dirent->children; p != NULL ; p = p->next) {
		MSOleDirent *dirent = p->data;
		if (dirent->name != NULL && !strcmp (name, dirent->name))
			return gsf_infile_msole_new_child (ole, dirent);
	}
	return NULL;
}

static int
gsf_infile_msole_num_children (GsfInfile *infile)
{
	GsfInfileMSOle *ole = GSF_INFILE_MSOLE (infile);

	g_return_val_if_fail (ole->dirent != NULL, -1);

	if (!ole->dirent->is_directory)
		return -1;
	return g_list_length (ole->dirent->children);
}

static void
gsf_infile_msole_init (GObject *obj)
{
	GsfInfileMSOle *ole = GSF_INFILE_MSOLE (obj);

	ole->info = NULL;

	ole->bat		= NULL;
	ole->num_blocks		= 0;
	ole->stream.buf		= NULL;
	ole->stream.buf_size	= 0; 
}

static void
gsf_infile_msole_class_init (GObjectClass *gobject_class)
{
	GsfInputClass  *input_class  = GSF_INPUT_CLASS (gobject_class);
	GsfInfileClass *infile_class = GSF_INFILE_CLASS (gobject_class);

	gobject_class->finalize		= gsf_infile_msole_finalize;
	input_class->dup		= gsf_infile_msole_dup;
	/*input_class->size : default implementation is fine */
	input_class->eof		= gsf_infile_msole_eof;
	input_class->read		= gsf_infile_msole_read;
	input_class->seek		= gsf_infile_msole_seek;
	infile_class->num_children	= gsf_infile_msole_num_children;
	infile_class->name_by_index	= gsf_infile_msole_name_by_index;
	infile_class->child_by_index	= gsf_infile_msole_child_by_index;
	infile_class->child_by_name	= gsf_infile_msole_child_by_name;
}

GSF_CLASS (GsfInfileMSOle, gsf_infile_msole,
	   gsf_infile_msole_class_init, gsf_infile_msole_init,
	   GSF_INFILE_TYPE)

/**
 * gsf_infile_msole_new :
 * @input :
 * @err   :
 *
 * Opens the root directory of an MS Ole file.
 * NOTE : Absorbs a reference to the input
 *
 * Return value : the new ole file handler
 **/
GsfInfile *
gsf_infile_msole_new (GsfInput *input, GError **err)
{
	GsfInfileMSOle	*ole;

	ole = g_object_new (GSF_INFILE_MSOLE_TYPE, NULL);
	ole->input = input; /* absorb reference */
	gsf_input_set_size (GSF_INPUT (ole), 0);

	if (ole_init_info (ole, err)) {
		g_object_unref (G_OBJECT (ole));
		return NULL;
	}

	return GSF_INFILE (ole);
}
