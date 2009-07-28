#include "s4_be.h"
#include "be.h"
#include "bpt.h"
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

	ret = bpt_insert (be, S4_INT_STORE, a);
	if (!ret)
		ret = bpt_insert (be, S4_REV_STORE, b);

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

	ret = bpt_remove (be, S4_INT_STORE, a);
	if (!ret)
		ret = bpt_remove (be, S4_REV_STORE, b);

	be_wunlock (be);

	return ret;
}


/**
 * Find all properties for this entry with key key.
 */
s4_set_t *s4be_ip_get (s4be_t *be, s4_entry_t *entry, int32_t key)
{
	s4_set_t *a, *b;
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
	a = bpt_find (be, S4_INT_STORE, start, stop);

	start.key_b = -start.key_b;
	stop.key_b = start.key_b + 1;

	b = bpt_find (be, S4_INT_STORE, start, stop);
	be_runlock (be);

	return s4_set_union (a, b);
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
	ret = bpt_find (be, S4_REV_STORE, start, stop);
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
	ret = bpt_find (be, S4_INT_STORE, start, stop);
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
	ret = bpt_find (be, S4_REV_STORE, start, stop);
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
	ret = bpt_find (be, S4_REV_STORE, start, stop);
	be_runlock (be);

	return ret;
}


static int _fix_key (s4be_t *old, s4be_t *new, int32_t bpt, bpt_record_t key)
{
	bpt_record_t nkey, rkey;
	if (key.key_a < 0) {
		nkey.key_a = -s4be_st_lookup (new,
				S4_PNT (old, pat_node_to_key (old, -key.key_a), char));
		nkey.val_a = key.val_a;
	} else {
		nkey.key_a = s4be_st_lookup (new,
				S4_PNT (old, pat_node_to_key (old, key.key_a), char));
		nkey.val_a = s4be_st_lookup (new,
				S4_PNT (old, pat_node_to_key (old, key.val_a), char));

		if (nkey.val_a == 0)
			return -1;
	}
	if (key.key_b < 0) {
		nkey.key_b = -s4be_st_lookup (new,
				S4_PNT (old, pat_node_to_key (old, -key.key_b), char));
		nkey.val_b = key.val_b;
	} else {
		nkey.key_b = s4be_st_lookup (new,
				S4_PNT (old, pat_node_to_key (old, key.key_b), char));
		nkey.val_b = s4be_st_lookup (new,
				S4_PNT (old, pat_node_to_key (old, key.val_b), char));

		if (nkey.val_b == 0)
			return -1;
	}

	nkey.src = s4be_st_lookup (new,
			S4_PNT (old, pat_node_to_key (old, key.src), char));


	if (nkey.key_a == 0 || nkey.key_b == 0 || nkey.src == 0)
		return -1;

	rkey.key_a = nkey.key_b;
	rkey.val_a = nkey.val_b;
	rkey.key_b = nkey.key_a;
	rkey.val_b = nkey.val_a;
	rkey.src = nkey.src;

	if (bpt == S4_INT_STORE) {
		bpt_insert (new, S4_INT_STORE, nkey);
		bpt_insert (new, S4_REV_STORE, rkey);
	} else {
		bpt_insert (new, S4_INT_STORE, rkey);
		bpt_insert (new, S4_REV_STORE, nkey);
	}

	return 0;
}

/* Try to recover the database */
int _ip_recover (s4be_t *old, s4be_t *rec)
{
	bpt_recover (old, rec, S4_INT_STORE, _fix_key);
	bpt_recover (old, rec, S4_REV_STORE, _fix_key);
	return 0;
}

int _ip_verify (s4be_t *be)
{
	return bpt_verify (be, S4_INT_STORE) || bpt_verify (be, S4_REV_STORE);
}

/**
 * @}
 */
