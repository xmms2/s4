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

/**
 * @defgroup Lock Lock
 * @ingroup S4
 * @brief Locks entries so only one transaction can use tham at a time.
 *
 * @{
 */

struct s4_lock_St {
	GMutex *lock;
	GCond *upgrade_signal, *signal;
	GHashTable *transactions;
	int writers_waiting;
	int readers;
	int exclusive;
	int upgrade, want_upgrade;
};


s4_lock_t *_lock_alloc ()
{
	s4_lock_t *lock = calloc (sizeof (s4_lock_t), 1);
	lock->lock = g_mutex_new ();
	lock->signal = g_cond_new ();
	lock->upgrade_signal = g_cond_new ();
	lock->transactions = g_hash_table_new (NULL, NULL);

	return lock;
}

void _lock_free (s4_lock_t *lock)
{
	g_mutex_free (lock->lock);
	g_cond_free (lock->signal);
	g_cond_free (lock->upgrade_signal);
	g_hash_table_destroy (lock->transactions);
	free (lock);
}

static int _lock_has_trans (s4_lock_t *lock, s4_transaction_t *trans)
{
	return g_hash_table_lookup (lock->transactions, trans) != NULL;
}

static void _lock_add_trans (s4_lock_t *lock, s4_transaction_t *trans)
{
	g_hash_table_insert (lock->transactions, trans, GINT_TO_POINTER (1));
}

static void _lock_del_trans (s4_lock_t *lock, s4_transaction_t *trans)
{
	g_hash_table_remove (lock->transactions, trans);
}

/* Checks if making trans wait for lock would deadlock. */
static int _lock_will_deadlock (s4_lock_t *lock, s4_transaction_t *trans, int first)
{
	GHashTableIter iter;
	GList *transactions = NULL;
	s4_transaction_t *t;
	int ret = 0;

	if (lock == NULL)
		return 0;

	g_mutex_lock (lock->lock);
	g_hash_table_iter_init (&iter, lock->transactions);

	while (g_hash_table_iter_next (&iter, (void**)&t, NULL)) {
		if (t != trans) {
			transactions = g_list_prepend (transactions, t);
		} else {
			ret = !first && t == trans;
		}
	}

	g_mutex_unlock (lock->lock);

	while (!ret && transactions != NULL) {
		t = transactions->data;
		ret = _lock_will_deadlock (_transaction_get_waiting_for (t), trans, 0);
		transactions = g_list_delete_link (transactions, transactions);
	}

	return ret;
}

int _lock_exclusive (s4_lock_t *lock, s4_transaction_t *trans)
{
	_transaction_set_waiting_for (trans, lock);

	if (_lock_will_deadlock (lock, trans, 1)) {
		_transaction_set_waiting_for (trans, NULL);
		s4_set_errno (S4E_DEADLOCK);
		return 0;
	}

	g_mutex_lock (lock->lock);

	if (_lock_has_trans (lock, trans)) {
		if (!lock->exclusive) {
			lock->want_upgrade = 1;
			lock->readers--;
			while (lock->readers) {
				g_cond_wait (lock->upgrade_signal, lock->lock);
			}
			lock->want_upgrade = 0;
		}
	} else {
		lock->writers_waiting++;
		while (lock->readers || lock->exclusive || lock->upgrade) {
			g_cond_wait (lock->signal, lock->lock);
		}
		lock->writers_waiting--;

		_lock_add_trans (lock, trans);
		_transaction_add_lock (trans, lock);
	}

	_transaction_set_waiting_for (trans, NULL);
	lock->exclusive = 1;

	g_mutex_unlock (lock->lock);
	return 1;
}

int _lock_shared (s4_lock_t *lock, s4_transaction_t *trans)
{
	int upgrade = !(_transaction_get_flags (trans) & S4_TRANS_READONLY);

	_transaction_set_waiting_for (trans, lock);

	if (_lock_will_deadlock (lock, trans, 1)) {
		_transaction_set_waiting_for (trans, NULL);
		s4_set_errno (S4E_DEADLOCK);
		return 0;
	}

	g_mutex_lock (lock->lock);

	if (!_lock_has_trans (lock, trans)) {
		while (lock->exclusive || lock->writers_waiting || (lock->upgrade && upgrade)) {
			g_cond_wait (lock->signal, lock->lock);
		}

		lock->readers++;
		if (upgrade) {
			lock->upgrade = 1;
		}
		_lock_add_trans (lock, trans);
		_transaction_add_lock (trans, lock);
	}

	_transaction_set_waiting_for (trans, NULL);
	g_mutex_unlock (lock->lock);
	return 1;
}

static void _lock_unlock (s4_lock_t *lock, s4_transaction_t *trans)
{
	int upgrade = 1;

	g_mutex_lock (lock->lock);
	if (lock->exclusive) {
		lock->exclusive = 0;
		g_cond_signal (lock->signal);
	} else if (lock->readers) {
		lock->readers--;

		if (lock->readers == 0) {
			if (lock->want_upgrade) {
				g_cond_signal (lock->upgrade_signal);
			} else {
				g_cond_signal (lock->signal);
			}
		}

	}

	_lock_del_trans (lock, trans);
	if (upgrade) {
		lock->upgrade = 0;
	}

	g_mutex_unlock (lock->lock);
}

void _lock_unlock_all (s4_transaction_t *trans)
{
	GList *locks = _transaction_get_locks (trans);

	for (; locks != NULL; locks = g_list_next (locks)) {
		s4_lock_t *lock = locks->data;
		_lock_unlock (lock, trans);
	}
}

/**
 * @}
 */
