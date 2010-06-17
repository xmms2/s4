#ifndef _S4_PRIV_H
#define _S4_PRIV_H

#include <s4.h>
#include <glib.h>
#include <stdio.h>
#include "idt.h"

struct s4_St {
	GHashTable *str_table;
	GStaticMutex str_table_lock;
	GHashTable *norm_str_table;
	GStaticMutex norm_str_table_lock;
	idt_t *id_str_table;
	GStaticMutex id_str_table_lock;

	GHashTable *index_table;
	GStaticMutex index_table_lock;

	GHashTable *intpair_table;
	GStaticRWLock intpair_lock;

	FILE *logfile;
	char *filename;
};

typedef struct str_St str_t;

void s4_set_errno (int err);
int s4_sourcepref_get_priority (s4_sourcepref_t *sp, int32_t src);

str_t *_st_insert (s4_t *be, int32_t new_id, char *string);
int _st_ref (s4_t *be, const char *str);
int _st_ref_id (s4_t *be, int32_t id);
int _st_unref (s4_t *be, const char *str);
int32_t _st_lookup (s4_t *be, const char *str);
int32_t *_st_lookup_all (s4_t *be, const char *str);
char *_st_reverse (s4_t *be, int str_id);
char *_st_reverse_normalized (s4_t *be, int str_id);
char *_st_normalize (const char *key);
void _st_foreach (s4_t *be,
		void (*func) (int32_t node, const char *str, void *userdata),
		void *userdata);

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
int _index_delete (s4_index_t *index, s4_val_t *val, void *data);
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

#endif
