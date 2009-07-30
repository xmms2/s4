#ifndef _S4_BE_H
#define _S4_BE_H

#include <s4.h>
#include <glib.h>
#include <stdint.h>


s4be_t *s4be_open (const char *filename, int flags);
int s4be_close (s4be_t *be);
int s4be_verify (s4be_t *be, int thorough);
int s4be_recover (s4be_t *old, s4be_t *rec);
void s4be_sync (s4be_t *be);

int s4be_st_ref (s4be_t *be, const char *str);
int s4be_st_unref (s4be_t *be, const char *str);
int s4be_st_lookup (s4be_t *be, const char *str);
int s4be_st_refcount (s4be_t *be, int32_t node);
char *s4be_st_reverse (s4be_t *be, int str_id);
GList *s4be_st_regexp (s4be_t *be, const char *pat);
void s4be_st_foreach (s4be_t *be,
		void (*func) (int32_t node, void *userdata),
		void *userdata);

int s4be_ip_add (s4be_t *be, s4_entry_t *entry, s4_entry_t *prop);
int s4be_ip_del (s4be_t *be, s4_entry_t *entry, s4_entry_t *prop);
s4_set_t *s4be_ip_get (s4be_t *be, s4_entry_t *entry, int32_t key);
s4_set_t *s4be_ip_has_this (s4be_t *be, s4_entry_t *entry);
s4_set_t *s4be_ip_this_has (s4be_t *be, s4_entry_t *entry);
s4_set_t *s4be_ip_smaller (s4be_t *be, s4_entry_t *entry);
s4_set_t *s4be_ip_greater (s4be_t *be, s4_entry_t *entry);
void s4be_ip_foreach (s4be_t *be,
		void (*func) (s4_entry_t *e, s4_entry_t *p, void* userdata),
		void *userdata);

#endif /* _S4_BE_H */
