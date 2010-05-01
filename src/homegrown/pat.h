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

#ifndef _PAT_H
#define _PAT_H

#include "be.h"

/**
 * Structure used to lookup and insert things into the patricie trie
 */
typedef struct pat_key_St {
	void *common_key;
	int32_t common_keylen; /* length in bits! */

	void *unique_key;
	int32_t unique_keylen; /* length in bytes */
	int32_t unique_keyoff; /* the number of bytes to skip when comparing */
} pat_key_t;


typedef struct pat_trie_St {
	int32_t root;
	int32_t list_start;
} pat_trie_t;


int32_t pat_lookup (s4be_t *s4, int32_t trie, pat_key_t *key);
int32_t pat_lookup_parent (s4be_t *s4, int32_t trie, pat_key_t *key);
int32_t pat_insert (s4be_t *s4, int32_t trie, pat_key_t *key);
int     pat_remove (s4be_t *s4, int32_t trie, pat_key_t *key);
int32_t pat_node_to_key (s4be_t *s4, int32_t node);
int32_t pat_parent (s4be_t *s4, int32_t node);
int32_t pat_parent_key_count (s4be_t *s4, int32_t parent);
int32_t pat_parent_first_key (s4be_t *s4, int32_t parent);
char   *pat_node_to_str (s4be_t *s4, int32_t node);
int32_t pat_first (s4be_t *s4, int32_t trie);
int32_t pat_next (s4be_t *s4, int32_t trie, int32_t node);
int     pat_verify (s4be_t *be, int32_t trie);
void    pat_recover (s4be_t *be,
		void (*func) (int32_t node, void *userdata),
		void *userdata);

#endif /* _PAT_H */
