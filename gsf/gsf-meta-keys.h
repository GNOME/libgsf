/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-meta-keys.h: a list of keys used with the meta-data bags
 *
 * Copyright (C) 2002 Dom Lachowicz (cinamod@hotmail.com)
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
#ifndef GSF_META_KEYS_H
#define GSF_META_KEYS_H

/* Proposed schema so that we can have some accessor and compose routines (getvendorname, getversion, gettag):
 * urn:VendorName:Version:Tag
 * Proposed default vendor names:
 * DublinCore (http://dublincore.org)
 * GSF (ourselves, remaining properties from OLE that don't map cleanly onto DublinCore)
 * Custom (user-defined namespace)
 *
 */

/* (String) A formal name given to the resource */
#define GSF_META_KEY_TITLE              "urn:DublinCore:1.0:Title"

/* (String) An entity primarily responsible for making the content of the resource
 * typically a person, organization, or service
 */
#define GSF_META_KEY_CREATOR            "urn:DublinCore:1.0:Creator"

/* (String) The topic of the content of the resource, *typically* including keywords */
#define GSF_META_KEY_SUBJECT            "urn:DublinCore:1.0:Subject"

/* (String) An account of the content of the resource */
#define GSF_META_KEY_DESCRIPTION        "urn:DublinCore:1.0:Description"

/* (String) An entity responsible for making the resource available */
#define GSF_META_KEY_PUBLISHER          "urn:DublinCore:1.0:Publisher"

/* (String) An entity responsible for making contributions to the content of the resource */
#define GSF_META_KEY_CONTRIBUTOR        "urn:DublinCore:1.0:Contributor"

/* (Date as ISO String) A date associated with an event in the life cycle of the resource (creation/publication date) */
#define GSF_META_KEY_DATE_CREATED       "urn:DublinCore:1.0:DateCreated"

/* (String) The nature or genre of the content of the resource
 * See http://dublincore.org/documents/dcmi-type-vocabulary/
 */
#define GSF_META_KEY_TYPE               "urn:DublinCore:1.0:Type"

/* (String) The physical or digital manifestation of the resource. mime-type-ish */
#define GSF_META_KEY_FORMAT             "urn:DublinCore:1.0:Format"

/* (String) A reference to a resource from which the present resource is derived */
#define GSF_META_KEY_SOURCE             "urn:DublinCore:1.0:Source"

/* (String) A language of the intellectual content of the resource (basically xx_YY form for us) */
#define GSF_META_KEY_LANGUAGE           "urn:DublinCore:1.0:Language"

/* (String) A reference to a related resource ("see-also") */
#define GSF_META_KEY_RELATION           "urn:DublinCore:1.0:Relation"

/* (String) The extent or scope of the content of the resource
 * spatial location, temporal period, or jurisdiction
 */
#define GSF_META_KEY_COVERAGE           "urn:DublinCore:1.0:Coverage"

/* (String) Information about rights held in and over the resource */
#define GSF_META_KEY_RIGHTS             "urn:DublinCore:1.0:Rights"

/* (String) Searchable, indexable keywords. Similar to PDF keywords or HTML's meta block */
#define GSF_META_KEY_KEYWORDS           "urn:GSF:1.0:Keywords"

/* (Date as ISO String) The last time this document was saved */
#define GSF_META_KEY_DATE_LAST_MODIFIED "urn:GSF:1.0:DateLastModified"

/* (Date as ISO String) The last time this document was printed */
#define GSF_META_KEY_DATE_LAST_PRINTED  "urn:GSF:1.0:DateLastPrinted"

/* (String) The creator (product) of this document. AbiWord, Gnumeric, etc...  */
#define GSF_META_KEY_GENERATOR          "urn:GSF:1.0:Generator"

/* (Integer) Count of pages in the document, if appropriate */
#define GSF_META_KEY_COUNT_PAGES        "urn:GSF:1.0:CountPages"

/* (Integer) Count of words in the document */
#define GSF_META_KEY_COUNT_WORDS        "urn:GSF:1.0:CountWords"

/* (Integer) Count of characters in the document */
#define GSF_META_KEY_COUNT_CHARACTERS   "urn:GSF:1.0:CountCharacters"

/* (String) Human-readable version description representing the document's current stage in its life-cycle.
 * Could be "1st Edition", "beta", a cvs revision tag, the number of times it has been saved, ...
 */
#define GSF_META_KEY_REVISION_TAG       "urn:GSF:1.0:RevisionTag"

#endif /* GSF_META_KEYS_H */
