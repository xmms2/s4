/*  S4 - An XMMS2 medialib backend
 *  Copyright (C) 2009 Sivert Berg
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

#ifndef _S4_H
#define _S4_H

#include <glib.h>

#define ENTRY_INT 0
#define ENTRY_STR 1

/* Flags */
#define S4_RECOVER          1 << 0
#define S4_VERIFY           1 << 1
#define S4_VERIFY_THOROUGH  1 << 2
#define S4_VERIFY_REFCOUNT  1 << 3
#define S4_VERIFY_MASK      (S4_VERIFY_THOROUGH | S4_VERIFY_REFCOUNT)
#define S4_NEW              1 << 4
#define S4_EXISTS           1 << 5
#define S4_SYNC_THREAD      1 << 6

/* Error codes */
#define S4E_EXISTS 1
#define S4E_NOENT 2
#define S4E_OPEN  3
#define S4E_STHREAD 4
#define S4E_INCONS 5
#define S4E_MAGIC 6

typedef struct s4_entry_St {
	int type;
	char *key_s;
	int key_i;
	char *val_s;
	int val_i;
	char *src_s;
	int src_i;
} s4_entry_t;

struct s4_set_St;
typedef struct s4_set_St s4_set_t;

struct s4be_St;
typedef struct s4be_St s4be_t;

struct s4_St {
	s4be_t *be;
	GThread *s_thread;
	GCond *cond;
	GMutex *cond_mutex;
};
typedef struct s4_St s4_t;

/* s4.c */
s4_t *s4_open (const char *name, int flags);
int s4_close (s4_t *s4);
int s4_verify (s4_t *s4, int flags);
int s4_recover (s4_t *s4, const char *name);
void s4_sync (s4_t *s4);
int s4_start_sync_thread (s4_t *s4);
int s4_stop_sync_thread (s4_t *s4);
int s4_errno (void);

/* set.c */
s4_set_t *s4_set_new (int size);
void s4_set_free (s4_set_t *set);
int s4_set_size (s4_set_t *set);
s4_set_t *s4_set_intersection (s4_set_t *a, s4_set_t *b);
s4_set_t *s4_set_union (s4_set_t *a, s4_set_t *b);
s4_set_t *s4_set_complement (s4_set_t *a, s4_set_t *b);
s4_entry_t *s4_set_get (s4_set_t *set, int index);
s4_entry_t *s4_set_next (s4_set_t *set);
void s4_set_reset (s4_set_t *set);
int s4_set_insert (s4_set_t *set, s4_entry_t *entry);

/* entry.c */
s4_entry_t *s4_entry_get_s (s4_t *s4, const char *key, const char *val);
s4_entry_t *s4_entry_get_i (s4_t *s4, const char *key, int val);
s4_entry_t *s4_entry_copy (s4_entry_t *entry);
void s4_entry_free (s4_entry_t *entry);
void s4_entry_free_strings (s4_entry_t *entry);
s4_set_t *s4_entry_contains (s4_t *s4, s4_entry_t *entry);
s4_set_t *s4_entry_contained(s4_t *s4, s4_entry_t *entry);
int s4_entry_add (s4_t *s4, s4_entry_t *entry, s4_entry_t *prop, const char *src);
int s4_entry_del (s4_t *s4, s4_entry_t *entry, s4_entry_t *prop, const char *src);
void s4_entry_fillin (s4_t *s4, s4_entry_t *entry);
s4_set_t *s4_entry_smaller (s4_t *s4, s4_entry_t *entry, int key);
s4_set_t *s4_entry_greater (s4_t *s4, s4_entry_t *entry, int key);
s4_set_t *s4_entry_get_property (s4_t *s4, s4_entry_t *entry, const char *prop);
s4_set_t *s4_entry_match (s4_t *s4, s4_set_t *set, const char *pattern, int case_sens);
s4_set_t *s4_entry_get_entries (s4_t *s4, const char *key, const char *val);

/* query.c */
// s4_set_t *s4_query (s4_t *s4, xmms_coll_dag_t *dag, xmmsv_coll_t *coll);

#endif /* _S4_H */
