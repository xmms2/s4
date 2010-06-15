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

#ifndef _MIDB_H
#define _MIDB_H

#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <s4_be.h>
#include "idt.h"

struct s4be_St {
	GHashTable *str_table;
	GStaticMutex str_table_lock;
	GHashTable *norm_str_table;
	GStaticMutex norm_str_table_lock;
	idt_t *id_str_table;
	GStaticMutex id_str_table_lock;

	GStaticRWLock rwlock;

	FILE *logfile;
	char *filename;
};

#define LOG_STRING_INSERT  1
#define LOG_PAIR_INSERT 2
#define LOG_PAIR_REMOVE 3

typedef struct midb_data_St {
	int32_t key_a, val_a;
	int32_t key_b, val_b;
	int32_t src;
} midb_data_t;

typedef struct log_entry_St {
	int32_t type;
	union {
		midb_data_t *pair;
		struct {
			const char *str;
			int32_t id;
		} str;
	} data;
} log_entry_t;

typedef struct str_St str_t;

void be_rlock (s4be_t *s4);
void be_runlock (s4be_t *s4);
void be_wlock (s4be_t *s4);
void be_wunlock (s4be_t *s4);

int s4be_st_ref_id (s4be_t *be, int32_t id);
str_t *s4be_st_insert (s4be_t *be, int32_t new_id, char *string);

int s4be_st_ref (s4be_t *be, const char *str);
int s4be_st_unref (s4be_t *be, const char *str);
int32_t s4be_st_lookup (s4be_t *be, const char *str);
int32_t *s4be_st_lookup_all (s4be_t *be, const char *str);
int s4be_st_get_refcount (s4be_t *be, int32_t node);
int s4be_st_set_refcount (s4be_t *be, int32_t node, int refcount);
int s4be_st_remove (s4be_t *be, const char* str);
char *s4be_st_reverse (s4be_t *be, int str_id);
char *s4be_st_reverse_normalized (s4be_t *be, int str_id);
char *s4be_st_normalize (const char *key);
void s4be_st_foreach (s4be_t *be,
		void (*func) (int32_t node, const char *str, void *userdata),
		void *userdata);
void midb_log (s4be_t *be, log_entry_t *entry);
void midb_log_string_insert (s4be_t *be, int32_t id, const char *string);
void midb_log_pair_insert (s4be_t *be, midb_data_t *rec);
void midb_log_pair_remove (s4be_t *be, midb_data_t *rec);

void sp_add (s4be_t *be, int32_t src);
GHashTable *sp_create (s4be_t *be, const char *srcprefs[]);
void sp_free (GHashTable *h);
int sp_get (s4be_t *be, int32_t src);

#endif
