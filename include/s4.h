#ifndef _S4_H
#define _S4_H

#include <glib.h>

#define ENTRY_INT 0
#define ENTRY_STR 1

typedef struct s4_entry_St {
	int type;
	char *key_s;
	int key_i;
	char *val_s;
	int val_i;
	char *src_s;
	int src_i;
} s4_entry_t;

typedef struct s4_set_St {
	struct s4_set_St *next;
	s4_entry_t entry;
} s4_set_t;

struct s4be_St;
typedef struct s4be_St s4be_t;

struct s4_St {
	s4be_t *be;
	GStaticRWLock rwlock;
	GThread *s_thread;
	GCond *cond;
	GMutex *cond_mutex;
};
typedef struct s4_St s4_t;

/* s4.c */
s4_t *s4_open (const char *name);
int s4_close (s4_t *s4);

/* set.c */
s4_set_t *s4_set_intersection (s4_set_t *a, s4_set_t *b);
s4_set_t *s4_set_union (s4_set_t *a, s4_set_t *b);
s4_set_t *s4_set_next (s4_set_t *set);
void s4_set_free (s4_set_t *set);

/* entry.c */
s4_entry_t *s4_entry_get_s (s4_t *s4, const char *key, const char *val);
s4_entry_t *s4_entry_get_i (s4_t *s4, const char *key, int val);
void s4_entry_free (s4_entry_t *entry);
void s4_entry_free_strings (s4_entry_t *entry);
s4_set_t *s4_entry_contains (s4_t *s4, s4_entry_t *entry);
s4_set_t *s4_entry_contained(s4_t *s4, s4_entry_t *entry);
int s4_entry_add (s4_t *s4, s4_entry_t *entry, s4_entry_t *prop, const char *src);
int s4_entry_del (s4_t *s4, s4_entry_t *entry, s4_entry_t *prop, const char *src);
void s4_entry_fillin (s4_t *s4, s4_entry_t *entry);
s4_set_t *s4_entry_smaller (s4_t *s4, s4_entry_t *entry);
s4_set_t *s4_entry_greater (s4_t *s4, s4_entry_t *entry);
s4_set_t *s4_entry_get_property (s4_t *s4, s4_entry_t *entry, const char *prop);

/* query.c */
// s4_set_t *s4_query (s4_t *s4, xmms_coll_dag_t *dag, xmmsv_coll_t *coll);

#endif /* _S4_H */
