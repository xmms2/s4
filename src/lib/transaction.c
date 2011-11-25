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
 *
 * @defgroup Transactions Transactions
 * @ingroup S4
 * @brief Functions dealing with transactions.
 *
 * @{
 */

struct s4_transaction_St {
	int flags;
	s4_t *s4;
	oplist_t *ops;
	GList *locks;
	s4_lock_t *waiting_for;
	int error_code;
	int restartable, failed;
};


static void _transaction_free (s4_transaction_t *trans)
{
	_lock_unlock_all (trans);
	g_list_free (trans->locks);
	_oplist_free (trans->ops);
	free (trans);
}

void _transaction_writing (s4_transaction_t *trans)
{
	_oplist_insert_writing (trans->ops);
}

s4_lock_t *_transaction_get_waiting_for (s4_transaction_t *trans)
{
	return g_atomic_pointer_get (&trans->waiting_for);
}

void _transaction_set_waiting_for (s4_transaction_t *trans, s4_lock_t *waiting_for)
{
	g_atomic_pointer_set (&trans->waiting_for, waiting_for);
}

GList *_transaction_get_locks (s4_transaction_t *trans)
{
	return trans->locks;
}

void _transaction_add_lock (s4_transaction_t *trans, s4_lock_t *lock)
{
	trans->locks = g_list_prepend (trans->locks, lock);
}

void _transaction_set_deadlocked (s4_transaction_t *trans)
{
	trans->failed = 1;
	trans->error_code = S4E_DEADLOCK;
}

s4_transaction_t *_transaction_dummy_alloc (s4_t *s4)
{
	s4_transaction_t *trans = calloc (sizeof (s4_transaction_t), 1);
	trans->s4 = s4;

	return trans;
}

void _transaction_dummy_free (s4_transaction_t *trans)
{
	_lock_unlock_all (trans);
	g_list_free (trans->locks);
	free (trans);
}

int _transaction_get_flags (s4_transaction_t *trans)
{
	return trans->flags;
}

/**
 * Starts a new transaction.
 *
 * @param s4 The database to run the transaction on.
 * @param flags Flags specifying what kind of transaction this should be.
 * @return A new transaction that can be used when calling s4_add, s4_del
 * and s4_query.
 */
s4_transaction_t *s4_begin (s4_t *s4, int flags)
{
	s4_transaction_t *trans = calloc (sizeof (s4_transaction_t), 1);
	trans->s4 = s4;
	trans->flags = flags;
	trans->ops = _oplist_new (trans);
	trans->restartable = 1;

	_log_lock_file (s4);

	return trans;
}

/**
 * Commits a transaction. On success the operations in the transactions
 * will be applied in one atomic step, on error none of the operations
 * in the transaction will be applied.
 *
 * @param trans The transaction to commit.
 * @return 0 on error (and sets s4_errno), non-zero on success.
 */
int s4_commit (s4_transaction_t *trans)
{
	int ret = 0;

	if (trans->failed) {
		s4_set_errno (trans->error_code);
	} else {
		ret = _log_write (trans->ops);

		if (ret == 0) {
			_start_sync (_transaction_get_db (trans));
			s4_set_errno (S4E_LOGFULL);
		}
	}

	if (ret == 0) {
		_oplist_last (trans->ops);
		_oplist_rollback (trans->ops);
	}

	_log_unlock_file (_transaction_get_db (trans));
	_transaction_free (trans);

	return ret;
}

/**
 * Aborts a transaction.
 * The database will behave like the transaction never happened.
 *
 * @param trans The transaction
 * @return 0 on error, non-zero on success.
 */
int s4_abort (s4_transaction_t *trans)
{
	_oplist_last (trans->ops);
	_oplist_rollback (trans->ops);
	_transaction_free (trans);

	return 1;
}

s4_t *_transaction_get_db (s4_transaction_t *trans)
{
	return trans->s4;
}

/**
 * Adds a relationship to the database.
 * It takes both a database handle and a transaction handle,
 * but only one if used. If the transaction handle is NULL it
 * will create a local transaction, otherwise it will use the
 * transaction passed, and thus s4 is not needed.
 *
 * @param s4 The database to add to.
 * @param trans The transaction to use.
 * @param key_a Key A.
 * @param val_a Value A.
 * @param key_b Key B.
 * @param val_b Value B.
 * @param src Source.
 * @return 0 on error, non-zero on success.
 */
int s4_add (s4_transaction_t *trans,
		const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b,
		const char *src)
{
	int ret;
	s4_t *db = _transaction_get_db (trans);

	if (trans->flags & S4_TRANS_READONLY) {
		trans->failed = 1;
		trans->error_code = S4E_READONLY;
		return 0;
	}

	key_a = _string_lookup (db, key_a);
	key_b = _string_lookup (db, key_b);
	src = _string_lookup (db, src);
	val_a = _const_lookup (db, val_a);
	val_b = _const_lookup (db, val_b);

	if (trans->failed) {
		ret = 0;
	} else {
		_oplist_insert_add (trans->ops, key_a, val_a, key_b, val_b, src);
		ret = _s4_add (trans, key_a, val_a, key_b, val_b, src);

		if (!ret) {
			trans->failed = 1;
			trans->error_code = S4E_EXECUTE;
		}
	}

	return ret;
}

/**
 * Deletes a relationship from the database.
 * It takes both a database handle and a transaction handle,
 * but only one if used. If the transaction handle is NULL it
 * will create a local transaction, otherwise it will use the
 * transaction passed, and thus s4 is not needed.
 *
 * @param s4 The database to delete from.
 * @param trans The transaction to use.
 * @param key_a Key A.
 * @param val_a Value A.
 * @param key_b Key B.
 * @param val_b Value B.
 * @param src Source.
 * @return 0 on error, non-zero on success.
 */
int s4_del (s4_transaction_t *trans,
		const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b,
		const char *src)
{
	int ret;
	s4_t *db = _transaction_get_db (trans);

	if (trans->flags & S4_TRANS_READONLY) {
		trans->failed = 1;
		trans->error_code = S4E_READONLY;
		return 0;
	}

	key_a = _string_lookup (db, key_a);
	key_b = _string_lookup (db, key_b);
	src = _string_lookup (db, src);
	val_a = _const_lookup (db, val_a);
	val_b = _const_lookup (db, val_b);

	if (trans->failed) {
		ret = 0;
	} else {
		_oplist_insert_del (trans->ops, key_a, val_a, key_b, val_b, src);
		ret = _s4_del (trans, key_a, val_a, key_b, val_b, src);

		if (!ret) {
			trans->failed = 1;
			trans->error_code = S4E_EXECUTE;
		}
	}

	return ret;
}

/**
 * Queries an S4 database.
 *
 * @param s4 The database to query. If s4 is NULL trans must be non-null.
 * @param trans The transaction to use. If trans is NULL s4 must be non-null.
 * @param spec The fetchspecification to use when querying.
 * @param cond The condition to use when querying.
 * @return A resultset containing the fetched data.
 */
s4_resultset_t *s4_query (s4_transaction_t *trans,
		s4_fetchspec_t *spec, s4_condition_t *cond)
{
	s4_resultset_t *ret;

	trans->restartable = 0;

	if (trans->failed) {
		ret = s4_resultset_create (0);
	} else {
		ret = _s4_query (trans, spec, cond);
	}

	return ret;
}

/**
 * @}
 */
