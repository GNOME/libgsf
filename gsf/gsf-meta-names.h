/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-meta-names.h: a list of gsf-meta-names to "generically" represent 
 *                   all diviserly available implementation-specific
 *                   meta-names.
 *
 * Author:  Veerapuram Varadhan (vvaradhan@novell.com)
 *
 * Copyright (C) 2004 Novell, Inc
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */
#ifndef GSF_META_NAMES_H
#define GSF_META_NAMES_H

/**
 * GSF_META_NAME_TITLE:
 *
 * (String) A formal name given to the resource.
 */
#define GSF_META_NAME_TITLE                 "dc:title"

/**
 * GSF_META_NAME_DESCRIPTION:
 *
 * (String) An account of the content of the resource.
 */
#define GSF_META_NAME_DESCRIPTION           "dc:description"

/**
 * GSF_META_NAME_SUBJECT:
 *
 * (String) The topic of the content of the resource,
 * <emphasis>typically</emphasis> including keywords.
 */
#define GSF_META_NAME_SUBJECT               "dc:subject"

/**
 * GSF_META_NAME_DATE_MODIFIED:
 *
 * (Date as ISO String) The last time this document was saved.
 */
#define GSF_META_NAME_DATE_MODIFIED         "dc:date-modified"

/**
 * GSF_META_NAME_DATE_CREATED:
 *
 * (Date as ISO String) A date associated with an event in the life cycle of
 * the resource (creation/publication date).
 */
#define GSF_META_NAME_DATE_CREATED          "gsf:date-created"

/**
 * GSF_META_NAME_KEYWORDS:
 *
 * (String) Searchable, indexable keywords. Similar to PDF keywords or HTML's
 * meta block.
 */
#define GSF_META_NAME_KEYWORDS              "dc:keywords"

/**
 * GSF_META_NAME_LANGUAGE:
 *
 * (String) A language of the intellectual content of the resource (basically
 * xx_YY form for us).
 */
#define GSF_META_NAME_LANGUAGE              "dc:language"

/**
 * GSF_META_NAME_REVISION_COUNT:
 *
 * (Integer) Count of revision on the document, if appropriate.
 */
#define GSF_META_NAME_REVISION_COUNT        "gsf:revision-count"

/**
 * GSF_META_NAME_EDITING_DURATION:
 *
 * (Date as ISO String) The total-time taken until the last modification.
 */
#define GSF_META_NAME_EDITING_DURATION      "gsf:editing-duration"

/**
 * GSF_META_NAME_TABLE_COUNT:
 *
 * (Integer) Count of tables in the document, if appropriate.
 */
#define GSF_META_NAME_TABLE_COUNT           "gsf:table-count"

/**
 * GSF_META_NAME_IMAGE_COUNT:
 *
 * (Integer) Count of images in the document, if appropriate.
 */
#define GSF_META_NAME_IMAGE_COUNT           "gsf:image-count"

/**
 * GSF_META_NAME_OBJECT_COUNT:
 *
 * (Integer) Count of objects (OLE and other graphics) in the document, if
 * appropriate.
 */
#define GSF_META_NAME_OBJECT_COUNT          "gsf:object-count"

/**
 * GSF_META_NAME_PAGE_COUNT:
 *
 * (Integer) Count of pages in the document, if appropriate.
 */
#define GSF_META_NAME_PAGE_COUNT            "gsf:page-count"

/**
 * GSF_META_NAME_PARAGRAPH_COUNT:
 *
 * (Integer) Count of paragraphs in the document, if appropriate.
 */
#define GSF_META_NAME_PARAGRAPH_COUNT       "gsf:paragraph-count"

/**
 * GSF_META_NAME_WORD_COUNT:
 *
 * (Integer) Count of words in the document.
 */
#define GSF_META_NAME_WORD_COUNT            "gsf:word-count"

/**
 * GSF_META_NAME_CHARACTER_COUNT:
 *
 * (Integer) Count of characters in the document.
 */
#define GSF_META_NAME_CHARACTER_COUNT       "gsf:character-count"

/**
 * GSF_META_NAME_CELL_COUNT:
 *
 * (Integer) Count of cells in the spread-sheet document, if appropriate.
 */
#define GSF_META_NAME_CELL_COUNT            "gsf:cell-count"

/**
 * GSF_META_NAME_SPREADSHEET_COUNT:
 *
 * (Integer) Count of pages in the document, if appropriate.
 */
#define GSF_META_NAME_SPREADSHEET_COUNT     "gsf:spreadsheet-count"

/**
 * GSF_META_NAME_CREATOR:
 *
 * (String) An entity primarily responsible for making the content of the
 * resource typically a person, organization, or service.
 */
#define GSF_META_NAME_CREATOR               "gsf:creator"

/**
 * GSF_META_NAME_TEMPLATE:
 *
 * (String) The template file that is been used to generate this document.
 */
#define GSF_META_NAME_TEMPLATE              "gsf:template"

/* GSF_META_NAME_LAST_SAVED_BY:
 *
 * (String) The entity that made the last change to the document, typically a
 * person, organization, or service.
 */
#define GSF_META_NAME_LAST_SAVED_BY         "gsf:last-saved-by"

/**
 * GSF_META_NAME_LAST_PRINTED:
 *
 * (Date as ISO String) The last time this document was printed.
 */
#define GSF_META_NAME_LAST_PRINTED          "gsf:last-printed"

/**
 * GSF_META_NAME_SECURITY:
 *
 * (Integer) Level of security.
 *
 * <informaltable frame="none" role="params">
 * <tgroup cols="2">
 * <thead>
 * <row><entry align="left">Level</entry><entry>Value</entry></row>
 * </thead>
 * <tbody>
 * <row><entry>None</entry><entry>0</entry></row>
 * <row><entry>Password protected</entry><entry>1</entry></row>
 * <row><entry>Read-only recommended</entry><entry>2</entry></row>
 * <row><entry>Read-only enforced</entry><entry>3</entry></row>
 * <row><entry>Locked for annotations</entry><entry>4</entry></row>
 * </tbody></tgroup></informaltable>
 */
#define GSF_META_NAME_SECURITY			"gsf:security"

/**
 * GSF_META_NAME_CATEGORY
 *
 * (String) Category of the document. <note>example???</note>
 */
#define GSF_META_NAME_CATEGORY			"gsf:category"

/**
 * GSF_META_NAME_PRESENTATION_FORMAT:
 *
 * (String) Type of presentation, like "On-screen Show", "SlideView" etc.
 */
#define GSF_META_NAME_PRESENTATION_FORMAT	"gsf:presentation-format"

/**
 * GSF_META_NAME_THUMBNAIL:
 *
 * (Clipboard Format (VT_CF)) Thumbnail data of the document, typically a
 * preview image of the document.
 */
#define GSF_META_NAME_THUMBNAIL			"gsf:thumbnail"

/**
 * GSF_META_NAME_GENERATOR:
 *
 * (String) The creator (product) of this document. AbiWord, Gnumeric, etc...
 */
#define GSF_META_NAME_GENERATOR			"gsf:generator"

/**
 * GSF_META_NAME_LINE_COUNT:
 *
 * (Integer) Count of liness in the document.
 */
#define GSF_META_NAME_LINE_COUNT		"gsf:line-count"

/**
 * GSF_META_NAME_SLIDE_COUNT:
 *
 * (Integer) Count of slides in the presentation document.
 */
#define GSF_META_NAME_SLIDE_COUNT		"gsf:slide-count"

/**
 * GSF_META_NAME_NOTE_COUNT:
 *
 * (Integer) Count of "notes" in the document.
 */
#define GSF_META_NAME_NOTE_COUNT		"gsf:note-count"

/**
 * GSF_META_NAME_HIDDEN_SLIDE_COUNT:
 *
 * (Integer) Count of hidden-slides in the presentation document.
 */
#define GSF_META_NAME_HIDDEN_SLIDE_COUNT	"gsf:hidden-slide-count"

/**
 * GSF_META_NAME_MM_CLIP_COUNT:
 *
 * (Integer) Count of "multi-media" clips in the document.
 */
#define GSF_META_NAME_MM_CLIP_COUNT		"gsf:MM-clip-count"

/**
 * GSF_META_NAME_BYTE_COUNT:
 *
 * (Integer) Count of bytes in the document.
 */
#define GSF_META_NAME_BYTE_COUNT		"gsf:byte-count"

/* (Boolean) ????? */
#define GSF_META_NAME_SCALE			"gsf:scale"

/* (VT_VECTOR|VT_VARIANT) ??????? */
#define GSF_META_NAME_HEADING_PAIRS		"gsf:heading-pairs"

/* (VT_VECTOR|VT_LPSTR) ??????? */
/* In spreadsheets this is a list of the sheet names, and the named expressions */
#define GSF_META_NAME_DOCUMENT_PARTS		"gsf:document-parts"

/**
 * GSF_META_NAME_MANAGER:
 *
 * (String) Name of the manager of "CREATOR" entity.
 */
#define GSF_META_NAME_MANAGER			"gsf:manager"

/**
 * GSF_META_NAME_COMPANY:
 *
 * (String) Name of the company/organization that the "CREATOR" entity is
 * associated with.
 */
#define GSF_META_NAME_COMPANY			"gsf:company"

/* (Boolean) ??????? */
#define GSF_META_NAME_LINKS_DIRTY		"gsf:links-dirty"

/* (Unknown) User-defined names */
#define GSF_META_NAME_MSOLE_UNKNOWN_17		"msole:unknown-doc-17"
#define GSF_META_NAME_MSOLE_UNKNOWN_18		"msole:unknown-doc-18"
#define GSF_META_NAME_MSOLE_UNKNOWN_19		"msole:unknown-doc-19"	/* bool */
#define GSF_META_NAME_MSOLE_UNKNOWN_20		"msole:unknown-doc-20"
#define GSF_META_NAME_MSOLE_UNKNOWN_21		"msole:unknown-doc-21"
#define GSF_META_NAME_MSOLE_UNKNOWN_22		"msole:unknown-doc-22"	/* bool */
#define GSF_META_NAME_MSOLE_UNKNOWN_23		"msole:unknown-doc-23"	/* i4 */

/* (None) Reserved name (PID) for Dictionary */
#define GSF_META_NAME_DICTIONARY            "gsf:dictionary"

/**
 * GSF_META_NAME_LOCALE_SYSTEM_DEFAULT:
 *
 * (Unsigned Integer) Identifier representing the default system locale.
 */
#define GSF_META_NAME_LOCALE_SYSTEM_DEFAULT	"gsf:default-locale"

/**
 * GSF_META_NAME_CASE_SENSITIVE
 *
 * (Unsigned Integer) Identifier representing the case-sensitiveness.
 * <note>of what ?? why is it an integer ??</note>
 */
#define GSF_META_NAME_CASE_SENSITIVE		"gsf:case-sensitivity"

/**
 * GSF_META_NAME_INITIAL_CREATOR:
 *
 * (String) Specifies the name of the person who created the document
 * initially.
 */
#define GSF_META_NAME_INITIAL_CREATOR		"gsf:initial-creator"

/**
 * GSF_META_NAME_PRINTED_BY:
 *
 * (String) Specifies the name of the last person who printed the document.
 */
#define GSF_META_NAME_PRINTED_BY		"gsf:printed-by"

#endif /* GSF_META_NAMES_H */
