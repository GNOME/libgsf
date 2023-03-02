/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-outfile-msole.c:
 *
 * Copyright (C) 2002-2006 Jody Goldberg (jody@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Outc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf-outfile-msole.h>
#include <gsf/gsf.h>
#include <gsf/gsf-msole-impl.h>
#include <glib/gi18n-lib.h>

#include <string.h>

static GObjectClass *parent_class;
static GsfOutputClass *gsf_output_class;

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "libgsf:msole"

typedef enum { MSOLE_DIR, MSOLE_SMALL_BLOCK, MSOLE_BIG_BLOCK } MSOleOutfileType;

/* The most common values */
#define OLE_DEFAULT_THRESHOLD	 0x1000
#define OLE_DEFAULT_SB_SHIFT	 6
#define OLE_DEFAULT_BB_SHIFT	 9
#define OLE_DEFAULT_BB_SIZE	 (1u << OLE_DEFAULT_BB_SHIFT)
#define OLE_DEFAULT_SB_SIZE	 (1u << OLE_DEFAULT_SB_SHIFT)

struct _GsfOutfileMSOle {
	GsfOutfile parent;

	GsfOutput    	*sink;
	GsfOutfileMSOle	*root;

	GsfMSOleSortingKey *key;

	MSOleOutfileType type;
	unsigned	 first_block;
	unsigned	 blocks;
	unsigned	 child_index;

	struct {
		unsigned shift;
		unsigned size;
	} bb, sb;

	union {
		struct {
			GSList 	  *children;
			GPtrArray *root_order;	/* only valid for the root */
		} dir;
		struct {
			guint8 *buf;
		} small_block;
		struct {
			size_t  start_offset;	/* in bytes */
		} big_block;
	} content;
	unsigned char clsid[16];		/* 16 byte GUID used by some apps */
};

enum {
	PROP_0,
	PROP_SINK,
	PROP_SMALL_BLOCK_SIZE,
	PROP_BIG_BLOCK_SIZE
};

typedef struct {
	GsfOutfileClass base;
} GsfOutfileMSOleClass;

static void
gsf_outfile_msole_set_sink (GsfOutfileMSOle *ole, GsfOutput *sink)
{
	if (sink)
		g_object_ref (sink);
	if (ole->sink)
		g_object_unref (ole->sink);
	ole->sink = sink;
}

/* returns the number of times 1 must be shifted left to reach value */
static guint
compute_shift (guint value)
{
	guint i = 0;
	while ((value >> i) > 1)
		i++;
	return i;
}

static void
gsf_outfile_msole_set_bb_size (GsfOutfileMSOle *ole, guint size)
{
	ole->bb.size = size;
	ole->bb.shift = compute_shift (size);
}

static void
gsf_outfile_msole_set_sb_size (GsfOutfileMSOle *ole, guint size)
{
	ole->sb.size = size;
	ole->sb.shift = compute_shift (size);
}

static void
gsf_outfile_msole_finalize (GObject *obj)
{
	GsfOutfileMSOle *ole = GSF_OUTFILE_MSOLE (obj);

	gsf_msole_sorting_key_free (ole->key);
	ole->key = NULL;

	switch (ole->type) {
	case MSOLE_DIR:
		g_slist_free (ole->content.dir.children);
		ole->content.dir.children = NULL;
		if (ole->content.dir.root_order != NULL)
			g_warning ("Finalizing a MSOle Outfile without closing it.");
		break;

	case MSOLE_SMALL_BLOCK:
		g_free (ole->content.small_block.buf);
		ole->content.small_block.buf = NULL;
		break;

	case MSOLE_BIG_BLOCK:
		break;
	default:
		g_assert_not_reached ();
	}

	parent_class->finalize (obj);
}

static void
gsf_outfile_msole_dispose (GObject *obj)
{
	GsfOutfileMSOle *ole = GSF_OUTFILE_MSOLE (obj);
	GsfOutput *output = GSF_OUTPUT (obj);

	if (!gsf_output_is_closed (output))
		gsf_output_close (output);

	gsf_outfile_msole_set_sink (ole, NULL);

	parent_class->dispose (obj);
}

static gboolean
gsf_outfile_msole_seek (GsfOutput *output, gsf_off_t offset,
			GSeekType whence)
{
	GsfOutfileMSOle *ole = (GsfOutfileMSOle *)output;

	switch (whence) {
	case G_SEEK_SET : break;
	case G_SEEK_CUR : offset += output->cur_offset;	break;
	case G_SEEK_END : offset += output->cur_size;	break;
	default: g_assert_not_reached ();
	}

	switch (ole->type) {
	case MSOLE_DIR:
		if (offset != 0) {
			g_warning ("Attempt to seek a directory");
			return FALSE;
		}
		return TRUE;

	case MSOLE_SMALL_BLOCK:
		/* it is ok to seek past the big block threshold
		 * we don't convert until they _write_ something
		 */
		return TRUE;

	case MSOLE_BIG_BLOCK:
		return gsf_output_seek (ole->sink,
			(gsf_off_t)(ole->content.big_block.start_offset + offset),
			G_SEEK_SET);

	default:
		g_assert_not_reached ();
	}

	return FALSE;
}

/* Globals to support variable OLE sector size.					*/
/* 512 and 4096 bytes are the only known values for sector size on		*/
/* Win2k/XP platforms.  Attempts to create OLE files on Win2k/XP with		*/
/* other values	using StgCreateStorageEx() fail with invalid parameter.		*/
/* This code has been tested with 128,256,512,4096,8192	sizes for		*/
/* libgsf read/write.  Interoperability with MS OLE32.DLL has been		*/
/* tested with 512 and 4096 block size for filesizes up to 2GB.			*/

#define ZERO_PAD_BUF_SIZE 4096

/* static objects are zero-initialized as per C/C++ standards */
static guint8 const zero_buf [ZERO_PAD_BUF_SIZE];

/* Calculate the block of the current offset in the file.  A useful idiom is to
 * pad_zero to move to the start of the next block, then get the block number.
 * This avoids fence post type problems with partial blocks. */
static inline guint32
ole_cur_block (GsfOutfileMSOle const *ole)
{
	return (gsf_output_tell (ole->sink) - OLE_HEADER_SIZE) >> ole->bb.shift;
}

static inline unsigned
ole_bytes_left_in_block (GsfOutfileMSOle *ole)
{
	/* blocks are multiples of bb.size (the header is padded out to bb.size) */
	unsigned r = gsf_output_tell (ole->sink) % ole->bb.size;
	return (r != 0) ? (ole->bb.size - r) : 0;
}

static void
ole_pad_zero (GsfOutfileMSOle *ole)
{
	/* no need to bounds check.  len will always be less than bb.size, and
	 * we already check that zero_buf is big enough at creation */
	unsigned len = ole_bytes_left_in_block (ole);
	if (len > 0)
		gsf_output_write (ole->sink, len, zero_buf);
}

#define CHUNK_SIZE 1024u
#define FLUSH do { if (bufi) gsf_output_write (sink, bufi * BAT_INDEX_SIZE, buf); bufi = 0; } while (0)
#define ADD_ITEM(i_) do { GSF_LE_SET_GUINT32 (buf + bufi * BAT_INDEX_SIZE, (i_)); bufi++; if (bufi == CHUNK_SIZE) FLUSH; } while (0)

/* Utility routine to generate a BAT for a file known to be sequential and
 * continuous. */
static void
ole_write_bat (GsfOutput *sink, guint32 block, unsigned blocks)
{
	guint8 buf[BAT_INDEX_SIZE * CHUNK_SIZE];
	guint bufi = 0;

	while (blocks-- > 1) {
		block++;
		ADD_ITEM (block);
	}
	ADD_ITEM (BAT_MAGIC_END_OF_CHAIN);
	FLUSH;
}

static void
ole_write_const (GsfOutput *sink, guint32 value, unsigned n)
{
	guint8 buf[BAT_INDEX_SIZE * CHUNK_SIZE];
	guint bufi = 0;

	while (n-- > 0)
		ADD_ITEM(value);

	FLUSH;
}

#undef CHUNK_SIZE
#undef FLUSH
#undef ADD_ITEM

static guint64
datetime_to_filetime (GDateTime *dt)
{
	static const guint64 epoch = G_GINT64_CONSTANT (11644473600);

	if (!dt)
		return 0u;

	/* ft is number of 100ns since Jan 1 1601 */

	return (g_date_time_to_unix(dt) + epoch) * 10000000u
		+ g_date_time_get_microsecond(dt) * 10u;
}

static void
ole_pad_bat_unused (GsfOutfileMSOle *ole, unsigned residual)
{
	ole_write_const (ole->sink, BAT_MAGIC_UNUSED,
		(ole_bytes_left_in_block (ole) / BAT_INDEX_SIZE) - residual);
}

/* write the metadata (dirents, small block, xbats) */
static gboolean
gsf_outfile_msole_write_directory (GsfOutfileMSOle *ole)
{
	GsfOutfile *tmp;
	guint8  buf [OLE_HEADER_SIZE];
	guint32	sbat_start, num_sbat, sb_data_start, sb_data_size, sb_data_blocks;
	guint32	bat_start, num_bat, dirent_start, num_dirent_blocks, next, child_index;
	unsigned i, j, blocks, num_xbat, xbat_pos;
	gsf_off_t data_size;
	unsigned metabat_size = ole->bb.size / BAT_INDEX_SIZE - 1;
	GPtrArray *elem = ole->root->content.dir.root_order;

	/* write small block data */
	blocks = 0;
	sb_data_start = ole_cur_block (ole);
	data_size = gsf_output_tell (ole->sink);
	for (i = 0 ; i < elem->len ; i++) {
		GsfOutfileMSOle *child = g_ptr_array_index (elem, i);
		if (child->type == MSOLE_SMALL_BLOCK) {
			gsf_off_t size = gsf_output_size (GSF_OUTPUT (child));
			if (size > 0) {
				child->blocks = ((size - 1) >> ole->sb.shift) + 1;
				gsf_output_write (ole->sink,
						  child->blocks << ole->sb.shift,
						  child->content.small_block.buf);
				child->first_block = blocks;
				blocks += child->blocks;
			} else {
				child->blocks = 0;
				child->first_block = BAT_MAGIC_END_OF_CHAIN;
			}
		}
	}
	data_size = gsf_output_tell (ole->sink) - data_size;
	sb_data_size = data_size;
	if ((gsf_off_t) sb_data_size != data_size) {
		/* Check for overflow */
		g_warning ("File too big");
		return FALSE;
	}
	ole_pad_zero (ole);
	sb_data_blocks = ole_cur_block (ole) - sb_data_start;

	/* write small block BAT (the meta bat is in a file) */
	sbat_start = ole_cur_block (ole);
	for (i = 0 ; i < elem->len ; i++) {
		GsfOutfileMSOle *child = g_ptr_array_index (elem, i);
		if (child->type == MSOLE_SMALL_BLOCK && child->blocks > 0)
			ole_write_bat (ole->sink, child->first_block, child->blocks);
	}
	ole_pad_bat_unused (ole, 0);
	num_sbat = ole_cur_block (ole) - sbat_start;

	/* write dirents */
	dirent_start = ole_cur_block (ole);
	for (i = 0 ; i < elem->len ; i++) {
		GsfOutfileMSOle *child = g_ptr_array_index (elem, i);
		glong j, name_len = 0;

		memset (buf, 0, DIRENT_SIZE);

		/* Hard code 'Root Entry' for the root */
		if (i == 0 || gsf_output_name (GSF_OUTPUT (child)) != NULL) {
			char const *name = (i == 0)
				? "Root Entry" : gsf_output_name (GSF_OUTPUT (child));
			gunichar2 *name_utf16 = g_utf8_to_utf16 (name,
				-1, NULL, &name_len, NULL);
			if (name_len >= DIRENT_MAX_NAME_SIZE)
				name_len = DIRENT_MAX_NAME_SIZE-1;

			/* be wary about endianness */
			for (j = 0 ; j < name_len ; j++)
				GSF_LE_SET_GUINT16 (buf + j*2, name_utf16 [j]);
			g_free (name_utf16);
			name_len++;
		}
		GSF_LE_SET_GUINT16 (buf + DIRENT_NAME_LEN, name_len*2);

		if (child->root == child) {
			GSF_LE_SET_GUINT8  (buf + DIRENT_TYPE,	DIRENT_TYPE_ROOTDIR);
			GSF_LE_SET_GUINT32 (buf + DIRENT_FIRSTBLOCK,
				(sb_data_size > 0) ? sb_data_start : BAT_MAGIC_END_OF_CHAIN);
			GSF_LE_SET_GUINT32 (buf + DIRENT_FILE_SIZE, sb_data_size);
			memcpy (buf + DIRENT_CLSID, child->clsid, sizeof (child->clsid));
		} else if (child->type == MSOLE_DIR) {
			GSF_LE_SET_GUINT8 (buf + DIRENT_TYPE, DIRENT_TYPE_DIR);
			GSF_LE_SET_GUINT32 (buf + DIRENT_FIRSTBLOCK, BAT_MAGIC_END_OF_CHAIN);
			GSF_LE_SET_GUINT32 (buf + DIRENT_FILE_SIZE, 0);
			/* write the class id */
			memcpy (buf + DIRENT_CLSID, child->clsid, sizeof (child->clsid));
		} else {
			guint32 size = child->parent.parent.cur_size;

			if ((gsf_off_t) size != child->parent.parent.cur_size)
				g_warning ("File too big");
			GSF_LE_SET_GUINT8 (buf + DIRENT_TYPE, DIRENT_TYPE_FILE);
			GSF_LE_SET_GUINT32 (buf + DIRENT_FIRSTBLOCK, child->first_block);
			GSF_LE_SET_GUINT32 (buf + DIRENT_FILE_SIZE, size);
		}
		GSF_LE_SET_GUINT64 (buf + DIRENT_MODIFY_TIME,
				    datetime_to_filetime (gsf_output_get_modtime (GSF_OUTPUT (child))));

		/* make everything black (red == 0) */
		GSF_LE_SET_GUINT8  (buf + DIRENT_COLOUR, 1);

		tmp = gsf_output_container (GSF_OUTPUT (child));
		next = DIRENT_MAGIC_END;
		if (child->root != child && tmp != NULL) {
			GSList *ptr = GSF_OUTFILE_MSOLE (tmp)->content.dir.children;
			for (; ptr != NULL ; ptr = ptr->next)
				if (ptr->data == child) {
					if (ptr->next != NULL) {
						GsfOutfileMSOle *sibling = ptr->next->data;
						next = sibling->child_index;
					}
					break;
				}
		}
		/* make linked list rather than tree, only use next */
		GSF_LE_SET_GUINT32 (buf + DIRENT_PREV, DIRENT_MAGIC_END);
		GSF_LE_SET_GUINT32 (buf + DIRENT_NEXT, next);

		child_index = DIRENT_MAGIC_END;
		if (child->type == MSOLE_DIR && child->content.dir.children != NULL) {
			GsfOutfileMSOle *first = child->content.dir.children->data;
			child_index = first->child_index;
		}
		GSF_LE_SET_GUINT32 (buf + DIRENT_CHILD, child_index);

		gsf_output_write (ole->sink, DIRENT_SIZE, buf);
	}
	ole_pad_zero (ole);
	num_dirent_blocks = ole_cur_block (ole) - dirent_start;

	/* write BAT */
	bat_start = ole_cur_block (ole);
	for (i = 0 ; i < elem->len ; i++) {
		GsfOutfileMSOle *child = g_ptr_array_index (elem, i);
		if (child->type == MSOLE_BIG_BLOCK)
			ole_write_bat (ole->sink, child->first_block, child->blocks);
	}
	if (sb_data_blocks > 0)
		ole_write_bat (ole->sink, sb_data_start, sb_data_blocks);
	if (num_sbat > 0)
		ole_write_bat (ole->sink, sbat_start, num_sbat);
	ole_write_bat (ole->sink, dirent_start, num_dirent_blocks);

	/* List the BAT and meta-BAT blocks in the BAT.  Doing this may
	 * increase the size of the bat and hence the metabat, so be
	 * prepared to iterate.
	 */
	num_bat = 0;
	num_xbat = 0;
	while (gsf_output_error (ole->sink) == NULL) {
		// If we have an error, then the actual size as reported
		// by _tell and ->cur_size may be out of sync.  We don't
		// want to loop forever here.

		unsigned i = ((ole->sink->cur_size
		      + BAT_INDEX_SIZE * (num_bat + num_xbat)
		      - OLE_HEADER_SIZE - 1) >> ole->bb.shift) + 1;
		i -= bat_start;
		if (num_bat != i) {
			num_bat = i;
			continue;
		}
		i = 0;
		if (num_bat > OLE_HEADER_METABAT_SIZE)
			i = 1 + ((num_bat - OLE_HEADER_METABAT_SIZE - 1)
				 / metabat_size);
		if (num_xbat != i) {
			num_xbat = i;
			continue;
		}

		break;
	}

	ole_write_const (ole->sink, BAT_MAGIC_BAT, num_bat);
	ole_write_const (ole->sink, BAT_MAGIC_METABAT, num_xbat);
	ole_pad_bat_unused (ole, 0);

	if (num_xbat > 0) {
		xbat_pos = ole_cur_block (ole);
		blocks = OLE_HEADER_METABAT_SIZE;
	} else {
		xbat_pos = BAT_MAGIC_END_OF_CHAIN;
		blocks = num_bat;
	}

	/* fix up the header */
	if (ole->bb.size == 4096) {
		/* set _cSectDir for 4k sector files */
		GSF_LE_SET_GUINT32 (buf,   num_dirent_blocks);
		gsf_output_seek (ole->sink,
			(gsf_off_t) OLE_HEADER_CSECTDIR, G_SEEK_SET);
		gsf_output_write (ole->sink, 4, buf);
	}
	GSF_LE_SET_GUINT32 (buf,   num_bat);
	GSF_LE_SET_GUINT32 (buf+4, dirent_start);
	gsf_output_seek (ole->sink,
		(gsf_off_t) OLE_HEADER_NUM_BAT, G_SEEK_SET);
	gsf_output_write (ole->sink, 8, buf);

	GSF_LE_SET_GUINT32 (buf+0x0,
		(num_sbat > 0) ? sbat_start : BAT_MAGIC_END_OF_CHAIN);
	GSF_LE_SET_GUINT32 (buf+0x4, num_sbat);
	GSF_LE_SET_GUINT32 (buf+0x8, xbat_pos);
	GSF_LE_SET_GUINT32 (buf+0xc, num_xbat);
	gsf_output_seek (ole->sink, (gsf_off_t) OLE_HEADER_SBAT_START,
			 G_SEEK_SET);
	gsf_output_write (ole->sink, 0x10, buf);

	/* write initial Meta-BAT */
	for (i = 0 ; i < blocks ; i++) {
		GSF_LE_SET_GUINT32 (buf, bat_start + i);
		gsf_output_write (ole->sink, BAT_INDEX_SIZE, buf);
	}

	/* write extended Meta-BAT */
	if (num_xbat > 0) {
		gsf_output_seek (ole->sink, 0, G_SEEK_END);
		for (i = 0 ; i++ < num_xbat ; ) {
			bat_start += blocks;
			num_bat   -= blocks;
			blocks = (num_bat > metabat_size) ? metabat_size : num_bat;
			for (j = 0 ; j < blocks ; j++) {
				GSF_LE_SET_GUINT32 (buf, bat_start + j);
				gsf_output_write (ole->sink, BAT_INDEX_SIZE, buf);
			}

			if (i == num_xbat) {
				ole_pad_bat_unused (ole, 1);
				xbat_pos = BAT_MAGIC_END_OF_CHAIN;
			} else
				xbat_pos++;
			GSF_LE_SET_GUINT32 (buf, xbat_pos);
			gsf_output_write (ole->sink, BAT_INDEX_SIZE, buf);
		}
	}

	/* free the children */
	for (i = 0 ; i < elem->len ; i++)
		g_object_unref (g_ptr_array_index (elem, i));
	g_ptr_array_free (elem, TRUE);
	ole->content.dir.root_order = NULL;

	return TRUE;
}

static void
gsf_outfile_msole_hoist_error (GsfOutfileMSOle *ole)
{
	GsfOutput *oole = GSF_OUTPUT (ole);
	if (gsf_output_error (oole) == NULL &&
	    gsf_output_error (ole->sink)) {
		oole->err = g_error_copy (ole->sink->err);
	}
}


static gboolean
gsf_outfile_msole_close (GsfOutput *output)
{
	GsfOutfileMSOle *ole = (GsfOutfileMSOle *)output;

	if (gsf_output_container (output) == NULL) {	/* The root dir */
		gboolean ok = gsf_outfile_msole_write_directory (ole);
		gsf_outfile_msole_hoist_error (ole);
		if (!gsf_output_close (ole->sink))
			ok = FALSE;
		return ok;
	}

	if (ole->type == MSOLE_BIG_BLOCK) {
		gsf_outfile_msole_seek (output, 0, G_SEEK_END);
		ole_pad_zero (ole);
		ole->blocks = ole_cur_block (ole) - ole->first_block;
		return gsf_output_unwrap (G_OBJECT (output), ole->sink);
	}

	return TRUE;
}

static gboolean
gsf_outfile_msole_write (GsfOutput *output,
			 size_t num_bytes, guint8 const *data)
{
	GsfOutfileMSOle *ole = (GsfOutfileMSOle *)output;
	size_t wsize;

	g_return_val_if_fail (ole->type != MSOLE_DIR, FALSE);
	if (ole->type == MSOLE_SMALL_BLOCK) {
		gboolean ok;
		guint8 *buf;
		gsf_off_t start_offset;

		if ((output->cur_offset + num_bytes) < OLE_DEFAULT_THRESHOLD) {
			memcpy (ole->content.small_block.buf + output->cur_offset,
				data, num_bytes);
			return TRUE;
		}
		ok = gsf_output_wrap (G_OBJECT (output), ole->sink);
		if (!ok)
			return FALSE;

		buf = ole->content.small_block.buf;
		ole->content.small_block.buf = NULL;
		start_offset = gsf_output_tell (ole->sink);
		ole->content.big_block.start_offset = start_offset;
		if ((gsf_off_t) ole->content.big_block.start_offset
		    != start_offset) {
			/* Check for overflow */
			g_warning ("File too big");
			return FALSE;
		}

		ole->first_block = ole_cur_block (ole);
		ole->type = MSOLE_BIG_BLOCK;
		wsize = output->cur_size;
		if ((gsf_off_t) wsize != output->cur_size) {
			/* Check for overflow */
			g_warning ("File too big");
			return FALSE;
		}
		gsf_output_write (ole->sink, wsize, buf);
		g_free (buf);

		// If we had a seek then we might not be at the right location.
		// This can happen both with a seek beyond the end of file
		// (see bug #14) and with a backward seek.
		gsf_output_seek (ole->sink,
				 ole->content.big_block.start_offset + output->cur_offset,
				 G_SEEK_SET);
	}

	g_return_val_if_fail (ole->type == MSOLE_BIG_BLOCK, FALSE);

	gsf_output_write (ole->sink, num_bytes, data);

	return TRUE;
}

static gsf_off_t gsf_outfile_msole_vprintf (GsfOutput *output,
	char const *format, va_list args) G_GNUC_PRINTF (2, 0);

static gsf_off_t
gsf_outfile_msole_vprintf (GsfOutput *output, char const *format, va_list args)
{
	GsfOutfileMSOle *ole = (GsfOutfileMSOle *)output;

	/* An optimization. */
	if (ole->type == MSOLE_BIG_BLOCK)
		return gsf_output_vprintf (ole->sink, format, args);

	/* In other cases, use the gsf_output_real_vprintf fallback method.
	 * (This eventually calls gsf_outfile_msole_write, which will also
	 * check that ole->type != MSOLE_DIR.)
	 */
	return gsf_output_class->Vprintf (output, format, args);
}


static void
ole_register_child (GsfOutfileMSOle *root, GsfOutfileMSOle *child)
{
	child->root = root;
	g_object_ref (child);
	child->child_index = root->content.dir.root_order->len;
	g_ptr_array_add (root->content.dir.root_order, child);
}

static gint
ole_name_cmp (GsfOutfileMSOle const *a, GsfOutfileMSOle const *b)
{
	return gsf_msole_sorting_key_cmp (a->key, b->key);
}

static void
make_sorting_name (GsfOutfileMSOle *ole,
		   G_GNUC_UNUSED GParamSpec *pspec,
		   G_GNUC_UNUSED gpointer user)
{
	const char *name = gsf_output_name (GSF_OUTPUT (ole));
	gsf_msole_sorting_key_free (ole->key);
	ole->key = gsf_msole_sorting_key_new (name);
}

static GsfOutput *
gsf_outfile_msole_new_child (GsfOutfile *parent,
			     char const *name, gboolean is_dir,
			     char const *first_property_name, va_list args)
{
	GsfOutfileMSOle *ole_parent = (GsfOutfileMSOle *)parent;
	GsfOutfileMSOle *child;

	g_return_val_if_fail (ole_parent != NULL, NULL);
	g_return_val_if_fail (ole_parent->type == MSOLE_DIR, NULL);

	child = (GsfOutfileMSOle *)g_object_new_valist (
		GSF_OUTFILE_MSOLE_TYPE, first_property_name, args);
	if (is_dir) {
		child->type = MSOLE_DIR;
		child->content.dir.children = NULL;
	} else {
		/* start as small block */
		child->type = MSOLE_SMALL_BLOCK;
		child->content.small_block.buf = g_new0 (guint8, OLE_DEFAULT_THRESHOLD);
	}
	child->root   = ole_parent->root;
	gsf_outfile_msole_set_sink (child, ole_parent->sink);
	gsf_outfile_msole_set_sb_size (child, ole_parent->sb.size);
	gsf_outfile_msole_set_bb_size (child, ole_parent->bb.size);
	gsf_output_set_name (GSF_OUTPUT (child), name);
	gsf_output_set_container (GSF_OUTPUT (child), parent);

	ole_parent->content.dir.children = g_slist_insert_sorted (
		ole_parent->content.dir.children, child,
		(GCompareFunc)ole_name_cmp);
	ole_register_child (ole_parent->root, child);

	return GSF_OUTPUT (child);
}

static void
gsf_outfile_msole_init (GObject *obj)
{
	GsfOutfileMSOle *ole = GSF_OUTFILE_MSOLE (obj);

	ole->sink   = NULL;
	ole->root   = NULL;
	ole->type   = MSOLE_DIR;

	ole->content.dir.children = NULL;
	ole->content.dir.root_order = NULL;
	memset (ole->clsid, 0, sizeof (ole->clsid));
}

static GObject*
gsf_outfile_msole_constructor (GType                  type,
			       guint                  n_construct_properties,
			       GObjectConstructParam *construct_params)
{
	GObject *obj = parent_class->constructor (type,
						  n_construct_properties,
						  construct_params);
	GsfOutfileMSOle *ole = (GsfOutfileMSOle *)obj;
	g_signal_connect (obj,
			  "notify::name",
			  G_CALLBACK (make_sorting_name), NULL);
	make_sorting_name (ole, NULL, NULL);
	return obj;
}

static void
gsf_outfile_msole_get_property (GObject     *object,
				guint        property_id,
				GValue      *value,
				GParamSpec  *pspec)
{
	GsfOutfileMSOle *ole = GSF_OUTFILE_MSOLE (object);

	switch (property_id) {
	case PROP_SINK:
		g_value_set_object (value, ole->sink);
		break;
	case PROP_SMALL_BLOCK_SIZE:
		g_value_set_uint (value, ole->sb.size);
		break;
	case PROP_BIG_BLOCK_SIZE:
		g_value_set_uint (value, ole->bb.size);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gsf_outfile_msole_set_property (GObject      *object,
				guint         property_id,
				GValue const *value,
				GParamSpec   *pspec)
{
	GsfOutfileMSOle *ole = GSF_OUTFILE_MSOLE (object);

	switch (property_id) {
	case PROP_SINK:
		gsf_outfile_msole_set_sink (ole, g_value_get_object (value));
		break;
	case PROP_SMALL_BLOCK_SIZE:
		gsf_outfile_msole_set_sb_size (ole, g_value_get_uint (value));
		break;
	case PROP_BIG_BLOCK_SIZE:
		gsf_outfile_msole_set_bb_size (ole, g_value_get_uint (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gsf_outfile_msole_class_init (GObjectClass *gobject_class)
{
	GsfOutputClass  *output_class  = GSF_OUTPUT_CLASS (gobject_class);
	GsfOutfileClass *outfile_class = GSF_OUTFILE_CLASS (gobject_class);

	gobject_class->finalize		= gsf_outfile_msole_finalize;
	gobject_class->dispose		= gsf_outfile_msole_dispose;
	gobject_class->constructor	= gsf_outfile_msole_constructor;
	gobject_class->get_property     = gsf_outfile_msole_get_property;
	gobject_class->set_property     = gsf_outfile_msole_set_property;
	output_class->Close		= gsf_outfile_msole_close;
	output_class->Seek		= gsf_outfile_msole_seek;
	output_class->Write		= gsf_outfile_msole_write;
	output_class->Vprintf		= gsf_outfile_msole_vprintf;
	outfile_class->new_child	= gsf_outfile_msole_new_child;

	g_object_class_install_property
		(gobject_class, PROP_SINK,
		 g_param_spec_object ("sink",
				      _("Sink"),
				      _("The destination for writes"),
				      GSF_OUTPUT_TYPE,
				      G_PARAM_STATIC_STRINGS |
				      G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(gobject_class,
		 PROP_SMALL_BLOCK_SIZE,
		 g_param_spec_uint ("small-block-size",
				    _("Small block size"),
				    _("The size of the OLE's small blocks"),
				    8u, ZERO_PAD_BUF_SIZE,
				    OLE_DEFAULT_SB_SIZE,
				    G_PARAM_STATIC_STRINGS |
				    G_PARAM_READWRITE |
				    G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(gobject_class,
		 PROP_BIG_BLOCK_SIZE,
		 g_param_spec_uint ("big-block-size",
				    _("Big block size"),
				    _("The size of the OLE's big blocks"),
				    DIRENT_SIZE, ZERO_PAD_BUF_SIZE,
				    OLE_DEFAULT_BB_SIZE,
				    G_PARAM_STATIC_STRINGS |
				    G_PARAM_READWRITE |
				    G_PARAM_CONSTRUCT_ONLY));

	parent_class = g_type_class_peek_parent (gobject_class);
	gsf_output_class = g_type_class_peek (GSF_OUTPUT_TYPE);
}

GSF_CLASS (GsfOutfileMSOle, gsf_outfile_msole,
	   gsf_outfile_msole_class_init, gsf_outfile_msole_init,
	   GSF_OUTFILE_TYPE)

/**
 * gsf_outfile_msole_new_full:
 * @sink: a #GsfOutput to hold the OLE2 file.
 * @bb_size: size of large blocks.
 * @sb_size: size of small blocks.
 *
 * Creates the root directory of an MS OLE file and manages the addition of
 * children.
 *
 * <note>This adds a reference to @sink.</note>
 *
 * Returns: the new ole file handler.
 **/
GsfOutfile *
gsf_outfile_msole_new_full (GsfOutput *sink, guint bb_size, guint sb_size)
{
	static guint8 const default_header [] = {
/* 0x00 */	0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1,
/* 0x08 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 0x18 */	0x3e, 0x00, 0x03, 0x00, 0xfe, 0xff, 0x09, 0x00,
/* 0x20 */	0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x28 */	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x30 */	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
/* 0x38 */	0x00, 0x10, 0x00, 0x00 /* 0x3c-0x4b: filled on close */
	};
	guint8 *buf;
	GsfOutfileMSOle *ole;

	g_return_val_if_fail (GSF_IS_OUTPUT (sink), NULL);
	g_return_val_if_fail (sb_size == (1u << compute_shift (sb_size)), NULL);
	g_return_val_if_fail (bb_size == (1u << compute_shift (bb_size)), NULL);
	g_return_val_if_fail (sb_size <= bb_size, NULL);

	ole = g_object_new (GSF_OUTFILE_MSOLE_TYPE,
			    "sink", sink,
			    "small-block-size", sb_size,
			    "big-block-size", bb_size,
			    "container", NULL,
			    "name", gsf_output_name (sink),
			    NULL);
	ole->type = MSOLE_DIR;
	ole->content.dir.root_order = g_ptr_array_new ();
	ole_register_child (ole, ole);

	/* build the header */
	buf = g_new (guint8, OLE_HEADER_SIZE);
	memcpy (buf, default_header, sizeof (default_header));
	memset (buf + sizeof (default_header), 0xff,
		OLE_HEADER_SIZE - sizeof (default_header));
	GSF_LE_SET_GUINT16 (buf + OLE_HEADER_BB_SHIFT, ole->bb.shift);
	GSF_LE_SET_GUINT16 (buf + OLE_HEADER_SB_SHIFT, ole->sb.shift);
	/* 4k sector OLE files seen in the wild have version 4 */
	if (ole->bb.size == 4096)
		GSF_LE_SET_GUINT16 (buf + OLE_HEADER_MAJOR_VER, 4);
	gsf_output_write (sink, OLE_HEADER_SIZE, buf);
	g_free (buf);

	/* header must be padded out to bb.size with zeros */
	ole_pad_zero(ole);

	return GSF_OUTFILE (ole);
}

/**
 * gsf_outfile_msole_new:
 * @sink: a #GsfOutput to hold the OLE2 file
 *
 * Creates the root directory of an MS OLE file and manages the addition of
 * children.
 *
 * <note>This adds a reference to @sink.</note>
 *
 * Returns: the new ole file handler.
 **/
GsfOutfile *
gsf_outfile_msole_new (GsfOutput *sink)
{
	return gsf_outfile_msole_new_full (sink,
		OLE_DEFAULT_BB_SIZE, OLE_DEFAULT_SB_SIZE);
}

/**
 * gsf_outfile_msole_set_class_id:
 * @ole: a #GsfOutfileMSOle
 * @clsid: (array fixed-size=16): Identifier (often a GUID in MS Windows apps)
 *
 * Write @clsid to the directory associated with @ole.
 *
 * Returns: %TRUE on success.
 **/
gboolean
gsf_outfile_msole_set_class_id (GsfOutfileMSOle *ole, guint8 const *clsid)
{
	g_return_val_if_fail (ole != NULL && ole->type == MSOLE_DIR, FALSE);
	memcpy (ole->clsid, clsid, sizeof (ole->clsid));
	return TRUE;
}
