/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-outfile-zip.c: zip archive output.
 *
 * Copyright (C) 2002-2006 Jon K Hellan (hellan@acm.org)
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
 * Foundation, Outc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <gsf-config.h>
#include <gsf/gsf-outfile-zip.h>
#include <gsf/gsf.h>
#include <gsf/gsf-zip-impl.h>
#include <glib/gi18n-lib.h>
#include "gsf-priv.h"

#include <string.h>
#include <time.h>
#include <zlib.h>

#define ZIP_CREATE_DEFAULT_ZIP64 -1
#define ZIP_ADD_UNIXTIME_FIELD TRUE

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "libgsf:zip"

enum {
	PROP_0,
	PROP_SINK,
	PROP_ENTRY_NAME,
	PROP_COMPRESSION_METHOD,
        PROP_DEFLATE_LEVEL,
	PROP_ZIP64
};

static GObjectClass *parent_class;

struct _GsfOutfileZip {
	GsfOutfile parent;

	GsfOutput     *sink;
	GsfOutfileZip *root;

	gint8 sink_is_seekable;
	gint8 zip64;
	char *entry_name;

	GsfZipVDir    *vdir;
	GPtrArray     *root_order;	/* only valid for the root */

	z_stream  *stream;
	GsfZipCompressionMethod compression_method;
	gint deflate_level;

	gboolean   writing;

	guint8   *buf;
	size_t    buf_size;
};

typedef struct {
	GsfOutfileClass  parent_class;
} GsfOutfileZipClass;

#define GSF_OUTFILE_ZIP_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GSF_OUTFILE_ZIP_TYPE, GsfOutfileZipClass))
#define GSF_IS_OUTFILE_ZIP_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GSF_OUTFILE_ZIP_TYPE))

static void
disconnect_children (GsfOutfileZip *zip)
{
	unsigned i;

	if (!zip->root_order)
		return;

	for (i = 0 ; i < zip->root_order->len ; i++) {
		GsfOutfileZip *child =
			g_ptr_array_index (zip->root_order, i);
		if (child)
			g_object_unref (child);
	}
	g_ptr_array_free (zip->root_order, TRUE);
	zip->root_order = NULL;
}

static void
gsf_outfile_zip_finalize (GObject *obj)
{
	GsfOutfileZip *zip = GSF_OUTFILE_ZIP (obj);

	/* If the closing failed, we might have stuff here.  */
	disconnect_children (zip);

	if (zip->sink != NULL) {
		g_object_unref (zip->sink);
		zip->sink = NULL;
	}

	g_free (zip->entry_name);

	if (zip->stream)
		(void) deflateEnd (zip->stream);
	g_free (zip->stream);
	g_free (zip->buf);

	if (zip == zip->root)
		gsf_zip_vdir_free (zip->vdir, TRUE); /* Frees vdirs recursively */

	parent_class->finalize (obj);
}

static GObject *
gsf_outfile_zip_constructor (GType                  type,
			     guint                  n_construct_properties,
			     GObjectConstructParam *construct_params)
{
	GsfOutfileZip *zip =(GsfOutfileZip *)
		(parent_class->constructor (type,
					    n_construct_properties,
					    construct_params));

	if (!zip->entry_name) {
		zip->vdir = gsf_zip_vdir_new ("", TRUE, NULL);
		zip->root_order = g_ptr_array_new ();
		zip->root = zip;

		/* The names are the same */
		gsf_output_set_name (GSF_OUTPUT (zip), gsf_output_name (zip->sink));
		gsf_output_set_container (GSF_OUTPUT (zip), NULL);
	}

	if (!gsf_output_get_modtime (GSF_OUTPUT (zip))) {
		GDateTime *modtime = g_date_time_new_now_utc ();
		gsf_output_set_modtime (GSF_OUTPUT (zip), modtime);
		g_date_time_unref (modtime);
	}

	return (GObject *)zip;
}

static gboolean
gsf_outfile_zip_seek (G_GNUC_UNUSED GsfOutput *output,
		      G_GNUC_UNUSED gsf_off_t offset,
		      G_GNUC_UNUSED GSeekType whence)
{
	return FALSE;
}

/*
 * The "mimetype" member is special for ODF.  It cannot have any
 * extra field (and thus cannot be a zip64 member).  Hardcode
 * this to help compatibility with libgsf users depending on
 * past behaviour of zip creation.
 *
 * The flip side is that such a file cannot be 4G+.
 */
static gboolean
special_mimetype_dirent (const GsfZipDirent *dirent)
{
	return (dirent->offset == 0 &&
		dirent->zip64 != TRUE &&
		dirent->compr_method == GSF_ZIP_STORED &&
		strcmp (dirent->name, "mimetype") == 0);
}

static gboolean
zip_dirent_write (GsfOutfileZip *zip, const GsfZipDirent *dirent)
{
	guint8 buf[ZIP_DIRENT_SIZE];
	int nlen = strlen (dirent->name);
	gboolean ret;
	GString *extras = g_string_sized_new (ZIP_DIRENT_SIZE + nlen + 100);
	gboolean offset_in_zip64 = dirent->offset >= G_MAXUINT32;
	gboolean zip64_here = (dirent->zip64 || offset_in_zip64);
	const guint8 extract = zip64_here ? 45 : 20;  /* Unsure if dirent->zip64 is enough */

	if (zip64_here) {
		char tmp[8];

		/*
		 * We could unconditionally store the offset here, but
		 * zipinfo has a known bug in which it fails to account
		 * for differences in extra fields between the global
		 * and the local headers.  So we try to make them the
		 * same.
		 */
		GSF_LE_SET_GUINT16 (tmp, ZIP_DIRENT_EXTRA_FIELD_ZIP64);
		GSF_LE_SET_GUINT16 (tmp + 2, (2 + offset_in_zip64) * 8);
		g_string_append_len (extras, tmp, 4);
		GSF_LE_SET_GUINT64 (tmp, dirent->usize);
		g_string_append_len (extras, tmp, 8);
		GSF_LE_SET_GUINT64 (tmp, dirent->csize);
		g_string_append_len (extras, tmp, 8);
		if (offset_in_zip64) {
			GSF_LE_SET_GUINT64 (tmp, dirent->offset);
			g_string_append_len (extras, tmp, 8);
		}
	} else if (dirent->zip64 == -1) {
		char tmp[8];

		/* Match the local header.  */
		GSF_LE_SET_GUINT16 (tmp, ZIP_DIRENT_EXTRA_FIELD_IGNORE);
		GSF_LE_SET_GUINT16 (tmp + 2, 2 * 8);
		g_string_append_len (extras, tmp, 4);
		GSF_LE_SET_GUINT64 (tmp, (guint64)0);
		g_string_append_len (extras, tmp, 8);
		GSF_LE_SET_GUINT64 (tmp, (guint64)0);
		g_string_append_len (extras, tmp, 8);
	}

	if (ZIP_ADD_UNIXTIME_FIELD &&
	    dirent->mtime &&
	    dirent->mtime <= G_MAXUINT32 &&
	    !special_mimetype_dirent (dirent)) {
		/* Clearly a year 2038 problem here.  */
		char tmp[4];
		GSF_LE_SET_GUINT16 (tmp, ZIP_DIRENT_EXTRA_FIELD_UNIXTIME);
		GSF_LE_SET_GUINT16 (tmp + 2, 5);
		g_string_append_len (extras, tmp, 4);
		tmp[0] = 1;
		g_string_append_len (extras, tmp, 1);
		GSF_LE_SET_GUINT32 (tmp, dirent->mtime);
		g_string_append_len (extras, tmp, 4);
	}

	memset (buf, 0, sizeof buf);
	GSF_LE_SET_GUINT32 (buf, ZIP_DIRENT_SIGNATURE);
	GSF_LE_SET_GUINT16 (buf + ZIP_DIRENT_ENCODER,
			    (ZIP_OS_UNIX << 8) + extract);
	GSF_LE_SET_GUINT16 (buf + ZIP_DIRENT_EXTRACT,
			    (ZIP_OS_MSDOS << 8) + extract);
	GSF_LE_SET_GUINT16 (buf + ZIP_DIRENT_FLAGS, dirent->flags);
	GSF_LE_SET_GUINT16 (buf + ZIP_DIRENT_COMPR_METHOD,
			    dirent->compr_method);
	GSF_LE_SET_GUINT32 (buf + ZIP_DIRENT_DOSTIME, dirent->dostime);
	GSF_LE_SET_GUINT32 (buf + ZIP_DIRENT_CRC32, dirent->crc32);
	GSF_LE_SET_GUINT32 (buf + ZIP_DIRENT_CSIZE,
			    zip64_here ? G_MAXUINT32 : dirent->csize);
	GSF_LE_SET_GUINT32 (buf + ZIP_DIRENT_USIZE,
			    zip64_here ? G_MAXUINT32 : dirent->usize);
	GSF_LE_SET_GUINT16 (buf + ZIP_DIRENT_NAME_SIZE, nlen);
	GSF_LE_SET_GUINT16 (buf + ZIP_DIRENT_EXTRAS_SIZE, extras->len);
	GSF_LE_SET_GUINT16 (buf + ZIP_DIRENT_COMMENT_SIZE, 0);
	GSF_LE_SET_GUINT16 (buf + ZIP_DIRENT_DISKSTART, 0);
	GSF_LE_SET_GUINT16 (buf + ZIP_DIRENT_FILE_TYPE, 0);
	/* Hardcode file mode 644 */
	GSF_LE_SET_GUINT32 (buf + ZIP_DIRENT_FILE_MODE, 0100644u << 16);
	GSF_LE_SET_GUINT32 (buf + ZIP_DIRENT_OFFSET,
			    offset_in_zip64 ? G_MAXUINT32 : dirent->offset);

	/* Stuff everything into extras so we can do just one write.  */
	g_string_insert_len (extras,          0, buf, sizeof buf);
	g_string_insert_len (extras, sizeof buf, dirent->name, nlen);

	ret = gsf_output_write (zip->sink, extras->len, extras->str);

	g_string_free (extras, TRUE);

	return ret;
}

static gboolean
zip_trailer_write (GsfOutfileZip *zip, unsigned entries,
		   gsf_off_t dirpos, gsf_off_t dirsize)
{
	guint8 buf[ZIP_TRAILER_SIZE];

	memset (buf, 0, sizeof buf);
	GSF_LE_SET_GUINT32 (buf, ZIP_TRAILER_SIGNATURE);
	GSF_LE_SET_GUINT16 (buf + ZIP_TRAILER_ENTRIES,
			    MIN (entries, G_MAXUINT16));
	GSF_LE_SET_GUINT16 (buf + ZIP_TRAILER_TOTAL_ENTRIES,
			    MIN (entries, G_MAXUINT16));
	GSF_LE_SET_GUINT32 (buf + ZIP_TRAILER_DIR_SIZE,
			    MIN (dirsize, G_MAXUINT32));
	GSF_LE_SET_GUINT32 (buf + ZIP_TRAILER_DIR_POS,
			    MIN (dirpos, G_MAXUINT32));

	return gsf_output_write (zip->sink, sizeof buf, buf);
}

static gboolean
zip_trailer64_write (GsfOutfileZip *zip, unsigned entries,
		     gsf_off_t dirpos, gsf_off_t dirsize)
{
	guint8 buf[ZIP_TRAILER64_SIZE];
	guint8 extract = 45;

	memset (buf, 0, sizeof buf);
	GSF_LE_SET_GUINT32 (buf, ZIP_TRAILER64_SIGNATURE);
	GSF_LE_SET_GUINT64 (buf + ZIP_TRAILER64_RECSIZE, sizeof buf - 12);
	GSF_LE_SET_GUINT16 (buf + ZIP_TRAILER64_ENCODER,
			    (ZIP_OS_UNIX << 8) + extract);
	GSF_LE_SET_GUINT16 (buf + ZIP_TRAILER64_EXTRACT,
			    (ZIP_OS_MSDOS << 8) + extract);
	GSF_LE_SET_GUINT32 (buf + ZIP_TRAILER64_DISK, 0);
	GSF_LE_SET_GUINT32 (buf + ZIP_TRAILER64_DIR_DISK, 0);
	GSF_LE_SET_GUINT32 (buf + ZIP_TRAILER64_ENTRIES, entries);
	GSF_LE_SET_GUINT32 (buf + ZIP_TRAILER64_TOTAL_ENTRIES, entries);
	GSF_LE_SET_GUINT64 (buf + ZIP_TRAILER64_DIR_SIZE, dirsize);
	GSF_LE_SET_GUINT64 (buf + ZIP_TRAILER64_DIR_POS, dirpos);

	return gsf_output_write (zip->sink, sizeof buf, buf);
}

static gboolean
zip_zip64_locator_write (GsfOutfileZip *zip, gsf_off_t trailerpos)
{
	guint8 buf[ZIP_ZIP64_LOCATOR_SIZE];

	memset (buf, 0, sizeof buf);
	GSF_LE_SET_GUINT32 (buf, ZIP_ZIP64_LOCATOR_SIGNATURE);
	GSF_LE_SET_GUINT32 (buf + ZIP_ZIP64_LOCATOR_DISK, 0);
	GSF_LE_SET_GUINT64 (buf + ZIP_ZIP64_LOCATOR_OFFSET, trailerpos);
	GSF_LE_SET_GUINT32 (buf + ZIP_ZIP64_LOCATOR_DISKS, 1);

	return gsf_output_write (zip->sink, sizeof buf, buf);
}

static gint
offset_ordering (gconstpointer a_, gconstpointer b_)
{
	GsfOutfileZip *a = *(GsfOutfileZip **)a_;
	GsfOutfileZip *b = *(GsfOutfileZip **)b_;
	gsf_off_t diff = a->vdir->dirent->offset - b->vdir->dirent->offset;

	return diff < 0 ? -1 : diff > 0 ? +1 : 0;
}


static gboolean
zip_close_root (GsfOutput *output)
{
	GsfOutfileZip *zip = GSF_OUTFILE_ZIP (output);
	gsf_off_t dirpos = gsf_output_tell (zip->sink), dirend;
	GPtrArray *elem = zip->root_order;
	unsigned entries = elem->len;
	unsigned i;
	gint8 zip64 = zip->zip64;

	/* Check that children are closed */
	for (i = 0 ; i < elem->len ; i++) {
		GsfOutfileZip *child = g_ptr_array_index (elem, i);
		GsfZipDirent *dirent = child->vdir->dirent;
		if (dirent->zip64 == TRUE)
			zip64 = TRUE;
		if (!gsf_output_is_closed (GSF_OUTPUT (child))) {
			g_warning ("Child still open");
			return FALSE;
		}
	}

	if (1) {
		/*
		 * It is unclear whether we need this.  However, the
		 * zipdetails utility gets utterly confused if we do
		 * not.
		 *
		 * If we do not sort, we will use the ordering in which
		 * the members were actually being written.  Note, that
		 * merely creating the member doesn't count -- it's the
		 * actual writing (or closing an empty member) that
		 * counts.
		 */
		g_ptr_array_sort (elem, offset_ordering);
	}

	/* Write directory */
	dirpos = gsf_output_tell (zip->sink);
	for (i = 0 ; i < entries ; i++) {
		GsfOutfileZip *child = g_ptr_array_index (elem, i);
		GsfZipDirent *dirent = child->vdir->dirent;
		if (!zip_dirent_write (zip, dirent))
			return FALSE;
	}
	dirend = gsf_output_tell (zip->sink);

	if (entries >= G_MAXUINT16 ||
	    dirend >= G_MAXUINT32 - ZIP_TRAILER_SIZE) {
		/* We don't have a choice; force zip64.  */
		zip64 = TRUE;
	}

	disconnect_children (zip);

	if (zip64 == -1)
		zip64 = FALSE;

	if (zip64) {
		if (!zip_trailer64_write (zip, entries, dirpos, dirend - dirpos))
			return FALSE;
		if (!zip_zip64_locator_write (zip, dirend))
			return FALSE;
	}

	return zip_trailer_write (zip, entries, dirpos, dirend - dirpos);
}

static void
stream_name_write_to_buf (GsfOutfileZip *zip, GString *res)
{
	GsfOutput  *output = GSF_OUTPUT (zip);
	GsfOutfile *container;

	if (zip == zip->root)
		return;

	container = gsf_output_container (output);
	if (container) {
		stream_name_write_to_buf (GSF_OUTFILE_ZIP (container), res);
		if (res->len) {
			/* Forward slash is specified by the format.  */
			g_string_append_c (res, ZIP_NAME_SEPARATOR);
		}
	}

	if (zip->entry_name)
		g_string_append (res, zip->entry_name);
}

static char *
stream_name_build (GsfOutfileZip *zip)
{
	GString *str = g_string_sized_new (80);
	stream_name_write_to_buf (zip, str);
	return g_string_free (str, FALSE);
}

static guint32
zip_time_make (GDateTime *modtime)
{
	gint year, month, day, hour, minute, second;
	guint32 ztime;

	g_date_time_get_ymd (modtime, &year, &month, &day);
	hour = g_date_time_get_hour (modtime);
	minute = g_date_time_get_minute (modtime);
	second = g_date_time_get_second (modtime);

	if (year < 1980 || year > 1980 + 0x7f)
		ztime = 0;
	else {
		ztime = (year - 1980) & 0x7f;
		ztime = (ztime << 4) | (month  & 0x0f);
		ztime = (ztime << 5) | (day & 0x1f);
		ztime = (ztime << 5) | (hour & 0x1f);
		ztime = (ztime << 6) | (minute & 0x3f);
		ztime = (ztime << 5) | ((second / 2) & 0x1f);
	}

	return ztime;
}

static GsfZipDirent *
zip_dirent_new_out (GsfOutfileZip *zip)
{
	char *name = stream_name_build (zip);
	/*
	 * The spec is a bit vague about the length limit for file names, but
	 * clearly we should not go beyond 0xffff.
	 */
	if (strlen (name) < G_MAXUINT16) {
		GsfZipDirent *dirent = gsf_zip_dirent_new ();
		GDateTime *modtime = gsf_output_get_modtime (GSF_OUTPUT (zip));
		gint64 mtime64;

		dirent->name = name;
		dirent->compr_method = zip->compression_method;

		if (!modtime)
			modtime = g_date_time_new_now_utc ();
		else
			g_date_time_ref (modtime);
		dirent->dostime = zip_time_make (modtime);
		mtime64 = g_date_time_to_unix (modtime);
		if (mtime64 == (gint64)(time_t)mtime64)
		    dirent->mtime = (time_t)mtime64;

		dirent->zip64 = zip->zip64;

		g_date_time_unref (modtime);

		return dirent;
	} else
		return NULL;
}

static gboolean
zip_header_write (GsfOutfileZip *zip)
{
	guint8 hbuf[ZIP_HEADER_SIZE];
	GsfZipDirent *dirent = zip->vdir->dirent;
	char *name = dirent->name;
	int   nlen = strlen (name);
	gboolean ret;
	gboolean real_zip64, has_ddesc;
	guint32 crc32;
	gsf_off_t csize, usize;
	GString *extras;
	guint8 extract = 20;
	gboolean sig_written = FALSE;

	memset (hbuf, 0, sizeof hbuf);
	GSF_LE_SET_GUINT32 (hbuf, ZIP_HEADER_SIGNATURE);

	if (zip->sink_is_seekable == -1) {
		/*
		 * We need to figure out if the sink is seekable, but we lack
		 * an API to check that.  Instead, write the signature and
		 * try to seek back onto it.  If seeking back fails, just
		 * don't rewrite it.
		 */
		if (!gsf_output_write (zip->sink, 4, hbuf))
			return FALSE;
		zip->sink_is_seekable =
			gsf_output_seek (zip->sink, dirent->offset, G_SEEK_SET);
		if (!zip->sink_is_seekable)
			sig_written = TRUE;
	}

	/* Now figure out if we need a DDESC record.  */
	if (zip->sink_is_seekable)
		dirent->flags &= ~ZIP_DIRENT_FLAGS_HAS_DDESC;
	else
		dirent->flags |= ZIP_DIRENT_FLAGS_HAS_DDESC;
	has_ddesc = (dirent->flags & ZIP_DIRENT_FLAGS_HAS_DDESC) != 0;
	crc32 = has_ddesc ? 0 : dirent->crc32;
	csize = has_ddesc ? 0 : dirent->csize;
	usize = has_ddesc ? 0 : dirent->usize;

	/*
	 * Determine if we need a real zip64 extra field.  We do so, if
	 * - forced
	 * - in auto mode, if usize or csize has overflowed
	 * - in auto mode, if we use a DDESC
	 */
	real_zip64 =
		(dirent->zip64 == TRUE ||
		 (dirent->zip64 == -1 &&
		  (has_ddesc ||
		   dirent->usize >= G_MAXUINT32 ||
		   dirent->csize >= G_MAXUINT32)));

	if (real_zip64)
		extract = 45;

	extras = g_string_sized_new (ZIP_HEADER_SIZE + nlen + 100);

	/*
	 * In the has_ddesc case, we write crc32/size/usize as zero and store
	 * the right values in the DDESC record that follows the data.
	 *
	 * In the !has_ddesc case, we return to the same spot and write the
	 * header a second time correcting crc32/size/usize, see
	 * see zip_header_patch_sizes.  For this reason, we must ensure that
	 * the record's length does not depend on the the sizes.
	 *
	 * In the the has_ddesc case we store zeroes here.  No idea what we
	 * were supposed to write.
	 */
	if (dirent->zip64 /* auto or forced */) {
		char tmp[8];
		guint16 typ = real_zip64
			? ZIP_DIRENT_EXTRA_FIELD_ZIP64
			: ZIP_DIRENT_EXTRA_FIELD_IGNORE;

		GSF_LE_SET_GUINT16 (tmp, typ);
		GSF_LE_SET_GUINT16 (tmp + 2, 2 * 8);
		g_string_append_len (extras, tmp, 4);
		GSF_LE_SET_GUINT64 (tmp, usize);
		g_string_append_len (extras, tmp, 8);
		GSF_LE_SET_GUINT64 (tmp, csize);
		g_string_append_len (extras, tmp, 8);
	}

	if (ZIP_ADD_UNIXTIME_FIELD &&
	    dirent->mtime &&
	    dirent->mtime <= G_MAXUINT32 &&
	    !special_mimetype_dirent (dirent)) {
		/* Clearly a year 2038 problem here.  */
		char tmp[4];
		GSF_LE_SET_GUINT16 (tmp, ZIP_DIRENT_EXTRA_FIELD_UNIXTIME);
		GSF_LE_SET_GUINT16 (tmp + 2, 5);
		g_string_append_len (extras, tmp, 4);
		tmp[0] = 1;
		g_string_append_len (extras, tmp, 1);
		GSF_LE_SET_GUINT32 (tmp, dirent->mtime);
		g_string_append_len (extras, tmp, 4);
	}

	GSF_LE_SET_GUINT16 (hbuf + ZIP_HEADER_EXTRACT,
			    (ZIP_OS_MSDOS << 8) + extract);
	GSF_LE_SET_GUINT16 (hbuf + ZIP_HEADER_FLAGS, dirent->flags);
	GSF_LE_SET_GUINT16 (hbuf + ZIP_HEADER_COMP_METHOD,
			    dirent->compr_method);
	GSF_LE_SET_GUINT32 (hbuf + ZIP_HEADER_DOSTIME, dirent->dostime);
	GSF_LE_SET_GUINT32 (hbuf + ZIP_HEADER_CRC32, crc32);
	GSF_LE_SET_GUINT32 (hbuf + ZIP_HEADER_CSIZE,
			    real_zip64 && !has_ddesc ? G_MAXUINT32 : csize);
	GSF_LE_SET_GUINT32 (hbuf + ZIP_HEADER_USIZE,
			    real_zip64 && !has_ddesc ? G_MAXUINT32 : usize);
	GSF_LE_SET_GUINT16 (hbuf + ZIP_HEADER_NAME_SIZE, nlen);
	GSF_LE_SET_GUINT16 (hbuf + ZIP_HEADER_EXTRAS_SIZE, extras->len);

	/* Stuff everything into extras so we can do just one write.  */
	g_string_insert_len (extras,           0, hbuf, sizeof hbuf);
	g_string_insert_len (extras, sizeof hbuf, name, nlen);
	if (sig_written)
		g_string_erase (extras, 0, 4);

	ret = gsf_output_write (zip->sink, extras->len, extras->str);

	g_string_free (extras, TRUE);

	if (real_zip64)
		dirent->zip64 = TRUE;

	return ret;
}

static gboolean
zip_init_write (GsfOutput *output)
{
	GsfOutfileZip *zip = GSF_OUTFILE_ZIP (output);
	GsfZipDirent *dirent;
	int      ret;

	if (zip->root->writing) {
		g_warning ("Already writing to another stream in archive");
		return FALSE;
	}

	if (!gsf_output_wrap (G_OBJECT (output), zip->sink))
		return FALSE;

	dirent = zip_dirent_new_out (zip);
	if (!dirent) {
		gsf_output_unwrap (G_OBJECT (output), zip->sink);
		return FALSE;
	}

	dirent->offset = gsf_output_tell (zip->sink);
	if (special_mimetype_dirent (dirent))
		dirent->zip64 = FALSE;

	zip->vdir->dirent = dirent;
	zip_header_write (zip);
	zip->writing = TRUE;
	zip->root->writing = TRUE;
	dirent->crc32 = crc32 (0L, Z_NULL, 0);
	if (zip->compression_method == GSF_ZIP_DEFLATED) {
		if (!zip->stream) {
			zip->stream = g_new0 (z_stream, 1);
		}
		ret = deflateInit2 (zip->stream, zip->deflate_level,
				    Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL,
				    Z_DEFAULT_STRATEGY);
		if (ret != Z_OK)
			return FALSE;
		if (!zip->buf) {
			zip->buf_size = ZIP_BUF_SIZE;
			zip->buf = g_new (guint8, zip->buf_size);
		}
		zip->stream->next_out  = zip->buf;
		zip->stream->avail_out = zip->buf_size;
	}

	return TRUE;
}

static gboolean
zip_output_block (GsfOutfileZip *zip)
{
	size_t num_bytes = zip->buf_size - zip->stream->avail_out;
	GsfZipDirent *dirent = zip->vdir->dirent;

	if (!gsf_output_write (zip->sink, num_bytes, zip->buf)) {
		return FALSE;
	}
	dirent->csize += num_bytes;
	if (dirent->zip64 == FALSE && dirent->csize >= G_MAXUINT32)
		return FALSE;

	zip->stream->next_out  = zip->buf;
	zip->stream->avail_out = zip->buf_size;

	return TRUE;
}

static gboolean
zip_flush (GsfOutfileZip *zip)
{
	int zret;

	do {
		zret = deflate (zip->stream, Z_FINISH);
		if (zret == Z_OK || (zret == Z_BUF_ERROR && zip->stream->avail_out == 0)) {
			/*  In this case Z_OK or Z_BUF_ERROR means more buffer
			    space is needed  */
			if (!zip_output_block (zip))
				return FALSE;
		}
	} while (zret == Z_OK || zret == Z_BUF_ERROR);
	if (zret != Z_STREAM_END)
		return FALSE;
	if (!zip_output_block (zip))
		return FALSE;

	return TRUE;
}

/* Write the per stream data descriptor */
static gboolean
zip_ddesc_write (GsfOutfileZip *zip)
{
	guint8 buf[MAX (ZIP_DDESC_SIZE, ZIP_DDESC64_SIZE)];
	GsfZipDirent *dirent = zip->vdir->dirent;
	size_t size;

	/* Documentation says signature is not official.  */

	if (dirent->zip64) {
		GSF_LE_SET_GUINT32 (buf, ZIP_DDESC64_SIGNATURE);
		GSF_LE_SET_GUINT32 (buf + ZIP_DDESC64_CRC32, dirent->crc32);
		GSF_LE_SET_GUINT64 (buf + ZIP_DDESC64_CSIZE, dirent->csize);
		GSF_LE_SET_GUINT64 (buf + ZIP_DDESC64_USIZE, dirent->usize);
		size = ZIP_DDESC64_SIZE;
	} else {
		GSF_LE_SET_GUINT32 (buf, ZIP_DDESC_SIGNATURE);
		GSF_LE_SET_GUINT32 (buf + ZIP_DDESC_CRC32, dirent->crc32);
		GSF_LE_SET_GUINT32 (buf + ZIP_DDESC_CSIZE, dirent->csize);
		GSF_LE_SET_GUINT32 (buf + ZIP_DDESC_USIZE, dirent->usize);
		size = ZIP_DDESC_SIZE;
	}

	if (!gsf_output_write (zip->sink, size, buf)) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
zip_header_patch_sizes (GsfOutfileZip *zip)
{
	GsfZipDirent *dirent = zip->vdir->dirent;
	gsf_off_t pos = gsf_output_tell (zip->sink);
	gboolean ok;

	/* Rewrite the header in the same location again.  */
	ok = (gsf_output_seek (zip->sink, dirent->offset, G_SEEK_SET) &&
	      zip_header_write (zip) &&
	      gsf_output_seek (zip->sink, pos, G_SEEK_SET));

	if (ok && dirent->zip64 == -1) {
		/*
		 * We just wrote the final header.  Since we still are in
		 * auto-mode, the header did not use a real zip64 extra
		 * field.  Hence we don't need such a field.
		 */
		dirent->zip64 = FALSE;
	}

	return ok;
}

static gboolean
zip_close_stream (GsfOutput *output)
{
	GsfOutfileZip *zip = GSF_OUTFILE_ZIP (output);
	gboolean result;

	if (!zip->writing)
		if (!zip_init_write (output))
			return FALSE;

	if (zip->compression_method == GSF_ZIP_DEFLATED) {
		if (!zip_flush (zip))
			return FALSE;
	}

	if (zip->vdir->dirent->flags & ZIP_DIRENT_FLAGS_HAS_DDESC) {
		if (!zip_ddesc_write (zip)) /* Write data descriptor */
			return FALSE;
	} else {
		if (!zip_header_patch_sizes (zip)) /* Write crc, sizes */
			return FALSE;
	}
	zip->root->writing = FALSE;

	result = gsf_output_unwrap (G_OBJECT (output), zip->sink);

	/* Free unneeded memory */
	if (zip->stream) {
		(void) deflateEnd (zip->stream);
		g_free (zip->stream);
		zip->stream = NULL;
		g_free (zip->buf);
		zip->buf = NULL;
	}

	return result;
}

static gboolean
gsf_outfile_zip_close (GsfOutput *output)
{
	GsfOutfileZip *zip = GSF_OUTFILE_ZIP (output);
	gboolean ret;

	/* The root dir */
	if (zip == zip->root)
		ret = zip_close_root (output);
	else if (zip->vdir->is_directory)
		/* Directories: Do nothing. Should change this to actually
		 * write dirs which don't have children. */
		ret = TRUE;
	else
		ret = zip_close_stream (output);

	return ret;
}

static gboolean
gsf_outfile_zip_write (GsfOutput *output,
		       size_t num_bytes, guint8 const *data)
{
	GsfOutfileZip *zip = GSF_OUTFILE_ZIP (output);
	GsfZipDirent *dirent;
	int ret;

	g_return_val_if_fail (zip && zip->vdir, FALSE);
	g_return_val_if_fail (!zip->vdir->is_directory, FALSE);
	g_return_val_if_fail (data, FALSE);

	if (!zip->writing)
		if (!zip_init_write (output))
			return FALSE;

	dirent = zip->vdir->dirent;

	if (dirent->zip64 == FALSE &&
	    (num_bytes >= G_MAXUINT32 ||
	     gsf_output_tell (output) >= (gsf_off_t)(G_MAXUINT32 - num_bytes))) {
		/* Uncompressed size field would overflow.  */
		return FALSE;
	}

	if (zip->compression_method == GSF_ZIP_DEFLATED) {
		zip->stream->next_in  = (unsigned char *) data;
		zip->stream->avail_in = num_bytes;

		while (zip->stream->avail_in > 0) {
			if (zip->stream->avail_out == 0) {
				if (!zip_output_block (zip))
					return FALSE;
			}
			ret = deflate (zip->stream, Z_NO_FLUSH);
			if (ret != Z_OK)
				return FALSE;
		}
	} else {
		if (!gsf_output_write (zip->sink, num_bytes, data))
			return FALSE;
		dirent->csize += num_bytes;
	}
	dirent->crc32 = crc32 (dirent->crc32, data, num_bytes);
	dirent->usize += num_bytes;

	return TRUE;
}

static void
root_register_child (GsfOutfileZip *root, GsfOutfileZip *child)
{
	child->root = root;
	if (!child->vdir->is_directory) {
		g_object_ref (child);
		g_ptr_array_add (root->root_order, child);
	}
}

static void
gsf_outfile_zip_set_sink (GsfOutfileZip *zip, GsfOutput *sink)
{
	if (sink)
		g_object_ref (sink);
	if (zip->sink)
		g_object_unref (zip->sink);
	zip->sink = sink;
	zip->sink_is_seekable = -1;
}

static GsfOutput *
gsf_outfile_zip_new_child (GsfOutfile *parent,
			   char const *name, gboolean is_dir,
			   char const *first_property_name, va_list args)
{
	GsfOutfileZip *zip_parent = (GsfOutfileZip *)parent;
	GsfOutfileZip *child;
	size_t n_params = 0;
	GsfParam *params = NULL;
	char *display_name;
	const char **names;
	GValue *values;

	g_return_val_if_fail (zip_parent != NULL, NULL);
	g_return_val_if_fail (zip_parent->vdir, NULL);
	g_return_val_if_fail (zip_parent->vdir->is_directory, NULL);
	g_return_val_if_fail (name && *name, NULL);

	gsf_prop_settings_collect (GSF_OUTFILE_ZIP_TYPE,
				   &params, &n_params,
				   "sink", zip_parent->sink,
				   "entry-name", name,
				   NULL);
	gsf_prop_settings_collect_valist (GSF_OUTFILE_ZIP_TYPE,
					  &params, &n_params,
					  first_property_name,
					  args);
	if (!gsf_prop_settings_find ("modtime", params, n_params))
		gsf_prop_settings_collect (GSF_OUTFILE_ZIP_TYPE,
					   &params, &n_params,
					   "modtime", gsf_output_get_modtime (GSF_OUTPUT (parent)),
					   NULL);

	gsf_params_to_properties (params, n_params, &names, &values);

	child = (GsfOutfileZip *)g_object_new_with_properties (GSF_OUTFILE_ZIP_TYPE,
							       n_params,
							       names,
							       values);
	gsf_prop_settings_free (params, n_params);
	g_free (names);
	g_free (values);

	child->zip64 = zip_parent->zip64;
	child->vdir = gsf_zip_vdir_new (name, is_dir, NULL);

	/* FIXME: It isn't clear what encoding name is in.  */
	display_name = g_filename_display_name (name);
	gsf_output_set_name (GSF_OUTPUT (child), display_name);
	g_free (display_name);

	gsf_output_set_container (GSF_OUTPUT (child), parent);
	gsf_zip_vdir_add_child (zip_parent->vdir, child->vdir);
	root_register_child (zip_parent->root, child);

	return GSF_OUTPUT (child);
}

static void
gsf_outfile_zip_init (GObject *obj)
{
	GsfOutfileZip *zip = GSF_OUTFILE_ZIP (obj);

	zip->sink = NULL;
	zip->sink_is_seekable = -1;
	zip->root = NULL;
	zip->entry_name = NULL;
	zip->zip64 = ZIP_CREATE_DEFAULT_ZIP64;
	zip->vdir = NULL;
	zip->root_order = NULL;
	zip->stream = NULL;
	zip->compression_method = GSF_ZIP_DEFLATED;
	zip->deflate_level = Z_DEFAULT_COMPRESSION;
	zip->writing = FALSE;
	zip->buf = NULL;
	zip->buf_size = 0;
}

static void
gsf_outfile_zip_get_property (GObject     *object,
			      guint        property_id,
			      GValue      *value,
			      GParamSpec  *pspec)
{
	GsfOutfileZip *zip = (GsfOutfileZip *)object;

	switch (property_id) {
	case PROP_SINK:
		g_value_set_object (value, zip->sink);
		break;
	case PROP_ENTRY_NAME:
		g_value_set_string (value, zip->entry_name);
		break;
	case PROP_COMPRESSION_METHOD:
		g_value_set_int (value,
				 zip->vdir->dirent
				 ? zip->vdir->dirent->compr_method
				 : 0);
		break;
	case PROP_DEFLATE_LEVEL:
                g_value_set_int (value, zip->deflate_level);
		break;
	case PROP_ZIP64:
                g_value_set_int (value, zip->zip64);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gsf_outfile_zip_set_property (GObject      *object,
			      guint         property_id,
			      GValue const *value,
			      GParamSpec   *pspec)
{
	GsfOutfileZip *zip = (GsfOutfileZip *)object;

	switch (property_id) {
	case PROP_SINK:
		gsf_outfile_zip_set_sink (zip, g_value_get_object (value));
		break;
	case PROP_ENTRY_NAME:
		zip->entry_name = g_strdup (g_value_get_string (value));
		break;
	case PROP_COMPRESSION_METHOD: {
		int method = g_value_get_int (value);
		switch (method) {
		case GSF_ZIP_STORED:
		case GSF_ZIP_DEFLATED:
			zip->compression_method = method;
			break;
		default:
			g_warning ("Unsupported compression level %d", method);
		}
		break;
	}
	case PROP_DEFLATE_LEVEL: {
		int level = g_value_get_int (value);
                if (level == Z_DEFAULT_COMPRESSION || (level >= 0 && level <= 9))
			zip->deflate_level = level;
                else
			g_warning ("Unsupported deflate level %d", level);
	        }
		break;
	case PROP_ZIP64:
		zip->zip64 = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gsf_outfile_zip_class_init (GObjectClass *gobject_class)
{
	GsfOutputClass  *output_class  = GSF_OUTPUT_CLASS (gobject_class);
	GsfOutfileClass *outfile_class = GSF_OUTFILE_CLASS (gobject_class);

	gobject_class->constructor      = gsf_outfile_zip_constructor;
	gobject_class->finalize		= gsf_outfile_zip_finalize;
	gobject_class->get_property     = gsf_outfile_zip_get_property;
	gobject_class->set_property     = gsf_outfile_zip_set_property;

	output_class->Write		= gsf_outfile_zip_write;
	output_class->Seek		= gsf_outfile_zip_seek;
	output_class->Close		= gsf_outfile_zip_close;
	outfile_class->new_child	= gsf_outfile_zip_new_child;

	parent_class = g_type_class_peek_parent (gobject_class);

	g_object_class_install_property
		(gobject_class,
		 PROP_SINK,
		 g_param_spec_object ("sink",
				      _("Sink"),
				      _("Where the archive is written"),
				      GSF_OUTPUT_TYPE,
				      G_PARAM_STATIC_STRINGS |
				      G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property
		(gobject_class,
		 PROP_ENTRY_NAME,
		 g_param_spec_string ("entry-name",
				      _("Entry Name"),
				      _("The filename of this member in the archive without path"),
				      NULL,
				      G_PARAM_STATIC_STRINGS |
				      G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property
		(gobject_class,
		 PROP_COMPRESSION_METHOD,
		 g_param_spec_int ("compression-level",
				   _("Compression Level"),
				   _("The level of compression used, zero meaning none"),
				   0, 10,
				   GSF_ZIP_DEFLATED,
				   G_PARAM_STATIC_STRINGS |
				   G_PARAM_READWRITE |
				   G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property
		(gobject_class,
		 PROP_DEFLATE_LEVEL,
		 g_param_spec_int ("deflate-level",
				   _("Deflate Level"),
				   _("The level of deflate compression used, zero meaning none "
				     "and -1 meaning the zlib default"),
				   -1, 9,
				   Z_DEFAULT_COMPRESSION,
				   G_PARAM_STATIC_STRINGS |
				   G_PARAM_READWRITE |
				   G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property
		(gobject_class,
		 PROP_ZIP64,
		 g_param_spec_int ("zip64",
				   _("Zip64"),
				   _("Whether to use zip64 format, -1 meaning automatic"),
				   -1, 1,
				   ZIP_CREATE_DEFAULT_ZIP64,
				   G_PARAM_STATIC_STRINGS |
				   G_PARAM_READWRITE |
				   G_PARAM_CONSTRUCT_ONLY));
}

GSF_CLASS (GsfOutfileZip, gsf_outfile_zip,
	   gsf_outfile_zip_class_init, gsf_outfile_zip_init,
	   GSF_OUTFILE_TYPE)

/**
 * gsf_outfile_zip_new:
 * @sink: a #GsfOutput to hold the ZIP file
 * @err: Location to store error, or %NULL; currently unused.
 *
 * Creates the root directory of a Zip file and manages the addition of
 * children.
 *
 * <note>This adds a reference to @sink.</note>
 *
 * Returns: the new zip file handler
 **/
GsfOutfile *
gsf_outfile_zip_new (GsfOutput *sink, GError **err)
{
	g_return_val_if_fail (GSF_IS_OUTPUT (sink), NULL);

	if (err)
		*err = NULL;

	return (GsfOutfile *)g_object_new (GSF_OUTFILE_ZIP_TYPE,
					   "sink", sink,
					   NULL);
}

/* deprecated has no effect */
gboolean
gsf_outfile_zip_set_compression_method (G_GNUC_UNUSED GsfOutfileZip *zip,
					G_GNUC_UNUSED GsfZipCompressionMethod method)
{
	return TRUE;
}
