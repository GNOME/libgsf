/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-regress-xmlin-unknown.c: GsfXMLIn unknown-element handler
 */

#include <gsf/gsf.h>
#include <gsf/gsf-libxml.h>
#include <gsf/gsf-input-memory.h>

#include "libgsf-regress.h"

#include <string.h>

typedef struct {
	int	root_end_calls;
	int	text_len_at_root_end;
	int	handler_calls;
} State;

static void
root_end (GsfXMLIn *xin, G_GNUC_UNUSED GsfXMLBlob *blob)
{
	State *st = xin->user_state;
	st->root_end_calls++;
	st->text_len_at_root_end = (int)xin->content->len;
}

static GsfXMLInNode const dtd[] = {
	GSF_XML_IN_NODE (ROOT, ROOT, -1, "r", GSF_XML_CONTENT, NULL, root_end),
	GSF_XML_IN_NODE_END
};

/* Claims to have handled everything, but pushes no state. */
static gboolean
unknown_handler (GsfXMLIn *xin, G_GNUC_UNUSED xmlChar const *elem,
		 G_GNUC_UNUSED xmlChar const **attrs)
{
	State *st = xin->user_state;
	st->handler_calls++;
	return TRUE;
}

/*
 * A handler that returns TRUE without pushing a state used to leave
 * start_element with no frame while end_element still popped one: the
 * parent's end callback ran early (at the unknown element's close tag) and
 * the parent's accumulated text was discarded.
 */
static void
test_handled_without_push (void)
{
	static char const xml[] =
		"<r>before<x/>after<y><z/></y></r>";
	GsfXMLInDoc *doc = gsf_xml_in_doc_new (dtd, NULL);
	GsfInput *in = gsf_input_memory_new ((guint8 const *)xml, sizeof xml - 1, FALSE);
	State st = { 0, 0, 0 };
	gboolean ok;

	gsf_xml_in_doc_set_unknown_handler (doc, unknown_handler);
	ok = gsf_xml_in_doc_parse (doc, in, &st);

	check (ok, "document parses");
	check (st.handler_calls >= 2, "handler was called for the unknowns (%d)",
	       st.handler_calls);
	check (st.root_end_calls == 1,
	       "root end callback runs exactly once (ran %d times)",
	       st.root_end_calls);
	check (st.text_len_at_root_end == (int)strlen ("beforeafter"),
	       "root content survives the unknown elements (len %d)",
	       st.text_len_at_root_end);

	g_object_unref (in);
	gsf_xml_in_doc_free (doc);
}

int
main (int argc G_GNUC_UNUSED, char **argv G_GNUC_UNUSED)
{
	gsf_init ();
	test_handled_without_push ();
	gsf_shutdown ();
	return regress_exit_status ();
}
