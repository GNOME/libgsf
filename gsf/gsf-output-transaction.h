/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-output-transaction.h: 
 *
 * Copyright (C) 2004 Dom Lachowicz (cinamod@hotmail.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef GSF_OUTPUT_TRANSACTION_H
#define GSF_OUTPUT_TRANSACTION_H

G_BEGIN_DECLS

#define GSF_OUTPUT_TRANSACTION_TYPE        (gsf_output_transaction_get_type ())
#define GSF_OUTPUT_TRANSACTION(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_OUTPUT_TRANSACTION_TYPE, GsfOutputTransaction))
#define GSF_IS_OUTPUT_TRANSACTION(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_OUTPUT_TRANSACTION_TYPE))

typedef struct _GsfOutputTransaction GsfOutputTransaction;
typedef struct _GsfOutputTransactionClass GsfOutputTransactionClass;

GType      gsf_output_transaction_get_type (void);

GsfOutputTransaction *gsf_output_transaction_new       (GsfOutput *wrapped);
GsfOutputTransaction *gsf_output_transaction_new_named (GsfOutput *wrapped, const char * my_name);

gboolean   gsf_output_transaction_commit (GsfOutputTransaction * me);
void       gsf_output_transaction_abort  (GsfOutputTransaction * me);

G_END_DECLS

#endif /* GSF_OUTPUT_TRANSACTION_H */
