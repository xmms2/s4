#ifndef _PAT_H
#define _PAT_H

#include "be.h"

typedef struct pat_key_St {
	void *data;
	int32_t key_len;
	int32_t data_len;
} pat_key_t;


typedef struct pat_trie_St {
	int32_t root;
	int32_t list_start;
	int32_t list_end;
} pat_trie_t;


int32_t pat_lookup (s4be_t *s4, int32_t trie, pat_key_t *key);
int32_t pat_insert (s4be_t *s4, int32_t trie, pat_key_t *key);
int     pat_remove (s4be_t *s4, int32_t trie, pat_key_t *key);
int32_t pat_node_to_key (s4be_t *s4, int32_t node);
int32_t pat_first (s4be_t *s4, int32_t trie);
int32_t pat_next (s4be_t *s4, int32_t node);

#endif /* _PAT_H */
