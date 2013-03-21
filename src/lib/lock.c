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
 * Locks can be locked in three ways, read-only, upgradable and exclusive.
 * - Read-only is for read-only transactions that know they will not want
 *   an exclusive version of the lock later one
 * - Upgradable is for transactions that might want to upgrade it to an
 *   exclusive lock later on.
 * - Exclusive locks can only be held by one transaction.
 *
 * @{
 */

struct s4_lock_St {
	GMutex lock;
	GCond upgrade_signal, signal;
	GHashTable *transactions;
	int writers_waiting;
	int readers;
	int exclusive;
	int upgrade, want_upgrade;
};

/* Creates a new lock structure */
s4_lock_t *_lock_alloc ()
{
	s4_lock_t *lock = calloc (sizeof (s4_lock_t), 1);
	g_mutex_init (&lock->lock);
	g_cond_init (&lock->signal);
	g_cond_init (&lock->upgrade_signal);
	lock->transactions = g_hash_table_new (NULL, NULL);

	return lock;
}

/* Frees a lock structure */
void _lock_free (s4_lock_t *lock)
{
	g_mutex_clear (&lock->lock);
	g_cond_clear (&lock->signal);
	g_cond_clear (&lock->upgrade_signal);
	g_hash_table_destroy (lock->transactions);
	free (lock);
}

/* Checks if transactions holds this lock */
static int _lock_has_trans (s4_lock_t *lock, s4_transaction_t *trans)
{
	return g_hash_table_lookup (lock->transactions, trans) != NULL;
}

/* Adds a transactions to the table of transactions holding this lock */
static void _lock_add_trans (s4_lock_t *lock, s4_transaction_t *trans)
{
	g_hash_table_insert (lock->transactions, trans, GINT_TO_POINTER (1));
}

/* Removes a transactions from the table of transactions holding this lock */
static void _lock_del_trans (s4_lock_t *lock, s4_transaction_t *trans)
{
	g_hash_table_remove (lock->transactions, trans);
}

/* Checks if making trans wait for lock would deadlock.
 * Deadlock happens when we have to wait on a lock held by
 * a transaction waiting for a lock this transaction holds.
 * It can be thought of as searching for a cycle in a graph, where vertices
 * are locks and edges go from the lock to the locks the transactions holding
 * this lock are waiting for. If there is a cycle we have a deadlock.
 *
 * Returns 1 if it will deadlock, 0 otherwise
 */
static int _lock_will_deadlock_helper (s4_lock_t *lock, s4_transaction_t *trans, GHashTable *visited, int first)
{
	GHashTableIter iter;
	GList *waiting_for = NULL;
	s4_transaction_t *t;
	int ret = 0;

	/* Bail if the lock is NULL or has already been visited */
	if (lock == NULL || g_hash_table_lookup (visited, lock) != NULL)
		return 0;

	/* Add this lock to the table of visited locks */
	g_hash_table_insert (visited, lock, GINT_TO_POINTER (1));

	g_mutex_lock (&lock->lock);
	g_hash_table_iter_init (&iter, lock->transactions);

	/* Get a list of all transactions holding this lock */
	while (g_hash_table_iter_next (&iter, (void**)&t, NULL)) {
		if (t != trans) {
			waiting_for = g_list_prepend (waiting_for, _transaction_get_waiting_for (t));
		} else {
			/* If we found a lock that this transaction holds
			 * and it's not the first lock, we have a deadlock.
			 */
			ret = !first;
		}
	}

	g_mutex_unlock (&lock->lock);

	/* Check all the locks the transactions in the list are waiting for */
	while (!ret && waiting_for != NULL) {
		ret = _lock_will_deadlock_helper (waiting_for->data, trans, visited, 0);
		waiting_for = g_list_delete_link (waiting_for, waiting_for);
	}

	g_list_free (waiting_for);

	return ret;
}

static int _lock_will_deadlock (s4_lock_t *lock, s4_transaction_t *trans)
{
	GHashTable *visited;
	int ret;

	visited = g_hash_table_new (NULL, NULL);
	ret = _lock_will_deadlock_helper (lock, trans, visited, 1);

	g_hash_table_destroy (visited);
	return ret;
}

/* Aquires an exclusive lock. */
int _lock_exclusive (s4_lock_t *lock, s4_transaction_t *trans)
{
	_transaction_set_waiting_for (trans, lock);

	if (_lock_will_deadlock (lock, trans)) {
		_transaction_set_waiting_for (trans, NULL);
		s4_set_errno (S4E_DEADLOCK);
		return 0;
	}

	g_mutex_lock (&lock->lock);

	if (_lock_has_trans (lock, trans)) {
		/* If we already hold this lock, but not exclusively,
		 * we have to upgrade it
		 */
		if (!lock->exclusive) {
			lock->want_upgrade = 1;
			lock->readers--;
			while (lock->readers) {
				g_cond_wait (&lock->upgrade_signal, &lock->lock);
			}
			lock->want_upgrade = 0;
		}
	} else {
		lock->writers_waiting++;
		while (lock->readers || lock->exclusive || lock->upgrade) {
			g_cond_wait (&lock->signal, &lock->lock);
		}
		lock->writers_waiting--;

		_lock_add_trans (lock, trans);
		_transaction_add_lock (trans, lock);
	}

	_transaction_set_waiting_for (trans, NULL);
	lock->exclusive = 1;

	g_mutex_unlock (&lock->lock);
	return 1;
}

/* Aquires a shared (upgradable if this is not a read-only transcation) lock */
int _lock_shared (s4_lock_t *lock, s4_transaction_t *trans)
{
	/* If this is not a read-only transaction, we might want to
	 * aquire this lock exclusively later on, therefore it must be
	 * upgradable
	 */
	int upgrade = !(_transaction_get_flags (trans) & S4_TRANS_READONLY);

	_transaction_set_waiting_for (trans, lock);

	if (_lock_will_deadlock (lock, trans)) {
		_transaction_set_waiting_for (trans, NULL);
		s4_set_errno (S4E_DEADLOCK);
		return 0;
	}

	g_mutex_lock (&lock->lock);

	/* If we do not already hold this lock we have to aquire it */
	if (!_lock_has_trans (lock, trans)) {
		while (lock->exclusive || lock->writers_waiting || (lock->upgrade && upgrade)) {
			g_cond_wait (&lock->signal, &lock->lock);
		}

		lock->readers++;
		if (upgrade) {
			lock->upgrade = 1;
		}
		_lock_add_trans (lock, trans);
		_transaction_add_lock (trans, lock);
	}

	_transaction_set_waiting_for (trans, NULL);
	g_mutex_unlock (&lock->lock);
	return 1;
}

/* Unlocks a single lock held by trans */
static void _lock_unlock (s4_lock_t *lock, s4_transaction_t *trans)
{
	int upgrade = !(_transaction_get_flags (trans) & S4_TRANS_READONLY);

	g_mutex_lock (&lock->lock);
	if (lock->exclusive) {
		lock->exclusive = 0;
		g_cond_signal (&lock->signal);
	} else if (lock->readers) {
		lock->readers--;

		if (lock->readers == 0) {
			if (lock->want_upgrade) {
				g_cond_signal (&lock->upgrade_signal);
			} else {
				g_cond_signal (&lock->signal);
			}
		}

	}

	_lock_del_trans (lock, trans);
	if (upgrade) {
		lock->upgrade = 0;
	}

	g_mutex_unlock (&lock->lock);
}

/* Unlocks all locks held by trans */
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
