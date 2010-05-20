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

#ifndef _S4_BE_H
#define _S4_BE_H

#include <s4.h>
#include <stdint.h>

void s4_set_errno (int err);

s4be_t *s4be_open (const char *filename, int flags);
int s4be_close (s4be_t *be);
int s4be_verify (s4be_t *be, int thorough);
int s4be_recover (s4be_t *old, s4be_t *rec);
void s4be_sync (s4be_t *be);

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

int s4be_ip_add (s4be_t *be, s4_entry_t *entry, s4_entry_t *prop);
int s4be_ip_del (s4be_t *be, s4_entry_t *entry, s4_entry_t *prop);
s4_set_t *s4be_ip_get (s4be_t *be, s4_entry_t *entry, int32_t key);
s4_set_t *s4be_ip_has_this (s4be_t *be, s4_entry_t *entry);
s4_set_t *s4be_ip_this_has (s4be_t *be, s4_entry_t *entry);
s4_set_t *s4be_ip_smaller (s4be_t *be, s4_entry_t *entry, int key);
s4_set_t *s4be_ip_greater (s4be_t *be, s4_entry_t *entry, int key);
void s4be_ip_foreach (s4be_t *be,
		void (*func) (s4_entry_t *e, s4_entry_t *p, void* userdata),
		void *userdata);

#endif /* _S4_BE_H */
