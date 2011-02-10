/*  S4 - An XMMS2 medialib backend
 *  Copyright (C) 2009, 2010 Sivert Berg
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#ifndef _S4_PRIV_H
#define _S4_PRIV_H

#include <s4.h>
#include <glib.h>
#include <stdio.h>

typedef uint32_t log_number_t;

typedef struct s4_index_data_St s4_index_data_t;
typedef struct s4_const_data_St s4_const_data_t;
typedef struct s4_entry_data_St s4_entry_data_t;

struct s4_St {
	int open_flags;

	s4_index_data_t *index_data;
	s4_const_data_t *const_data;
	s4_entry_data_t *entry_data;

	FILE *logfile;
	int log_users;
	GMutex *log_lock;
	GCond *sync_cond, *sync_finished_cond;
	log_number_t last_checkpoint;
	log_number_t last_synced;
	log_number_t last_logpoint;
	log_number_t next_logpoint;
	int sync_thread_run;
	GThread *sync_thread;
	GMutex *sync_lock;

	char *filename;
	char *tmp_filename;
	unsigned char uuid[16];
};

typedef struct str_St str_t;

void s4_set_errno (s4_errno_t err);
void _start_sync (s4_t *s4);
void _sync (s4_t *s4);
int _reread_file (s4_t *s4);

int _s4_add_internal (s4_t *s4, const char *key_a, const s4_val_t *value_a,
		const char *key_b, const s4_val_t *value_b, const char *src);
s4_entry_data_t *_entry_create_data ();
void _entry_free_data (s4_entry_data_t *data);

s4_val_t *s4_val_new_internal_string (const char *str, s4_t *s4);

const char *_string_lookup (s4_t *s4, const char *str);
const char *_string_lookup_casefolded (s4_t *s4, const char *str);
const char *_string_lookup_collated (s4_t *s4, const char *str);
const s4_val_t *_string_lookup_val (s4_t *s4, const char *str);
const s4_val_t *_int_lookup_val (s4_t *s4, int32_t i);
const s4_val_t *_const_lookup (s4_t *s4, const s4_val_t *val);
s4_const_data_t *_const_create_data ();
void _const_free_data (s4_const_data_t *data);

typedef struct {
	int32_t key_a, val_a;
	int32_t key_b, val_b;
	int32_t src;
} s4_intpair_t;

typedef struct s4_index_St s4_index_t;
typedef int (*index_function_t)(const s4_val_t *val, void *data);

s4_index_data_t *_index_create_data (void);
void _index_free_data (s4_index_data_t *data);
s4_index_t *_index_get_a (s4_t *s4, const char *key, int create);
s4_index_t *_index_get_b (s4_t *s4, const char *key);
GList *_index_get_all_a (s4_t *s4);
GList *_index_get_all_b (s4_t *s4);
s4_index_t *_index_create (void);
int _index_add (s4_t *s4, const char *key, s4_index_t *index);
int _index_insert (s4_index_t *index, const s4_val_t *val, void *data);
int _index_delete (s4_index_t *index, const s4_val_t *val, void *data);
GList *_index_search (s4_index_t *index, index_function_t func, void *data);
GList *_index_lsearch (s4_index_t *index, index_function_t func, void *data);
void _index_free (s4_index_t *index);
int _index_lock_shared (s4_index_t *index, s4_transaction_t *trans);
int _index_lock_exclusive (s4_index_t *index, s4_transaction_t *trans);


int32_t s4_cond_get_ikey (s4_condition_t *cond);
void s4_cond_set_ikey (s4_condition_t *cond, int32_t ikey);

s4_result_t *s4_result_create (s4_result_t *next, const char *key, const s4_val_t *val, const char *src);
void s4_result_free (s4_result_t *res);

s4_resultrow_t *s4_resultrow_create (int colcount);
s4_resultrow_t *s4_resultrow_ref (s4_resultrow_t *row);
void s4_resultrow_unref (s4_resultrow_t *row);

int _s4_add (s4_transaction_t *trans, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src);
int _s4_del (s4_transaction_t *trans, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src);
s4_resultset_t *_s4_query (s4_transaction_t *trans, s4_fetchspec_t *fs, s4_condition_t *cond);
void _free_relations (s4_t *s4);

typedef struct s4_lock_St s4_lock_t;
s4_lock_t *_lock_alloc ();
void _lock_free (s4_lock_t *lock);
int _lock_exclusive (s4_lock_t *lock, s4_transaction_t *trans);
int _lock_shared (s4_lock_t *lock, s4_transaction_t *trans);
void _lock_unlock_all (s4_transaction_t *trans);

s4_t *_transaction_get_db (s4_transaction_t *trans);
void  _transaction_writing (s4_transaction_t *trans);
s4_lock_t *_transaction_get_waiting_for (s4_transaction_t *trans);
void _transaction_set_waiting_for (s4_transaction_t *trans, s4_lock_t *waiting_for);
GList *_transaction_get_locks (s4_transaction_t *trans);
void  _transaction_add_lock (s4_transaction_t *trans, s4_lock_t *lock);
void _transaction_set_deadlocked (s4_transaction_t *trans);
s4_transaction_t *_transaction_dummy_alloc (s4_t *s4);
void _transaction_dummy_free (s4_transaction_t *trans);

typedef struct oplist_St oplist_t;
oplist_t *_oplist_new (s4_transaction_t *trans);
void _oplist_free (oplist_t *list);
s4_t *_oplist_get_db (oplist_t *list);
s4_transaction_t *_oplist_get_trans (oplist_t *list);
void _oplist_insert_add (oplist_t *list,
		const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b,
		const char *src);
void _oplist_insert_del (oplist_t *list,
		const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b,
		const char *src);
void _oplist_insert_writing (oplist_t *list);
int _oplist_get_add (oplist_t *list,
		const char **key_a, const s4_val_t **val_a,
		const char **key_b, const s4_val_t **val_b,
		const char **src);
int _oplist_get_del (oplist_t *list,
		const char **key_a, const s4_val_t **val_a,
		const char **key_b, const s4_val_t **val_b,
		const char **src);
int _oplist_get_writing (oplist_t *list);
int _oplist_next (oplist_t *list);
void _oplist_first (oplist_t *list);
void _oplist_last (oplist_t *list);
int _oplist_rollback (oplist_t *list);
int _oplist_execute (oplist_t *list, int rollback_on_failure);

void _log_lock_file (s4_t *s4);
void _log_unlock_file (s4_t *s4);
void _log_lock_db (s4_t *s4);
void _log_unlock_db (s4_t *s4);
int _log_write (oplist_t *list);
void _log_checkpoint (s4_t *s4);
int _log_open (s4_t *s4);
int _log_close (s4_t *s4);

#endif
