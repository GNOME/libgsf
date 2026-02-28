#include <gsf/gsf.h>

static const int VT_I2 = 2;

typedef struct {
	guint32		id;
	gsf_off_t	offset;
} GsfMSOleMetaDataProp;


static unsigned
msole_codepage_char_size (int codepage)
{
	return (codepage == 1200 || codepage == 1201
		? 2
		: 1);
}

static int
msole_prop_cmp (gconstpointer a, gconstpointer b)
{
	GsfMSOleMetaDataProp const *prop_a = a;
	GsfMSOleMetaDataProp const *prop_b = b;

	if (prop_a->offset < prop_b->offset)
		return -1;
	else if (prop_a->offset > prop_b->offset)
		return +1;
	else
		return 0;
}

/**
 * gsf_doc_meta_data_has_msole_unicode_wrong_format:
 * @in: #GsfInput
 *
 * Check if an MS OLE property stream contains Unicode dictionary entries
 * where the string length is stored as a byte count (old buggy format from
 * older libgsf versions) rather than a character count as required by
 * MS-OLEPS 2.16.
 *
 * Returns: %TRUE if wrong-format Unicode dictionary entries are detected,
 *   %FALSE otherwise (including on read errors or non-Unicode streams).
 **/
static gboolean
gsf_doc_meta_data_has_msole_unicode_wrong_format (GsfInput *in)
{
	guint8 const *data;
	guint16 version;
	guint32 os, num_sections;
	guint32 *sect_offsets = NULL;
	unsigned i, j;
	gboolean result = FALSE;
	gsf_off_t saved_pos;

	g_return_val_if_fail (GSF_IS_INPUT (in), FALSE);

	saved_pos = gsf_input_tell (in);

	if (gsf_input_size (in) <= 0)
		goto out;

	if (gsf_input_seek (in, 0, G_SEEK_SET))
		goto out;

	data = gsf_input_read (in, 28, NULL);
	if (NULL == data)
		goto out;

	os           = GSF_LE_GET_GUINT16 (data + 6);
	version      = GSF_LE_GET_GUINT16 (data + 2);
	num_sections = GSF_LE_GET_GUINT32 (data + 24);
	if (GSF_LE_GET_GUINT16 (data + 0) != 0xfffe
	    || (version != 0 && version != 1)
	    || os > 2
	    || num_sections > gsf_input_size(in) / 20
	    || num_sections > 100)
		goto out;

	/* Read all section offsets first, before seeking elsewhere */
	sect_offsets = g_new (guint32, num_sections);
	for (i = 0; i < num_sections; i++) {
		data = gsf_input_read (in, 20, NULL);
		if (NULL == data)
			goto out;
		sect_offsets[i] = GSF_LE_GET_GUINT32 (data + 16);
	}

	for (i = 0; i < num_sections; i++) {
		guint32 sect_offset = sect_offsets[i];
		guint32 sect_size, num_props;
		unsigned char_size = 1;

		/* Read section header */
		if (gsf_input_seek (in, sect_offset, G_SEEK_SET) ||
		    NULL == (data = gsf_input_read (in, 8, NULL)))
			goto out;

		sect_size = GSF_LE_GET_GUINT32 (data);
		num_props = GSF_LE_GET_GUINT32 (data + 4);

		if (num_props <= 0)
			continue;

		if ((num_props > gsf_input_remaining(in) / 8) || (sect_offset + sect_size > gsf_input_size(in)))
			goto out;

		/* Scan property ID/offset pairs to find codepage (id==1) and dictionary (id==0) */
		{
			guint32 codepage_offset = 0, dict_offset = 0;
			gboolean has_codepage = FALSE, has_dict = FALSE;
			guint32 dict_size;

			GsfMSOleMetaDataProp *props = g_new (GsfMSOleMetaDataProp, num_props);
			for (j = 0; j < num_props; j++) {
				if (NULL == (data = gsf_input_read (in, 8, NULL))) {
					g_free (props);
					goto out;
				}
				props[j].id = GSF_LE_GET_GUINT32 (data);
				props[j].offset = GSF_LE_GET_GUINT32 (data + 4);
				if (props[j].id == 1) {
					has_codepage = TRUE;
					codepage_offset = props[j].offset;
				} else if (props[j].id == 0) {
					has_dict = TRUE;
					dict_offset = props[j].offset;
				}
			}

			/* Read codepage directly from the stream */
			if (has_codepage) {
				if (gsf_input_seek (in, sect_offset + codepage_offset, G_SEEK_SET) == 0 &&
				    NULL != (data = gsf_input_read (in, 8, NULL))) {
					guint32 cp_type = GSF_LE_GET_GUINT32 (data);
					if (cp_type == VT_I2) {
						int codepage = GSF_LE_GET_GINT16 (data + 4);
						char_size = msole_codepage_char_size (codepage);
					}
				}
			}

			if (!has_dict || char_size != 2) {
				/* No dictionary or not Unicode, so no wrong-format issue possible */
				g_free (props);
				continue;
			}

			/* Find the end boundary for the dictionary property */
			qsort (props, num_props, sizeof (GsfMSOleMetaDataProp),
			       msole_prop_cmp);

			/* Compute dict_size from sorted property offsets */
			dict_size = sect_size - dict_offset;
			for (j = 0; j < num_props; j++) {
				if (props[j].offset > dict_offset) {
					dict_size = props[j].offset - dict_offset;
					break;
				}
			}
			g_free (props);

			/* Read and scan the dictionary */
			if (gsf_input_seek (in, sect_offset + dict_offset, G_SEEK_SET))
				goto out;

			data = gsf_input_read (in, dict_size, NULL);
			if (NULL == data)
				goto out;

			{
				guint8 const *start = data;
				guint8 const *end = data + dict_size;
				guint32 n, k;

				if (end - data < 4)
					continue;
				n = GSF_LE_GET_GUINT32 (data);
				/* skip entry count */
				data += 4;

				for (k = 0; k < n && data < end; k++) {
					guint32 len, char_count_bytes;
					gboolean byte_count_valid = FALSE;
					gboolean char_count_valid = FALSE;

					if (end - data < 8)
						break;
					/* skip id */
					data += 4;
					len = GSF_LE_GET_GUINT32 (data);
					data += 4;

					if (len == 0 || len >= 0x10000)
						break;

					char_count_bytes = len * char_size;

					/* Check byte count interpretation (wrong format) */
					if (len >= 2 && len <= (guint32)(end - data)) {
						guint8 const *pos = data + len - 2;
						if (pos[0] == 0 && pos[1] == 0)
							byte_count_valid = TRUE;
					}

					/* Check character count interpretation (correct format) */
					if (char_count_bytes >= 2 && char_count_bytes <= (guint32)(end - data)) {
						guint8 const *pos = data + char_count_bytes - 2;
						if (pos[0] == 0 && pos[1] == 0)
							char_count_valid = TRUE;
					}

					if (byte_count_valid && !char_count_valid) {
						result = TRUE;
						goto out;
					}

					/* Advance past this entry using best-guess length */
					if (char_count_valid)
						data += char_count_bytes;
					else if (byte_count_valid)
						data += len;
					else
						break;

					/* Padding to 4-byte boundary relative to dictionary start
					 * (matching msole_prop_read which uses (data - start) % 4) */
					if ((data - start) % 4)
						data += 4 - ((data - start) % 4);
				}
			}
		}
	}

out:
	g_free (sect_offsets);
	gsf_input_seek (in, saved_pos, G_SEEK_SET);
	return result;
}


static gboolean
test (const char *stream_name, GsfInfile *inf)
{
	gboolean res = FALSE;
	GsfInput *child = gsf_infile_child_by_name (inf, stream_name);
	if (child) {
		if (gsf_doc_meta_data_has_msole_unicode_wrong_format (child)) {
			g_printerr ("Stream %s has bad encoding\n", stream_name);
			res = TRUE;
		}
		g_object_unref (child);
	}

	return res;
}

int
main (int argc, char **argv)
{
	GsfInput *in = gsf_input_stdio_new (argv[1], NULL);
	GsfInfile *inf = gsf_infile_msole_new (in, NULL);
	gboolean bad = FALSE;

	if (test ("\05SummaryInformation", inf))
		bad = TRUE;
	if (test ("\05DocumentSummaryInformation", inf))
		bad = TRUE;

	g_object_unref (inf);
	g_object_unref (in);

	return bad ? 1 : 0;
}
