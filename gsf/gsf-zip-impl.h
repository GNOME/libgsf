/* THIS IS NOT INSTALLED */

/*
 * gsf-zip-impl.h:
 *
 * Copyright (C) 2002-2006 Tambet Ingo (tambet@ximian.com)
 * Copyright (C) 2014 Morten Welinder (terra@gnome.org)
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef GSF_ZIP_IMPL_H
#define GSF_ZIP_IMPL_H

#include <gsf/gsf-outfile-zip.h> /* for GsfZipCompressionMethod */

G_BEGIN_DECLS

/* Every member file is preceded by a header with this format.  */
#define ZIP_HEADER_SIGNATURE            0x04034b50
#define ZIP_HEADER_SIZE 		30
#define ZIP_HEADER_EXTRACT 		 4
#define ZIP_HEADER_FLAGS 	         6
#define ZIP_HEADER_COMP_METHOD           8
#define ZIP_HEADER_DOSTIME              10
#define ZIP_HEADER_CRC32		14
#define ZIP_HEADER_CSIZE		18
#define ZIP_HEADER_USIZE                22
#define ZIP_HEADER_NAME_SIZE		26
#define ZIP_HEADER_EXTRAS_SIZE		28

/* Members may have this record after the compressed data.  It is meant
   to be used only when it is not possible to seek back and patch the
   right values into the header.  */
#define ZIP_DDESC_SIGNATURE             0x08074b50
#define ZIP_DDESC_SIZE                  16
#define ZIP_DDESC_CRC32                 4
#define ZIP_DDESC_CSIZE                 8
#define ZIP_DDESC_USIZE                 12

/* 64-bit version of above.  Used when the ZIP64 extra field is present
   in the header.  */
#define ZIP_DDESC64_SIGNATURE           ZIP_DDESC_SIGNATURE
#define ZIP_DDESC64_SIZE                24
#define ZIP_DDESC64_CRC32               4
#define ZIP_DDESC64_CSIZE               8
#define ZIP_DDESC64_USIZE               16

/* The whole archive ends with a trailer.  */
#define ZIP_TRAILER_SIGNATURE           0x06054b50
#define ZIP_TRAILER_SIZE 		22
#define ZIP_TRAILER_DISK 		4
#define ZIP_TRAILER_DIR_DISK 		6
#define ZIP_TRAILER_ENTRIES 		8
#define ZIP_TRAILER_TOTAL_ENTRIES 	10
#define ZIP_TRAILER_DIR_SIZE 		12
#define ZIP_TRAILER_DIR_POS 		16
#define ZIP_TRAILER_COMMENT_SIZE	20

/* A zip64 locator comes immediately before the trailer, if it is present.  */
#define ZIP_ZIP64_LOCATOR_SIGNATURE     0x07064b50
#define ZIP_ZIP64_LOCATOR_SIZE 		20
#define ZIP_ZIP64_LOCATOR_DISK		4
#define ZIP_ZIP64_LOCATOR_OFFSET	8
#define ZIP_ZIP64_LOCATOR_DISKS		16

/* A zip64 archive has this record somewhere to extend the field sizes.  */
#define ZIP_TRAILER64_SIGNATURE         0x06064b50
#define ZIP_TRAILER64_SIZE 		56  /* or more */
#define ZIP_TRAILER64_RECSIZE            4
#define ZIP_TRAILER64_ENCODER   	12
#define ZIP_TRAILER64_EXTRACT   	14
#define ZIP_TRAILER64_DISK		16
#define ZIP_TRAILER64_DIR_DISK		20
#define ZIP_TRAILER64_ENTRIES		24
#define ZIP_TRAILER64_TOTAL_ENTRIES	32
#define ZIP_TRAILER64_DIR_SIZE 		40
#define ZIP_TRAILER64_DIR_POS 		48

/* This defines the entries in the central directory.  */
#define ZIP_DIRENT_SIGNATURE            0x02014b50
#define ZIP_DIRENT_SIZE                 46
#define ZIP_DIRENT_ENCODER              4
#define ZIP_DIRENT_EXTRACT              6
#define ZIP_DIRENT_FLAGS                8
#define ZIP_DIRENT_COMPR_METHOD         10
#define ZIP_DIRENT_DOSTIME              12
#define ZIP_DIRENT_CRC32                16
#define ZIP_DIRENT_CSIZE                20
#define ZIP_DIRENT_USIZE                24
#define ZIP_DIRENT_NAME_SIZE            28
#define ZIP_DIRENT_EXTRAS_SIZE          30
#define ZIP_DIRENT_COMMENT_SIZE         32
#define ZIP_DIRENT_DISKSTART            34
#define ZIP_DIRENT_FILE_TYPE            36
#define ZIP_DIRENT_FILE_MODE            38
#define ZIP_DIRENT_OFFSET               42

/* A few well-defined extra-field tags.  */
enum {
	ZIP_DIRENT_EXTRA_FIELD_ZIP64 = 0x0001,
	ZIP_DIRENT_EXTRA_FIELD_IGNORE = 0x4949,    /* "II" -- gsf defined */
	ZIP_DIRENT_EXTRA_FIELD_UNIXTIME = 0x5455,  /* "UT" */
	ZIP_DIRENT_EXTRA_FIELD_UIDGID = 0x7875    /* "ux" */
};

#define ZIP_DIRENT_FLAGS_HAS_DDESC 8

/* OS codes.  There are plenty, but this is all we need.  */
enum {
	ZIP_OS_MSDOS = 0,
	ZIP_OS_UNIX = 3
};

#define ZIP_NAME_SEPARATOR    '/'

#define ZIP_BLOCK_SIZE 32768
#define ZIP_BUF_SIZE 512

/* z_flags */
#define ZZIP_IS_ENCRYPTED(p)    ((*(unsigned char*)p)&1)
#define ZZIP_IS_COMPRLEVEL(p)  (((*(unsigned char*)p)>>1)&3)
#define ZZIP_IS_STREAMED(p)    (((*(unsigned char*)p)>>3)&1)

typedef struct {
	char                    *name;
	guint16                  flags;
	GsfZipCompressionMethod  compr_method;
	guint32                  crc32;
	gsf_off_t                csize;
	gsf_off_t                usize;
	gsf_off_t                offset;
	gsf_off_t                data_offset;
	guint32                  dostime;
	time_t                   mtime;
	gint8                    zip64;  /* -1 = auto, FALSE, TRUE. */
} GsfZipDirent;

typedef struct {
	char *name;
	gboolean is_directory;
	GsfZipDirent *dirent;
	GPtrArray *children;
	GSList *last_child; /* Unused */
} GsfZipVDir;

GType         gsf_zip_dirent_get_type (void);
GsfZipDirent *gsf_zip_dirent_new  (void);
void          gsf_zip_dirent_free (GsfZipDirent *dirent);

GType       gsf_zip_vdir_get_type (void);
GsfZipVDir *gsf_zip_vdir_new	(char const *name, gboolean is_directory,
				 GsfZipDirent *dirent);
void	    gsf_zip_vdir_free	(GsfZipVDir *vdir, gboolean free_dirent);
void	    gsf_zip_vdir_add_child	(GsfZipVDir *vdir, GsfZipVDir *child);
#ifndef GSF_DISABLE_DEPRECATED
G_DEPRECATED_FOR(gsf_zip_vdir_new)
GsfZipVDir *gsf_vdir_new	(char const *name, gboolean is_directory,
				 GsfZipDirent *dirent);
G_DEPRECATED_FOR(gsf_zip_vdir_free)
void	    gsf_vdir_free	(GsfZipVDir *vdir, gboolean free_dirent);
G_DEPRECATED_FOR(gsf_zip_vdir_add_child)
void	    gsf_vdir_add_child	(GsfZipVDir *vdir, GsfZipVDir *child);
#endif

G_END_DECLS

#endif /* GSF_ZIP_IMPL_H */
