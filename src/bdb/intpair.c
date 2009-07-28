#include <stdlib.h>
#include <string.h>
#include "s4_be.h"
#include "be.h"


typedef struct intpair_St {
	int key;
	int val;
} intpair_t;


int intpair_compare (DB *db, const DBT *key1, const DBT *key2)
{
	intpair_t *a, *b;
	int ret;

	a = key1->data;
	b = key2->data;

	ret = a->key - b->key;
	if (!ret)
		ret = a->val - b->val;

	if (ret > 0)
		ret = 1;
	if (ret < 0)
		ret = -1;

	return ret;
}


static void _setup_dbts (DBT *key, DBT *data, intpair_t *pair_a, intpair_t *pair_b)
{
	memset (key, 0, sizeof (DBT));
	memset(data,0, sizeof (DBT));

	key->data = pair_a;
	key->size = sizeof (intpair_t);
	data->data = pair_b;
	data->ulen = sizeof (intpair_t);
	data->flags = DB_DBT_USERMEM;
}

static void _entry_to_pair (intpair_t *pair, s4_entry_t *entry)
{
	pair->key = entry->key_i;
	pair->val = entry->val_i;
}


int s4be_ip_add (s4be_t *s4,
		s4_entry_t *entry,
		s4_entry_t *prop)
{
	DBT key, data;
	DB_TXN *tid;
	int ret;
	intpair_t pair_a, pair_b;

	_entry_to_pair (&pair_a, entry);
	_entry_to_pair (&pair_b, prop);
	
	_setup_dbts (&key, &data, &pair_a, &pair_b);
	data.size = sizeof (intpair_t);

	if ((ret = s4->env->txn_begin (s4->env, NULL, &tid, DB_TXN_NOSYNC)) != 0) {
		tid->abort (tid);
		printf ("Error in intpair_add_property (txn_begin)\n");
		return -1;
	}

	if ((ret = s4->pair_db->put (s4->pair_db, tid, &key, &data, 0)) != 0 ||
	    (ret = s4->pair_rev_db->put (s4->pair_rev_db, tid, &data, &key, 0))) {
		tid->abort (tid);
		printf ("Error in intpair_add_property\n");
		return -1;
	}

	tid->commit (tid, DB_TXN_NOSYNC);


	return 0;
}


int s4be_ip_del (s4be_t *s4,
		s4_entry_t *entry,
		s4_entry_t *prop)
{
	DBT key, data;
	DBC *cursor;
	int ret;
	intpair_t pair_a, pair_b;

	_entry_to_pair (&pair_a, entry);
	_entry_to_pair (&pair_b, prop);

	_setup_dbts (&key, &data, &pair_a, &pair_b);
	data.size = sizeof (intpair_t);

	ret = s4->pair_db->cursor (s4->pair_db, NULL, &cursor, DB_WRITECURSOR);
	ret = cursor->get (cursor, &key, &data, DB_GET_BOTH);
	ret = cursor->del (cursor, 0);
	cursor->close (cursor);

	ret = s4->pair_rev_db->cursor (s4->pair_rev_db, NULL, &cursor, DB_WRITECURSOR);
	ret = cursor->get (cursor, &data, &key, DB_GET_BOTH);
	ret = cursor->del (cursor, 0);
	cursor->close (cursor);

	return 0;
}


static s4_set_t *_db_get_set (DB *db, s4_entry_t *entry)
{
	s4_set_t *root, *cur, *prev;
	DBT key, data;
	DBC *cursor;
	intpair_t pair_a, pair_b;
	int ret;

	_entry_to_pair (&pair_a, entry);
	_setup_dbts (&key, &data, &pair_a, &pair_b);

	ret = db->cursor (db, NULL, &cursor, 0);
	ret = cursor->get (cursor, &key, &data, DB_SET);

	root = prev = cur = NULL;
	while (!ret) {
		prev = cur;
		cur = malloc (sizeof (s4_set_t));
		cur->next = NULL;
		cur->entry.key_s = cur->entry.val_s = NULL;
		if (prev != NULL) {
			prev->next = cur;
		} else {
			root = cur;
		}

		cur->entry.key_i = pair_b.key;
		cur->entry.val_i = pair_b.val;

		_setup_dbts (&key, &data, &pair_a, &pair_b);
		ret = cursor->get (cursor, &key, &data, DB_NEXT_DUP);
	}

	cursor->close (cursor);

	return root;
}


s4_set_t *s4be_ip_has_this (s4be_t *s4, s4_entry_t *entry)
{
	return _db_get_set (s4->pair_rev_db, entry);
}


s4_set_t *s4be_ip_this_has (s4be_t *s4, s4_entry_t *entry)
{
	return _db_get_set (s4->pair_db, entry);
}
