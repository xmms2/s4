#ifndef _S4_PRIV_H
#define _S4_PRIV_H

#include <s4.h>
#include <glib.h>
#include <stdio.h>
#include "idt.h"

struct s4_St {
	GHashTable *index_table;
	GStaticMutex index_table_lock;

	GHashTable *rel_table;
	GStaticRWLock rel_lock;

	GStringChunk *strings;
	GStaticMutex strings_lock;

	FILE *logfile;
	char *filename;
};

typedef struct str_St str_t;

void s4_set_errno (int err);
int s4_sourcepref_get_priority (s4_sourcepref_t *sp, const char *src);

const char *_string_lookup (s4_t *s4, const char *str);

typedef struct {
	int32_t key_a, val_a;
	int32_t key_b, val_b;
	int32_t src;
} s4_intpair_t;

int _ip_add (s4_t *be, s4_intpair_t *pair);
int _ip_del (s4_t *be, s4_intpair_t *pair);
void _ip_foreach (s4_t *be, void (*func) (s4_intpair_t *pair, void *data), void *data);

typedef struct s4_index_St s4_index_t;
typedef int (*index_function_t)(s4_val_t *val, void *data);

s4_index_t *_index_get (s4_t *s4, const char *key);
s4_index_t *_index_create (void);
int _index_add (s4_t *s4, const char *key, s4_index_t *index);
int _index_insert (s4_index_t *index, s4_val_t *val, void *data);
int _index_delete (s4_index_t *index, const s4_val_t *val, void *data);
GList *_index_search (s4_index_t *index, index_function_t func, void *data);
void _index_free (s4_index_t *index);


#define LOG_STRING_INSERT  1
#define LOG_PAIR_INSERT 2
#define LOG_PAIR_REMOVE 3

typedef struct log_entry_St {
	int32_t type;
	union {
		s4_intpair_t *pair;
		struct {
			const char *str;
			int32_t id;
		} str;
	} data;
} log_entry_t;

void _log (s4_t *be, log_entry_t *entry);
void _log_pair_remove (s4_t *be, s4_intpair_t *rec);
void _log_pair_insert (s4_t *be, s4_intpair_t *rec);
void _log_string_insert (s4_t *be, int32_t id, const char *string);

int32_t s4_cond_get_ikey (s4_condition_t *cond);
void s4_cond_set_ikey (s4_condition_t *cond, int32_t ikey);

s4_resultset_t *s4_resultset_create (int col_count);
void s4_resultset_add_row (s4_resultset_t *set, s4_result_t **results);

s4_result_t *s4_result_create (s4_result_t *next, const char *key, s4_val_t *val, const char *src);
void s4_result_free (s4_result_t *res);

int s4_fetchspec_size (s4_fetchspec_t *spec);
const char *s4_fetchspec_get_key (s4_fetchspec_t *spec, int index);
s4_sourcepref_t *s4_fetchspec_get_sourcepref (s4_fetchspec_t *spec, int index);

void s4_free_relations (s4_t *s4);

#endif
