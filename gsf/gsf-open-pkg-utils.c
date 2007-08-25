/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-open-pkg-utils.c: Utilities for handling Open Package zip files
 * 			from MS Office 2007 or XPS.
 *
 * Copyright (C) 2006-2007 Jody Goldberg (jody@gnome.org)
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

#include <gsf-config.h>
#include <gsf/gsf-open-pkg-utils.h>
#include <gsf/gsf-input.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-outfile-impl.h>
#include <gsf/gsf-impl-utils.h>

#include <glib/gi18n-lib.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "libgsf:open_pkg"

enum {
	OPEN_PKG_NS_REL
};

static GsfXMLInNS const open_pkg_ns[] = {
	GSF_XML_IN_NS (OPEN_PKG_NS_REL,   "http://schemas.openxmlformats.org/package/2006/relationships"),
	GSF_XML_IN_NS_END
};

struct _GsfOpenPkgRel {
	char *id, *type, *target;
};

struct _GsfOpenPkgRels {
	GHashTable	*by_id;
	GHashTable	*by_type;
};

static void
gsf_open_pkg_rels_free (GsfOpenPkgRels *rels)
{
	g_hash_table_destroy (rels->by_id);
	g_hash_table_destroy (rels->by_type);
	g_free (rels);
}

static void
gsf_open_pkg_rel_free (GsfOpenPkgRel *rel)
{
	g_free (rel->id);	rel->id = NULL;
	g_free (rel->type);	rel->type = NULL;
	g_free (rel->target);	rel->target = NULL;
	g_free (rel);
}

static void
open_pkg_rel_begin (GsfXMLIn *xin, xmlChar const **attrs)
{
	GsfOpenPkgRels *rels = xin->user_state;
	GsfOpenPkgRel *rel;
	xmlChar const *id = NULL;
	xmlChar const *type = NULL;
	xmlChar const *target = NULL;

	for (; attrs != NULL && attrs[0] && attrs[1] ; attrs += 2)
		if (0 == strcmp (attrs[0], "Id"))
			id = attrs[1];
		else if (0 == strcmp (attrs[0], "Type"))
			type = attrs[1];
		else if (0 == strcmp (attrs[0], "Target"))
			target = attrs[1];

	g_return_if_fail (id != NULL);
	g_return_if_fail (type != NULL);
	g_return_if_fail (target != NULL);

	rel = g_new (GsfOpenPkgRel, 1);
	rel->id		= g_strdup (id);
	rel->type	= g_strdup (type);
	rel->target	= g_strdup (target);

	g_hash_table_replace (rels->by_id, rel->id, rel);
	g_hash_table_replace (rels->by_type, rel->type, rel);
}

static GsfXMLInNode const open_pkg_rel_dtd[] = {
GSF_XML_IN_NODE_FULL (START, START, -1, NULL, GSF_XML_NO_CONTENT, FALSE, TRUE, NULL, NULL, 0),
GSF_XML_IN_NODE_FULL (START, RELS, OPEN_PKG_NS_REL, "Relationships", GSF_XML_NO_CONTENT, FALSE, TRUE, NULL, NULL, 0),
  GSF_XML_IN_NODE (RELS, REL, OPEN_PKG_NS_REL, "Relationship", GSF_XML_NO_CONTENT, open_pkg_rel_begin, NULL),

GSF_XML_IN_NODE_END
};

/**
 * gsf_open_pkg_get_rels :
 * @in : #GsfInput
 *
 * Returns: a hashtable of the relationships associated with @in
 **/
static GsfOpenPkgRels *
gsf_open_pkg_get_rels (GsfInput *in)
{
	GsfOpenPkgRels *rels;

	g_return_val_if_fail (in != NULL, NULL);

	if (NULL == (rels = g_object_get_data (G_OBJECT (in), "OpenPkgRels"))) {
		char const *part_name = gsf_input_name (in);
		GsfXMLInDoc *rel_doc;
		GsfInput *rel_stream;

		if (NULL != part_name) {
			GsfInfile *container = gsf_input_container (in);
			char *rel_name;

			g_return_val_if_fail (container != NULL, NULL);

			rel_name = g_strconcat (part_name, ".rels", NULL);
			rel_stream = gsf_infile_child_by_vname (container, "_rels", rel_name, NULL);
			g_free (rel_name);
		} else /* the root */
			rel_stream = gsf_infile_child_by_vname (GSF_INFILE (in), "_rels", ".rels", NULL);

		g_return_val_if_fail (rel_stream != NULL, NULL);

		rels = g_new (GsfOpenPkgRels, 1);
		rels->by_id = g_hash_table_new_full (g_str_hash, g_str_equal,
			NULL, (GDestroyNotify)gsf_open_pkg_rel_free);
		rels->by_type = g_hash_table_new (g_str_hash, g_str_equal);

		rel_doc = gsf_xml_in_doc_new (open_pkg_rel_dtd, open_pkg_ns);
		(void) gsf_xml_in_doc_parse (rel_doc, rel_stream, rels);

		gsf_xml_in_doc_free (rel_doc);
		g_object_unref (G_OBJECT (rel_stream));

		g_object_set_data_full (G_OBJECT (in), "OpenPkgRels", rels,
			(GDestroyNotify) gsf_open_pkg_rels_free);
	}

	return rels;
}

static GsfInput *
gsf_open_pkg_open_rel (GsfInput *in, GsfOpenPkgRel *rel)
{
	GsfInfile *container, *prev;
	gchar **elems;
	unsigned i;

	g_return_val_if_fail (rel != NULL, NULL);
	g_return_val_if_fail (in != NULL, NULL);

	container = gsf_input_name (in)
		? gsf_input_container (in) : GSF_INFILE (in);

	/* parts can not have '/' in their names ? TODO : PROVE THIS
	 * right now the only test is that worksheets can not have it
	 * in their names */
	elems = g_strsplit (rel->target, "/", 0);
	for (i = 0 ; elems[i] ; i++) {
		prev = container;
		if (0 == strcmp (elems[i], "..")) {
			container = gsf_input_container (GSF_INPUT (container));

			g_return_val_if_fail (container != NULL, NULL);

			g_object_ref (container);
			in = NULL;
		} else if (0 == strcmp (elems[i], ".")) {
			in = NULL; /* Be pedantic and ignore '.' */
			continue;
		} else {
			in = gsf_infile_child_by_name (container, elems[i]);

			if (NULL != elems[i+1]) {
				g_return_val_if_fail (GSF_IS_INFILE (in), NULL);
				container = GSF_INFILE (in);
			}
		}
		if (i > 0)
			g_object_unref (G_OBJECT (prev));

	}
	g_strfreev (elems);

	return in;
}

GsfInput *
gsf_open_pkg_get_rel_by_id (GsfInput *in, char const *id)
{
	GsfOpenPkgRel *rel = NULL;
	GsfOpenPkgRels *rels = gsf_open_pkg_get_rels (in);

	g_return_val_if_fail (rels != NULL, NULL);

	if (NULL != (rel = g_hash_table_lookup (rels->by_id, id)))
		return gsf_open_pkg_open_rel (in, rel);
	return NULL;
}

GsfInput *
gsf_open_pkg_get_rel_by_type (GsfInput *in, char const *type)
{
	GsfOpenPkgRel *rel = NULL;
	GsfOpenPkgRels *rels = gsf_open_pkg_get_rels (in);

	g_return_val_if_fail (rels != NULL, NULL);

	if (NULL != (rel = g_hash_table_lookup (rels->by_type, type)))
		return gsf_open_pkg_open_rel (in, rel);
	return NULL;
}

/**
 * gsf_open_pkg_parse_rel_by_id :
 * @xin : #GsfXMLIn
 * @part_id :
 * @dtd : #GsfXMLInNode
 * @ns : #GsfXMLInNS
 *
 * Returns: NULL on success or a GError which callerss need to free on failure.
 **/
GError *
gsf_open_pkg_parse_rel_by_id (GsfXMLIn *xin, char const *part_id,
			      GsfXMLInNode const *dtd,
			      GsfXMLInNS const *ns)
{
	GError *res = NULL;
	GsfInput *cur_stream, *part_stream;

	g_return_val_if_fail (xin != NULL, NULL);

	cur_stream = gsf_xml_in_get_input (xin);

	if (NULL == part_id)
		return g_error_new (gsf_input_error_id(), gsf_open_pkg_error_id (),
			_("Missing id for part in '%s'"),
			gsf_input_name (cur_stream) );

	part_stream = gsf_open_pkg_get_rel_by_id (cur_stream, part_id);
	if (NULL != part_stream) {
		GsfXMLInDoc *doc = gsf_xml_in_doc_new (dtd, ns);

		if (!gsf_xml_in_doc_parse (doc, part_stream, xin->user_state))
			res = g_error_new (gsf_input_error_id(), gsf_open_pkg_error_id (),
				_("Part '%s' in '%s' from '%s' is corrupt!"),
				part_id,
				gsf_input_name (part_stream),
				gsf_input_name (cur_stream) );
		gsf_xml_in_doc_free (doc);

		g_object_unref (G_OBJECT (part_stream));
	} else
		res = g_error_new (gsf_input_error_id(), gsf_open_pkg_error_id (),
			_("Unable to find part '%s' for '%s'"),
			part_id, gsf_input_name (cur_stream) );
	return res;
}

/*************************************************************/

struct _GsfOutfileOpenPkg {
	GsfOutfile  parent;

	GsfOutput  *sink;
	gboolean    is_dir;
	char	   *content_type;
	GSList	   *children;
	GSList	   *relations;
};

typedef struct {
	char *id, *type, *target;
} GsfOutfileOpenPkgRel;

typedef GsfOutfileClass GsfOutfileOpenPkgClass;

enum {
	PROP_0,
	PROP_SINK,
	PROP_CONTENT_TYPE,
	PROP_IS_DIR,
	PROP_IS_ROOT
};

static GObjectClass *parent_class;

static void
gsf_outfile_open_pkg_get_property (GObject     *object,
				   guint        property_id,
				   GValue      *value,
				   GParamSpec  *pspec)
{
	GsfOutfileOpenPkg *open_pkg = (GsfOutfileOpenPkg *)object;

	switch (property_id) {
	case PROP_SINK:
		g_value_set_object (value, open_pkg->sink);
		break;
	case PROP_CONTENT_TYPE:
		g_value_set_string (value, open_pkg->content_type);
		break;
	case PROP_IS_DIR:
		g_value_set_boolean (value, open_pkg->is_dir);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gsf_outfile_open_pkg_set_property (GObject      *object,
				   guint         property_id,
				   GValue const *value,
				   GParamSpec   *pspec)
{
	GsfOutfileOpenPkg *open_pkg = (GsfOutfileOpenPkg *)object;

	switch (property_id) {
	case PROP_SINK:
		gsf_outfile_open_pkg_set_sink (open_pkg, g_value_get_object (value));
		break;
	case PROP_CONTENT_TYPE:
		gsf_outfile_open_pkg_set_content_type (open_pkg, g_value_get_string (value));
		break;
	case PROP_IS_DIR:
		open_pkg->is_dir = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gsf_outfile_open_pkg_init (GObject *obj)
{
	GsfOutfileOpenPkg *open_pkg = GSF_OUTFILE_OPEN_PKG (obj);

	open_pkg->sink = NULL;
	open_pkg->content_type = NULL;
	open_pkg->is_dir = FALSE;
	open_pkg->children = NULL;
	open_pkg->relations = NULL;
}

static void
gsf_outfile_open_pkg_finalize (GObject *obj)
{
	GsfOutfileOpenPkg *open_pkg = GSF_OUTFILE_OPEN_PKG (obj);
	GSList *ptr;

	if (open_pkg->sink != NULL) {
		g_object_unref (open_pkg->sink);
		open_pkg->sink = NULL;
	}
	g_free (open_pkg->content_type);
	open_pkg->content_type = NULL;

	for (ptr = open_pkg->children ; ptr != NULL ; ptr = ptr->next)
		g_object_unref (ptr->data);
	g_slist_free (open_pkg->children);
	parent_class->finalize (obj);
}

static gboolean
gsf_outfile_open_pkg_write (GsfOutput *output, size_t num_bytes, guint8 const *data)
{
	GsfOutfileOpenPkg *open_pkg = GSF_OUTFILE_OPEN_PKG (output);
	return gsf_output_write (open_pkg->sink, num_bytes, data);
}
static gboolean
gsf_outfile_open_pkg_seek (GsfOutput *output, gsf_off_t offset, GSeekType whence)
{
	GsfOutfileOpenPkg *open_pkg = GSF_OUTFILE_OPEN_PKG (output);
	return gsf_output_seek (open_pkg->sink, offset, whence);
}
static GsfOutput *
gsf_outfile_open_pkg_new_child (GsfOutfile *parent,
				char const *name, gboolean is_dir,
				char const *first_property_name, va_list args)
{
	GsfOutfileOpenPkg *child, *open_pkg = GSF_OUTFILE_OPEN_PKG (parent);
	GsfOutput *sink;

	if (!open_pkg->is_dir)
		return NULL;

	child = (GsfOutfileOpenPkg *)g_object_new_valist (
		GSF_OUTFILE_OPEN_PKG_TYPE, first_property_name, args);
	gsf_output_set_name (GSF_OUTPUT (child), name);
	gsf_output_set_container (GSF_OUTPUT (child), parent);
	child->is_dir = is_dir;

	sink = gsf_outfile_new_child (GSF_OUTFILE (open_pkg->sink), name, is_dir);
	gsf_outfile_open_pkg_set_sink (child, sink);
	g_object_unref (sink);

	open_pkg->children = g_slist_prepend (open_pkg->children, child);
	g_object_ref (child);

	return GSF_OUTPUT (child);
}

static void
gsf_open_pkg_write_content_default (GsfXMLOut *xml, char const *ext, char const *type)
{
	gsf_xml_out_start_element (xml, "Default");
	gsf_xml_out_add_cstr (xml, "Extension", ext);
	gsf_xml_out_add_cstr (xml, "ContentType", type);
	gsf_xml_out_end_element (xml); /* </Default> */
}
static void
gsf_open_pkg_write_content_override (GsfOutfileOpenPkg const *open_pkg,
				     char const *base,
				     GsfXMLOut *xml)
{
	GsfOutfileOpenPkg const *child;
	char   *path;
	GSList *ptr;

	for (ptr = open_pkg->children ; ptr != NULL ; ptr = ptr->next) {
		child = ptr->data;
		if (child->is_dir) {
			path = g_strconcat (base, gsf_output_name (GSF_OUTPUT (child)), "/", NULL);
			gsf_open_pkg_write_content_override (child, path, xml);
		} else {
			path = g_strconcat (base, gsf_output_name (GSF_OUTPUT (child)), NULL);
			/* rels files do need content types, the defaults handle them */
			if (NULL != child->content_type) {
				gsf_xml_out_start_element (xml, "Override");
				gsf_xml_out_add_cstr (xml, "PartName", path);
				gsf_xml_out_add_cstr (xml, "ContentType", child->content_type);
				gsf_xml_out_end_element (xml); /* </Override> */
			}
		}
		g_free (path);
	}
}

static gboolean
gsf_outfile_open_pkg_close (GsfOutput *output)
{
	GsfOutfileOpenPkg *open_pkg = GSF_OUTFILE_OPEN_PKG (output);
	GsfOutput *dir;
	gboolean res = FALSE;
	char *rels_name;

	if (NULL == open_pkg->sink || gsf_output_is_closed (open_pkg->sink))
		return TRUE;

	/* Generate [Content_types].xml when we close the root dir */
	if (NULL == gsf_output_name (output)) {
		GsfOutput *out = gsf_outfile_new_child (GSF_OUTFILE (open_pkg->sink),
			"[Content_Types].xml", FALSE);
		GsfXMLOut *xml = gsf_xml_out_new (out);

		gsf_xml_out_start_element (xml, "Types");
		gsf_xml_out_add_cstr_unchecked (xml, "xmlns",
			"http://schemas.openxmlformats.org/package/2006/content-types");
		gsf_open_pkg_write_content_default (xml, "rels",
			"application/vnd.openxmlformats-package.relationships+xml");
		gsf_open_pkg_write_content_default (xml, "xlbin",
			"application/vnd.openxmlformats-officedocument.spreadsheetml.printerSettings");
		gsf_open_pkg_write_content_default (xml, "xml",
			"application/xml");
		gsf_open_pkg_write_content_override (open_pkg, "/", xml);
		gsf_xml_out_end_element (xml); /* </Types> */
		g_object_unref (xml);

		gsf_output_close (out);
		g_object_unref (out);

		dir = open_pkg->sink;
		rels_name = g_strdup (".rels");
	} else {
		res = gsf_output_close (open_pkg->sink);

		dir = (GsfOutput *)gsf_output_container (open_pkg->sink);
		rels_name = g_strconcat (gsf_output_name (output), ".rels", NULL);
	}

	if (NULL != open_pkg->relations) {
		GsfOutput *rels;
		GsfXMLOut *xml;
		GsfOutfileOpenPkgRel *rel;
		GSList *ptr;

		dir = gsf_outfile_new_child (GSF_OUTFILE (dir), "_rels", TRUE);
		rels = gsf_outfile_new_child (GSF_OUTFILE (dir), rels_name, FALSE);
		xml = gsf_xml_out_new (rels);

		gsf_xml_out_start_element (xml, "Relationships");
		gsf_xml_out_add_cstr_unchecked (xml, "xmlns",
			"http://schemas.openxmlformats.org/package/2006/relationships");

		for (ptr = open_pkg->relations ; ptr != NULL ; ptr = ptr->next) {
			rel = ptr->data;
			gsf_xml_out_start_element (xml, "Relationship");
			gsf_xml_out_add_cstr (xml, "Id", rel->id);
			gsf_xml_out_add_cstr (xml, "Type", rel->type);
			gsf_xml_out_add_cstr (xml, "Target", rel->target);
			gsf_xml_out_end_element (xml); /* </Relationship> */

			g_free (rel->id);
			g_free (rel->type);
			g_free (rel->target);
			g_free (rel);
		}
		g_slist_free (open_pkg->relations);

		gsf_xml_out_end_element (xml); /* </Relationships> */
		g_object_unref (xml);
		gsf_output_close (rels);
		g_object_unref (rels);
		g_object_unref (dir);
	}
	g_free (rels_name);

	/* close the container */
	if (NULL == gsf_output_name (output))
		return gsf_output_close (open_pkg->sink);
	return res;
}

static void
gsf_outfile_open_pkg_class_init (GObjectClass *gobject_class)
{
	GsfOutputClass  *output_class  = GSF_OUTPUT_CLASS (gobject_class);
	GsfOutfileClass *outfile_class = GSF_OUTFILE_CLASS (gobject_class);

	gobject_class->finalize		= gsf_outfile_open_pkg_finalize;
	gobject_class->get_property     = gsf_outfile_open_pkg_get_property;
	gobject_class->set_property     = gsf_outfile_open_pkg_set_property;

	output_class->Write		= gsf_outfile_open_pkg_write;
	output_class->Seek		= gsf_outfile_open_pkg_seek;
	output_class->Close		= gsf_outfile_open_pkg_close;
	outfile_class->new_child	= gsf_outfile_open_pkg_new_child;

	parent_class = g_type_class_peek_parent (gobject_class);

	g_object_class_install_property (gobject_class, PROP_SINK,
		 g_param_spec_object ("sink", "Sink", "The GsfOutput that stores the Open Package content.",
			GSF_OUTFILE_TYPE, GSF_PARAM_STATIC | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (gobject_class, PROP_CONTENT_TYPE,
		 g_param_spec_string ("content-type", "ContentType", "The ContentType stored in the root [Content_Types].xml file.",
			"", GSF_PARAM_STATIC | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (gobject_class, PROP_IS_DIR,
		 g_param_spec_boolean ("is-dir", "IsDir", "Can the outfile have children.",
			FALSE, GSF_PARAM_STATIC | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

GSF_CLASS (GsfOutfileOpenPkg, gsf_outfile_open_pkg,
	   gsf_outfile_open_pkg_class_init, gsf_outfile_open_pkg_init,
	   GSF_OUTFILE_TYPE)

/**
 * gsf_outfile_open_pkg_new :
 * @sink : #GsfOutfile
 *
 * Convenience routine to create a GsfOutfileOpenPkg inside @sink.
 *
 * Returns: a GsfOutfile that the caller is responsible for.
 **/
GsfOutfile *
gsf_outfile_open_pkg_new (GsfOutfile *sink)
{
	return g_object_new (GSF_OUTFILE_OPEN_PKG_TYPE,
		"sink", sink,	"is-dir", TRUE,
		NULL);
}

/**
 * gsf_outfile_open_pkg_set_sink :
 * @open_pkg : #GsfOutfileOpenPkg
 * @sink : #GsfOutput
 *
 * Assigns a GsfOutput (@sink) to store the package into.
 **/
void
gsf_outfile_open_pkg_set_sink (GsfOutfileOpenPkg *open_pkg, GsfOutput *sink)
{
	if (sink)
		g_object_ref (sink);
	if (open_pkg->sink)
		g_object_unref (open_pkg->sink);
	open_pkg->sink = sink;
}

/**
 * gsf_outfile_open_pkg_set_content_type :
 * @open_pkg : #GsfOutfileOpenPkg
 * @content_type : 
 *
 **/
void
gsf_outfile_open_pkg_set_content_type (GsfOutfileOpenPkg *open_pkg,
				       char const *content_type)
{
	g_return_if_fail (content_type != NULL);

	if (open_pkg->content_type != content_type) {
		g_free (open_pkg->content_type);
		open_pkg->content_type = g_strdup (content_type);
	}
}

/**
 * gsf_outfile_open_pkg_relate:
 * @child : #GsfOutfileOpenPkg
 * @parent : #GsfOutfileOpenPkg
 * @type : 
 *
 * Create a relationship between @child and @parent of @type.
 * Return the relID which the caller does not owm but will live as long as
 * @parent.
 **/
char const *
gsf_outfile_open_pkg_relate (GsfOutfileOpenPkg *child,
			     GsfOutfileOpenPkg *parent,
			     char const *type)
{
	GsfOutfileOpenPkgRel *rel = g_new (GsfOutfileOpenPkgRel, 1);
	char *tmp, *path = g_strdup (gsf_output_name (GSF_OUTPUT (child)));
	GsfOutfile *ptr, *base = parent->is_dir ? GSF_OUTFILE (parent)
		: gsf_output_container (GSF_OUTPUT (parent));

	ptr = GSF_OUTFILE (child);
	while (NULL != (ptr = gsf_output_container (GSF_OUTPUT (ptr))) &&
	       ptr != base) {
		path = g_strconcat (gsf_output_name (GSF_OUTPUT (ptr)), "/",
			(tmp = path), NULL);
		g_free (tmp);
	}

	/* Calculate the path from @child to @parent */
	rel->id = g_strdup_printf ("rId%u", g_slist_length (parent->relations) + 1);
	rel->type = g_strdup (type);
	rel->target = path;
	parent->relations = g_slist_prepend (parent->relations, rel);
	return rel->id;
}

/**
 * gsf_outfile_open_pkg_add_rel:
 * @dir : #GsfOutfile
 * @name : 
 * @content_type :
 * @parent : #GsfOutfile
 * @type :
 *
 * A convenience wrapper to create a child in @dir of @content_type then create
 * a @type relation to @parent
 *
 * Returns: the new part.
 **/
GsfOutput *
gsf_outfile_open_pkg_add_rel (GsfOutfile *dir,
			 char const *name,
			 char const *content_type,
			 GsfOutfile *parent,
			 char const *type)
{
	GsfOutput *part = gsf_outfile_new_child_full (dir, name, FALSE,
		"content-type", content_type,
		NULL);
	(void) gsf_outfile_open_pkg_relate (GSF_OUTFILE_OPEN_PKG (part),
		GSF_OUTFILE_OPEN_PKG (parent), type);
	return part;
}

gint
gsf_open_pkg_error_id (void)
{
	return 42;	/* something arbitrary */
}

