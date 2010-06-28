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

struct s4_St {
	GHashTable *index_table;
	GStaticMutex index_table_lock;

	GHashTable *rel_table;
	GStaticMutex rel_lock;

	GStringChunk *strings;
	GStaticMutex strings_lock;

	GHashTable *norm_table;
	GStaticMutex norm_lock;

	FILE *logfile;
	GMutex *log_lock;
	GCond *sync_cond, *sync_finished_cond;
	log_number_t last_checkpoint;
	log_number_t last_synced;
	log_number_t last_logpoint;
	int sync_thread_run;
	GThread *sync_thread;

	char *filename;
};

typedef struct str_St str_t;

void s4_set_errno (s4_errno_t err);
void _start_sync (s4_t *s4);
void _sync (s4_t *s4);

s4_val_t *s4_val_new_internal_string (const char *str, const char *normalized_str);
char *s4_normalize_string (const char *key);

const char *_string_lookup (s4_t *s4, const char *str);
const char *_string_lookup_normalized (s4_t *s4, const char *str);

typedef struct {
	int32_t key_a, val_a;
	int32_t key_b, val_b;
	int32_t src;
} s4_intpair_t;

typedef struct s4_index_St s4_index_t;
typedef int (*index_function_t)(s4_val_t *val, void *data);

s4_index_t *_index_get (s4_t *s4, const char *key);
s4_index_t *_index_create (void);
int _index_add (s4_t *s4, const char *key, s4_index_t *index);
int _index_insert (s4_index_t *index, s4_val_t *val, void *data);
int _index_delete (s4_index_t *index, const s4_val_t *val, void *data);
GList *_index_search (s4_index_t *index, index_function_t func, void *data);
void _index_free (s4_index_t *index);

void _log_del (s4_t *s4, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src);
void _log_add (s4_t *s4, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src);
int _log_open (s4_t *s4);
int _log_close (s4_t *s4);

int32_t s4_cond_get_ikey (s4_condition_t *cond);
void s4_cond_set_ikey (s4_condition_t *cond, int32_t ikey);

s4_resultset_t *s4_resultset_create (int col_count);
void s4_resultset_add_row (s4_resultset_t *set, s4_result_t **results);

s4_result_t *s4_result_create (s4_result_t *next, const char *key, s4_val_t *val, const char *src);
void s4_result_free (s4_result_t *res);

void _free_relations (s4_t *s4);

#endif
