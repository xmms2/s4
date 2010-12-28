/*  S4 - An XMMS2 medialib backend
 *  Copyright (C) 2009, 2010 Sivert Berg
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

#include "s4_priv.h"
#include <stdlib.h>

typedef struct lock_St {
	const char *key;
	const s4_val_t *val;
	s4_transaction_t *trans;
	GCond *cond;
	struct lock_St *next;
} lock_t;

static void _free_lock (lock_t *lock)
{
	g_cond_free (lock->cond);
	free (lock);
}

/* Create a new lock object */
static lock_t *_create_lock (const char *key, const s4_val_t *val)
{
	lock_t *lock = malloc (sizeof (lock_t));

	lock->key = key;
	lock->val = val;
	lock->cond = g_cond_new ();
	lock->trans = NULL;

	return lock;
}

/* Checks if making trans wait for lock would deadlock. */
static int _is_deadlocked (lock_t *lock, s4_transaction_t *trans)
{
	s4_transaction_t *t;
	for (t = lock->trans; t != trans && t != NULL; t = _transaction_get_waiting_for (t));
	return t == trans;
}

static guint _lock_hash (lock_t *lock)
{
	return GPOINTER_TO_INT (lock->key) ^ GPOINTER_TO_INT (lock->val);
}

static gboolean _lock_equal (lock_t *a, lock_t *b)
{
	return a->key == b->key && a->val == b->val;
}

GHashTable *_create_lock_table (void)
{
	return g_hash_table_new_full ((GHashFunc)_lock_hash,
			(GEqualFunc)_lock_equal, (GDestroyNotify)_free_lock, NULL);
}

/**
 * Locks an entry.
 *
 * @param trans The transaction doing the locking
 * @param key
 * @param val
 * @return 0 on error, non-zero otherwise.
 */
int _entry_lock (s4_transaction_t *trans, const char *key, const s4_val_t *val)
{
	int ret = 1;
	lock_t lookup_lock = {.key = key, .val = val};
	lock_t *lock;
	s4_t *s4 = _transaction_get_db (trans);

	g_mutex_lock (s4->lock_lock);
	lock = g_hash_table_lookup (s4->lock_table, &lookup_lock);

	if (lock == NULL) {
		lock = _create_lock (key, val);
		g_hash_table_insert (s4->lock_table, lock, lock);
	}

	if (lock->trans == trans) {
		goto finished;
	}

	while (ret && lock->trans != NULL) {
		if (_is_deadlocked (lock, trans)) {
			s4_set_errno (S4E_DEADLOCK);
			ret = 0;
		} else {
			_transaction_set_waiting_for (trans, lock->trans);
			g_cond_wait (lock->cond, s4->lock_lock);
			_transaction_set_waiting_for (trans, NULL);
		}
	}

	if (ret) {
		lock->trans = trans;
		lock->next = _transaction_get_locks (trans);
		_transaction_set_locks (trans, lock);
	}

finished:
	g_mutex_unlock (s4->lock_lock);
	return ret;
}

/**
 * Unlocks all locks held by a transaction.
 *
 * @param trans The transaction to release the locks of
 * @return 0 on error, non-zero otherwise.
 */
int _entry_unlock_all (s4_transaction_t *trans)
{
	int ret = 1;
	lock_t *lock;
	s4_t *s4 = _transaction_get_db (trans);

	g_mutex_lock (s4->lock_lock);

	for (lock = _transaction_get_locks (trans); lock != NULL; lock = lock->next) {
		lock->trans = NULL;
		g_cond_signal (lock->cond);
	}

	g_mutex_unlock (s4->lock_lock);
	return ret;
}
