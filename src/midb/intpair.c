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

#include "s4_be.h"
#include "midb.h"
#include "bpt.h"
#include "log.h"
#include <stdlib.h>


/**
 *
 * @defgroup Intpair Intpair
 * @ingroup Homegrown
 *
 * @{
 */

/**
 * Add the binding entry->prop to the database
 *
 * @param be The database handle
 * @param entry The entry to get a new property
 * @param prop The property to be set
 * @return 0 on success, -1 otherwise
 */
int s4be_ip_add (s4be_t *be, s4_entry_t *entry, s4_entry_t *prop)
{
	int ret;
	bpt_record_t a, b;

	b.key_b = a.key_a = entry->key_i;
	b.val_b = a.val_a = entry->val_i;
	a.key_b = b.key_a = prop->key_i;
	a.val_b = b.val_a = prop->val_i;
	a.src = b.src = prop->src_i;

	be_wlock (be);

	ret = bpt_insert (be->int_store, &a);
	if (!ret) {
		ret = bpt_insert (be->rev_store, &b);
		midb_log_pair_insert (be, &a);
	}

	be_wunlock (be);

	return ret;
}


/**
 * Remove the binding entry->prop
 *
 * @param be The database handle
 * @param entry The entry to remove
 * @param prop The property to remove
 * @return 0 on success, -1 otherwise
 */
int s4be_ip_del (s4be_t *be, s4_entry_t *entry, s4_entry_t *prop)
{
	int ret;
	bpt_record_t a, b;

	b.key_b = a.key_a = entry->key_i;
	b.val_b = a.val_a = entry->val_i;
	a.key_b = b.key_a = prop->key_i;
	a.val_b = b.val_a = prop->val_i;
	a.src = b.src = prop->src_i;

	be_wlock (be);

	ret = bpt_remove (be->int_store, &a);
	if (!ret) {
		ret = bpt_remove (be->rev_store, &b);
		midb_log_pair_remove (be, &a);
	}

	be_wunlock (be);

	return ret;
}


/**
 * Find all properties for this entry with key key.
 */
s4_set_t *s4be_ip_get (s4be_t *be, s4_entry_t *entry, int32_t key)
{
	s4_set_t *a, *b, *ret;
	bpt_record_t start, stop;

	start.key_a = entry->key_i;
	start.val_a = entry->val_i;
	start.key_b = key;
	start.val_b = INT32_MIN;
	start.src = INT32_MIN;

	stop.key_a = entry->key_i;
	stop.val_a = entry->val_i;
	stop.key_b = start.key_b + 1;
	stop.val_b = INT32_MIN;
	stop.src = INT32_MIN;

	be_rlock (be);
	a = bpt_find (be->int_store, &start, &stop, 0);

	start.key_b = -start.key_b;
	stop.key_b = start.key_b + 1;

	b = bpt_find (be->int_store, &start, &stop, 0);
	be_runlock (be);

	ret = s4_set_union (a, b);
	s4_set_free (a);
	s4_set_free (b);

	return ret;
}


/**
 * Get all the entries that has the property entry
 *
 * @param be The database handle
 * @param entry The property
 * @return A set with all the entries
 */
s4_set_t *s4be_ip_has_this (s4be_t *be, s4_entry_t *entry)
{
	s4_set_t *ret;
	bpt_record_t start, stop;

	start.key_a = entry->key_i;
	start.val_a = entry->val_i;
	start.key_b = start.val_b = INT32_MIN;
	start.src = INT32_MIN;

	stop.key_a = entry->key_i;
	stop.val_a = entry->val_i + 1;
	stop.key_b = stop.val_b = INT32_MIN;
	stop.src = INT32_MIN;

	be_rlock (be);
	ret = bpt_find (be->rev_store, &start, &stop, 0);
	be_runlock (be);

	return ret;
}


/**
 * Get all the properties that this entry has
 *
 * @param be The database handle
 * @param entry The entry
 * @return A set with all the properties
 */
s4_set_t *s4be_ip_this_has (s4be_t *be, s4_entry_t *entry)
{
	s4_set_t *ret;
	bpt_record_t start, stop;

	start.key_a = entry->key_i;
	start.val_a = entry->val_i;
	start.key_b = start.val_b = INT32_MIN;
	start.src = INT32_MIN;

	stop.key_a = entry->key_i;
	stop.val_a = entry->val_i + 1;
	stop.key_b = stop.val_b = INT32_MIN;
	stop.src = INT32_MIN;

	be_rlock (be);
	ret = bpt_find (be->int_store, &start, &stop, 0);
	be_runlock (be);
	return ret;
}


s4_set_t *s4be_ip_smaller (s4be_t *be, s4_entry_t *entry, int key)
{
	bpt_record_t start, stop;
	s4_set_t *ret;

	start.key_a = entry->key_i;
	start.val_a = INT32_MIN;
	start.key_b = start.val_b = INT32_MIN;
	start.src = INT32_MIN;

	stop.key_a = entry->key_i;
	stop.val_a = entry->val_i;
	stop.key_b = stop.val_b = INT32_MIN;
	stop.src = INT32_MIN;

	be_rlock (be);
	ret = bpt_find (be->rev_store, &start, &stop, key);
	be_runlock (be);

	return ret;
}


s4_set_t *s4be_ip_greater (s4be_t *be, s4_entry_t *entry, int key)
{
	bpt_record_t start, stop;
	s4_set_t *ret;

	start.key_a = entry->key_i;
	start.val_a = entry->val_i + 1;
	start.key_b = start.val_b = INT32_MIN;
	start.src = INT32_MIN;

	stop.key_a = start.key_a + 1;
	stop.val_a = INT32_MIN;
	stop.key_b = stop.val_b = INT32_MIN;
	stop.src = INT32_MIN;

	be_rlock (be);
	ret = bpt_find (be->rev_store, &start, &stop, key);
	be_runlock (be);

	return ret;
}

struct foreach_info {
	void (*func) (s4_entry_t *e, s4_entry_t *p, void *userdata);
	void *userdata;
};

static void _foreach_helper (bpt_record_t rec, void *userdata)
{
	struct foreach_info *info = userdata;
	s4_entry_t e, p;

	e.key_i = rec.key_a;
	e.val_i = rec.val_a;
	e.src_i = rec.src;
	p.key_i = rec.key_b;
	p.val_i = rec.val_b;
	p.src_i = rec.src;

	e.type = (e.key_i < 0)?ENTRY_INT:ENTRY_STR;
	p.type = (p.key_i < 0)?ENTRY_INT:ENTRY_STR;

	e.key_s = e.src_s = e.val_s = p.key_s = p.src_s = p.val_s = NULL;

	info->func (&e, &p, info->userdata);
}

void s4be_ip_foreach (s4be_t *be,
		void (*func) (s4_entry_t *e, s4_entry_t *p, void *userdata),
		void *userdata)
{
	struct foreach_info info;
	info.func = func;
	info.userdata = userdata;

	bpt_foreach (be->int_store, _foreach_helper, &info);
}

/**
 * @}
 */
