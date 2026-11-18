/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * libgsf-regress.h: shared helpers for the libgsf regression tests
 */

#ifndef GSF_REGRESS_H
#define GSF_REGRESS_H

#include <gsf/gsf.h>

#include <glib.h>
#include <stdio.h>
#include <string.h>

G_BEGIN_DECLS

/*
 * Record the outcome of one check.  @what is a printf-style format with
 * optional arguments.  Failures are accumulated in the library; call
 * regress_exit_status() at the end of main() to print the summary and get
 * the process exit status.
 */
void check (gboolean ok, char const *what, ...) G_GNUC_PRINTF (2, 3);

/* Print the summary ("all checks passed" / "*** N check(s) failed") and
 * return the process exit status (0 or 1). */
int regress_exit_status (void);

/* Silence GLib log messages (e.g. expected warnings from malformed input)
 * by installing a no-op default log handler; restore_g_log_messages() puts
 * the normal handler back.  inhibited_g_log_messages counts the messages
 * swallowed and is reset to zero by inhibit_g_log_messages(). */
extern int inhibited_g_log_messages;
void inhibit_g_log_messages (void);
void restore_g_log_messages (void);

/* Wrap @bytes in a memory input named @name (copied; the caller's buffer
 * is not referenced). */
GsfInput *regress_make_named_memory (char const *bytes, size_t len,
				     char const *name);

/* Build a two-member zip archive in memory ("aaa" = "hello",
 * "bbb" = "world") and return it as an input. */
GsfInput *regress_make_zip_input (void);

/* Build a two-member tar archive in memory (same members). */
GsfInput *regress_make_tar_input (void);

/* Append a ustar member named @name with @content to @b. */
void regress_tar_add (GByteArray *b, char const *name, char const *content);

/*
 * Append a GNU longname entry ('L') for @realname followed by the file
 * entry that carries @content.
 */
void regress_tar_add_longname (GByteArray *b, char const *realname,
			       char const *content);

/* Fill @buf with a deterministic, mildly varied pattern of @n bytes. */
void regress_fill_pattern (guint8 *buf, size_t n);

G_END_DECLS

#endif /* GSF_REGRESS_H */
