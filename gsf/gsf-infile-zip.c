/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-infile-zip.c :
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
#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf-infile-impl.h>
#include <gsf/gsf-impl-utils.h>

typedef struct {
	GsfInput *sb_file;

	int ref_count;
} ZipInfo;

struct _GsfInfileZip {
	GsfInfile parent;

	GsfInput *input;
	ZipInfo	 *info;
};

typedef struct {
	GsfInfileClass  parent_class;
} GsfInfileZipClass;

#define GSF_INFILE_ZIP_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GSF_INFILE_ZIP_TYPE, GsfInfileZipClass))
#define IS_GSF_INFILE_ZIP_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GSF_INFILE_ZIP_TYPE))

/**
 * Based on information in zziplib
 */
#define ZIP_HEADER_SIZE 		30
#define ZIP_HEADER_VERSION 		4
#define ZIP_HEADER_OS	 		5
#define ZIP_HEADER_FLAGS 		6
#define ZIP_HEADER_COMP_METHOD 		8
#define ZIP_HEADER_TIME 		10
#define ZIP_HEADER_CRC 			14
#define ZIP_HEADER_COMP_SIZE		18
#define ZIP_HEADER_UNCOMP_SIZE		22
#define ZIP_HEADER_NAME_LEN		26
#define ZIP_HEADER_EXTRA_LEN		28

#define ZIP_TRAILER_SIZE 		22
#define ZIP_TRAILER_DISK 		2
#define ZIP_TRAILER_DIR_DISK 		4
#define ZIP_TRAILER_ENTRIES 		6
#define ZIP_TRAILER_TOTAL_ENTRIES 	8
#define ZIP_TRAILER_DIR_SIZE 		10
#define ZIP_TRAILER_DIR_POS 		14
#define ZIP_TRAILER_COMMENT_SIZE	18

#if 0
    char  z_magic[4];  /* central file header signature (0x02014b50) */
    struct zzip_version z_encoder;  /* version made by */
    struct zzip_version z_extract;  /* version need to extract */
    char  z_flags[2];  /* general purpose bit flag */
    char  z_compr[2];  /* compression method */
    struct zzip_dostime z_dostime;  /* last mod file time&date (dos format) */
    char  z_crc32[4];  /* crc-32 */
    char  z_csize[4];  /* compressed size */
    char  z_usize[4];  /* uncompressed size */
    char  z_namlen[2]; /* filename length (null if stdin) */
    char  z_extras[2];  /* extra field length */
    char  z_comment[2]; /* file comment length */
    char  z_diskstart[2]; /* disk number of start (if spanning zip over multiple disks) */
    char  z_filetype[2];  /* internal file attributes, bit0 = ascii */
    char  z_filemode[4];  /* extrnal file attributes, eg. msdos attrib byte */
    char  z_off[4];    /* relative offset of local file header, seekval if singledisk */
    /* followed by filename (of variable size) */
    /* followed by extra field (of variable size) */
    /* followed by file comment (of variable size) */
/* z_flags */
#define ZZIP_IS_ENCRYPTED(p)    ((*(unsigned char*)p)&1)
#define ZZIP_IS_COMPRLEVEL(p)  (((*(unsigned char*)p)>>1)&3)
#define ZZIP_IS_STREAMED(p)    (((*(unsigned char*)p)>>3)&1)

/* z_compr */
#define ZZIP_IS_STORED          0
#define ZZIP_IS_SHRUNK          1
#define ZZIP_IS_REDUCEDx1       2
#define ZZIP_IS_REDUCEDx2       3
#define ZZIP_IS_REDUCEDx3       4
#define ZZIP_IS_REDUCEDx4       5
#define ZZIP_IS_IMPLODED        6
#define ZZIP_IS_TOKENIZED       7
#define ZZIP_IS_DEFLATED        8
#define ZZIP_IS_DEFLATED_BETTER 9
#define ZZIP_IS_IMPLODED_BETTER 10

#endif

static void
zip_info_unref (ZipInfo *info)
{
	if (info->ref_count-- != 1)
		return;

	g_free (info);
}

/**
 * zip_init_info :
 * @zip :
 * @err : optionally NULL
 *
 * Read zip headers and do some sanity checking
 * along the way.
 *
 * Return value: TRUE on error setting @err if it is supplied.
 **/
static gboolean
zip_init_info (GsfInfileZip *zip, GError **err)
{
	static guint8 const dirent_signature[] =
		{ 'P', 'K', 0x01, 0x02 };
	static guint8 const header_signature[] =
		{ 'P', 'K', 0x03, 0x04 };
	static guint8 const trailer_signature[] =
		{ 'P', 'K', 0x05, 0x06 };
	guint8 const *header;
	ZipInfo *info;

	/* Check the header */
	if (gsf_input_seek (zip->input, 0, GSF_SEEK_SET) ||
	    NULL == (header = gsf_input_read (zip->input, ZIP_HEADER_SIZE, NULL)) ||
	    0 != memcmp (header, header_signature, sizeof (header_signature))) {
		if (err != NULL)
			*err = g_error_new (gsf_input_error (), 0,
				"No Zip signature");
		return TRUE;
	}

	/* Find and check the trailing header */
#if 0
		for (s = p + maplen-1; (s >= p); s--)
		{
			if (*s == 'P'
			    && p+maplen-1-s > sizeof(*trailer)-2
			    && ZZIP_DISK_TRAILER_CHECKMAGIC(s))
			{
				/* if the file-comment is not present, it happens
				   that the z_comment field often isn't either */
				if (p+maplen-1-s > sizeof(*trailer))
				{ memcpy (trailer, s, sizeof(*trailer)); }
				else
				{
					memcpy (trailer, s, sizeof(*trailer)-2);
					trailer->z_comment[0] = 0; trailer->z_comment[1] = 0;
				}

				{ return(0); }
			}
		}

	DBG5("directory = { entries= %d/%d, size= %ld, seek= %ld } ", 
	     ZZIP_GET16(trailer.z_entries),  ZZIP_GET16(trailer.z_finalentries),
	     ZZIP_GET32(trailer.z_rootsize), ZZIP_GET32(trailer.z_rootseek));
	if ((rv = zzip_parse_root_directory(dir->fd, &trailer, &dir->hdr0)) != 0)
#endif

	info = g_new0 (ZipInfo, 1);
	zip->info = info;

#if 0
	zip->dirent = info->root_dir = zip_dirent_new (zip, 0, NULL);
	if (zip->dirent == NULL)
		return TRUE;
#endif

	return FALSE;
}

static GsfInput *
gsf_infile_zip_dup (GsfInput *src_input)
{
	GsfInfileZip const *src = GSF_INFILE_ZIP (src_input);
	GsfInfileZip *dst = NULL;

	return GSF_INPUT (dst);
}

static guint8 const *
gsf_infile_zip_read (GsfInput *input, size_t num_bytes, guint8 *buffer)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (input);

#if 0
	if (zip->stream.buf_size < num_bytes) {
		if (zip->stream.buf != NULL)
			g_free (zip->stream.buf);
		zip->stream.buf_size = num_bytes;
		zip->stream.buf = g_malloc (num_bytes);
	}

	return zip->stream.buf;
#endif
	return NULL;
}

static gboolean
gsf_infile_zip_seek (GsfInput *input, off_t offset, GsfOff_t whence)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (input);
	return FALSE;
}

static GsfInput *
gsf_infile_zip_child_by_index (GsfInfile *infile, int target)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (infile);
	return NULL;
}

static char const *
gsf_infile_zip_name_by_index (GsfInfile *infile, int target)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (infile);
	return NULL;
}

static GsfInput *
gsf_infile_zip_child_by_name (GsfInfile *infile, char const *name)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (infile);
	return NULL;
}

static int
gsf_infile_zip_num_children (GsfInfile *infile)
{
	GsfInfileZip *zip = GSF_INFILE_ZIP (infile);
	return 0;
}
static void
gsf_infile_zip_finalize (GObject *obj)
{
	GObjectClass *parent_class;
	GsfInfileZip *zip = GSF_INFILE_ZIP (obj);

	if (zip->info != NULL) {
		zip_info_unref (zip->info);
		zip->info = NULL;
	}
	parent_class = g_type_class_peek (GSF_INFILE_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (obj);
}

static void
gsf_infile_zip_init (GObject *obj)
{
	GsfInfileZip *zip = (GsfInfileZip *)obj;
	zip->input = NULL;
	zip->info  = NULL;
}

static void
gsf_infile_zip_class_init (GObjectClass *gobject_class)
{
	GsfInputClass  *input_class  = GSF_INPUT_CLASS (gobject_class);
	GsfInfileClass *infile_class = GSF_INFILE_CLASS (gobject_class);

	gobject_class->finalize		= gsf_infile_zip_finalize;
	input_class->Dup		= gsf_infile_zip_dup;
	input_class->Read		= gsf_infile_zip_read;
	input_class->Seek		= gsf_infile_zip_seek;
	infile_class->num_children	= gsf_infile_zip_num_children;
	infile_class->name_by_index	= gsf_infile_zip_name_by_index;
	infile_class->child_by_index	= gsf_infile_zip_child_by_index;
	infile_class->child_by_name	= gsf_infile_zip_child_by_name;
}

GSF_CLASS (GsfInfileZip, gsf_infile_zip,
	   gsf_infile_zip_class_init, gsf_infile_zip_init,
	   GSF_INFILE_TYPE)

/**
 * gsf_infile_zip_new :
 * @source :
 * @err   :
 *
 * Opens the root directory of a Zip file.
 * NOTE : adds a reference to @source
 *
 * Returns : the new zip file handler
 **/
GsfInfile *
gsf_infile_zip_new (GsfInput *source, GError **err)
{
	GsfInfileZip *zip;

	g_return_val_if_fail (IS_GSF_INPUT (source), NULL);

	zip = g_object_new (GSF_INFILE_ZIP_TYPE, NULL);
	g_object_ref (G_OBJECT (source));
	zip->input = source;
	gsf_input_set_size (GSF_INPUT (zip), 0);

	if (zip_init_info (zip, err)) {
		g_object_unref (G_OBJECT (zip));
		return NULL;
	}

	return GSF_INFILE (zip);
}

/**
 * gsf_infile_zip_get_ooname :
 * @input :
 **/
char const *
gsf_infile_zip_get_ooname (GsfInput *input)
{
	return NULL;
}
