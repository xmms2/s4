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

#ifndef _BPT_H
#define _BPT_H

#include <s4.h>

typedef struct bpt_record_St {
	int32_t key_a, val_a;
	int32_t key_b, val_b;
	int32_t src;
} bpt_record_t;

typedef struct bpt_node_St bpt_node_t;

typedef struct bpt_St {
	bpt_node_t *root;
	bpt_node_t *leaves;
} bpt_t;

bpt_t *bpt_create (void);
void bpt_destroy (bpt_t *bpt);
int bpt_insert (bpt_t *bpt, bpt_record_t *record);
int bpt_remove (bpt_t *bpt, bpt_record_t *record);
void bpt_foreach (bpt_t *bpt,
		void (*func)(bpt_record_t, void *userdata),
		void *userdata);
s4_set_t *bpt_find (bpt_t *bpt, bpt_record_t *start, bpt_record_t *stop, int key);
int bpt_verify (bpt_t *bpt);

#endif /* _BPT_H */
