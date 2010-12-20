#include "s4_priv.h"
#include <stdlib.h>


struct s4_transaction_St {
	int flags;
	oplist_t *ops;
	void *locks;
	s4_transaction_t *waiting_for;
	int restartable, deadlocked;
};


static void _transaction_free (s4_transaction_t *trans)
{
	_entry_unlock_all (trans);
	_oplist_free (trans->ops);
	free (trans);
}

void _transaction_writing (s4_transaction_t *trans)
{
	_oplist_insert_writing (trans->ops);
}

s4_transaction_t *_transaction_get_waiting_for (s4_transaction_t *trans)
{
	return trans->waiting_for;
}

void _transaction_set_waiting_for (s4_transaction_t *trans, s4_transaction_t *waiting_for)
{
	trans->waiting_for = waiting_for;
}

void *_transaction_get_locks (s4_transaction_t *trans)
{
	return trans->locks;
}

void _transaction_set_locks (s4_transaction_t *trans, void *locks)
{
	trans->locks = locks;
}

void _transaction_set_deadlocked (s4_transaction_t *trans)
{
	trans->deadlocked = 1;
}

/* Tries to acquire all the needed locks.
 * Returns 0 on error (e.g. deadlock), non-zero on success.
 */
int _transaction_lock (s4_transaction_t *trans)
{
	int tries = 5, ret = 1;

	if (trans->deadlocked) {
		s4_set_errno (S4E_DEADLOCK);
		return 0;
	} else if (!trans->restartable) {
		tries = 1;
	}

	do {
		_oplist_reset (trans->ops);

		while (ret && _oplist_next (trans->ops)) {
			const char *key_a, *key_b, *src;
			const s4_val_t *val_a, *val_b;

			if (_oplist_get_add (trans->ops, &key_a, &val_a, &key_b, &val_b, &src)
					|| _oplist_get_del (trans->ops, &key_a, &val_a, &key_b, &val_b, &src)) {
				ret = _entry_lock (trans, key_a, val_a);
			}
		}
	} while (--tries && !ret);

	return ret;
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
	trans->flags = flags;
	trans->ops = _oplist_new (s4);
	trans->restartable = 1;

	_log_lock_file (s4);

	return trans;
}

/**
 * Commits a transaction. On success the operations in the transactions
 * will be applied in one atomic step, on error none of the operations
 * in the transaction will be applied.
 *
 * @param s4 The database the transaction belongs to.
 * @param trans The transaction to commit.
 * @return 0 on error (and sets s4_errno), non-zero on success.
 */
int s4_commit (s4_transaction_t *trans)
{
	int ret;

	if (!_transaction_lock (trans)) {
		ret = 0;
	} else {
		ret = _oplist_execute (trans->ops, 1);
	}

	if (ret != 0) {
		ret = _log_write (trans->ops);

		if (ret == 0) {
			_oplist_rollback (trans->ops);
			_start_sync (_transaction_get_db (trans));
		}
	}

	_log_unlock_file (_transaction_get_db (trans));
	_transaction_free (trans);

	return ret;
}

/**
 * Aborts a transaction.
 * The database will behave like the transaction never happened.
 *
 * @param s4 The database the transaction belongs to
 * @param trans The transaction
 * @return 0 on error, non-zero on success.
 */
int s4_abort (s4_transaction_t *trans)
{
	_transaction_free (trans);

	return 1;
}

s4_t *_transaction_get_db (s4_transaction_t *trans)
{
	return _oplist_get_db (trans->ops);
}


int s4_add (s4_t *s4, s4_transaction_t *trans,
		const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b,
		const char *src)
{
	s4_transaction_t *t = (trans == NULL) ? s4_begin (s4, 0) : trans;
	int ret = 1;

	_oplist_insert_add (t->ops, key_a, val_a, key_b, val_b, src);

	if (t != trans) {
		ret = s4_commit (t);
	}

	return ret;
}

int s4_del (s4_t *s4, s4_transaction_t *trans,
		const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b,
		const char *src)
{
	s4_transaction_t *t = (trans == NULL) ? s4_begin (s4, 0) : trans;
	int ret = 1;

	_oplist_insert_del (t->ops, key_a, val_a, key_b, val_b, src);

	if (t != trans) {
		ret = s4_commit (t);
	}

	return ret;
}

s4_resultset_t *s4_query (s4_t *s4, s4_transaction_t *trans,
		s4_fetchspec_t *spec, s4_condition_t *cond)
{
	s4_transaction_t *t = (trans == NULL) ? s4_begin (s4, 0) : trans;
	s4_resultset_t *ret;

	t->restartable = 0;
	ret = _s4_query (t, spec, cond);

	if (t != trans) {
		s4_commit (t);
	}

	return ret;
}
