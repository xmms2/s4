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
#include "be.h"
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

	ret = bpt_insert (be, S4_INT_STORE, &a);
	if (!ret)
		ret = bpt_insert (be, S4_REV_STORE, &b);

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

	ret = bpt_remove (be, S4_INT_STORE, &a);
	if (!ret)
		ret = bpt_remove (be, S4_REV_STORE, &b);

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
	a = bpt_find (be, S4_INT_STORE, &start, &stop);

	start.key_b = -start.key_b;
	stop.key_b = start.key_b + 1;

	b = bpt_find (be, S4_INT_STORE, &start, &stop);
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
	ret = bpt_find (be, S4_REV_STORE, &start, &stop);
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
	ret = bpt_find (be, S4_INT_STORE, &start, &stop);
	be_runlock (be);
	return ret;
}


s4_set_t *s4be_ip_smaller (s4be_t *be, s4_entry_t *entry)
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
	ret = bpt_find (be, S4_REV_STORE, &start, &stop);
	be_runlock (be);

	return ret;
}


s4_set_t *s4be_ip_greater (s4be_t *be, s4_entry_t *entry)
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
	ret = bpt_find (be, S4_REV_STORE, &start, &stop);
	be_runlock (be);

	return ret;
}

struct recovery_info {
	int32_t bpt;
	s4be_t *old;
	s4be_t *new;
};


static void _fix_key (bpt_record_t key, void *userdata)
{
	bpt_record_t nkey, rkey;
	struct recovery_info *info = userdata;

	if (key.key_a < 0) {
		nkey.key_a = -s4be_st_lookup_collated (info->new,
				S4_PNT (info->old, pat_node_to_key (info->old, -key.key_a), char));
		nkey.val_a = key.val_a;
	} else {
		nkey.key_a = s4be_st_lookup_collated (info->new,
				S4_PNT (info->old, pat_node_to_key (info->old, key.key_a), char));
		nkey.val_a = s4be_st_lookup_collated (info->new,
				S4_PNT (info->old, pat_node_to_key (info->old, key.val_a), char));

		if (nkey.val_a == 0)
			return;
	}
	if (key.key_b < 0) {
		nkey.key_b = -s4be_st_lookup_collated (info->new,
				S4_PNT (info->old, pat_node_to_key (info->old, -key.key_b), char));
		nkey.val_b = key.val_b;
	} else {
		nkey.key_b = s4be_st_lookup_collated (info->new,
				S4_PNT (info->old, pat_node_to_key (info->old, key.key_b), char));
		nkey.val_b = s4be_st_lookup_collated (info->new,
				S4_PNT (info->old, pat_node_to_key (info->old, key.val_b), char));

		if (nkey.val_b == 0)
			return;
	}

	nkey.src = s4be_st_lookup_collated (info->new,
			S4_PNT (info->old, pat_node_to_key (info->old, key.src), char));


	if (nkey.key_a == 0 || nkey.key_b == 0 || nkey.src == 0)
		return;

	rkey.key_a = nkey.key_b;
	rkey.val_a = nkey.val_b;
	rkey.key_b = nkey.key_a;
	rkey.val_b = nkey.val_a;
	rkey.src = nkey.src;

	if (info->bpt == S4_INT_STORE) {
		bpt_insert (info->new, S4_INT_STORE, &nkey);
		bpt_insert (info->new, S4_REV_STORE, &rkey);
	} else {
		bpt_insert (info->new, S4_INT_STORE, &rkey);
		bpt_insert (info->new, S4_REV_STORE, &nkey);
	}
}

/* Try to recover the database */
int _ip_recover (s4be_t *old, s4be_t *rec)
{
	struct recovery_info info;
	info.old = old;
	info.new = rec;

	info.bpt = S4_INT_STORE;
	bpt_foreach (old, S4_INT_STORE, _fix_key, &info);
	info.bpt = S4_REV_STORE;
	bpt_foreach (old, S4_REV_STORE, _fix_key, &info);
	return 0;
}

struct verification_info {
	s4be_t *be;
	int32_t bpt;
	int missing;
};

void _verification_helper (bpt_record_t rec, void *u)
{
	struct verification_info *info = u;
	bpt_record_t start, stop;
	s4_set_t *set;

	start.key_a = rec.key_b;
	start.val_a = rec.val_b;
	start.key_b = rec.key_a;
	start.val_b = rec.val_a;
	start.src = rec.src;

	stop = start;
	stop.src++;

	set = bpt_find (info->be, info->bpt, &start, &stop);

	if (set == NULL)
		info->missing++;
	else
		s4_set_free (set);
}

int _ip_verify (s4be_t *be)
{
	int ret;
	struct verification_info info;

	info.be = be;

	ret = bpt_verify (be, S4_INT_STORE) & bpt_verify (be, S4_REV_STORE);

	info.missing = 0;
	info.bpt = S4_REV_STORE;
	bpt_foreach (be, S4_INT_STORE, _verification_helper, &info);
	if (info.missing) {
		S4_ERROR ("Found %i keys in S4_INT_STORE not in S4_REV_STORE",
				info.missing);
		ret = 0;
	}

	info.missing = 0;
	info.bpt = S4_INT_STORE;
	bpt_foreach (be, S4_REV_STORE, _verification_helper, &info);
	if (info.missing) {
		S4_ERROR ("Found %i keys in S4_REV_STORE not in S4_INT_STORE",
				info.missing);
		ret = 0;
	}


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
	p.key_i = rec.key_b;
	p.val_i = rec.val_b;
	p.src_i = rec.src;

	e.type = (e.key_i < 0)?ENTRY_INT:ENTRY_STR;
	p.type = (p.key_i < 0)?ENTRY_INT:ENTRY_STR;

	info->func (&e, &p, info->userdata);
}

void s4be_ip_foreach (s4be_t *be,
		void (*func) (s4_entry_t *e, s4_entry_t *p, void *userdata),
		void *userdata)
{
	struct foreach_info info;
	info.func = func;
	info.userdata = userdata;

	bpt_foreach (be, S4_INT_STORE, _foreach_helper, &info);
}

/**
 * @}
 */
