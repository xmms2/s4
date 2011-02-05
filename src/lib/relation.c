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
#include <string.h>

typedef struct {
	const char *key;
	const s4_val_t *val;
	const char *src;
} entry_data_t;

typedef struct {
	s4_lock_t *lock;
	const char *key;
	const s4_val_t *val;
	int size, alloc;

	entry_data_t *data;
} entry_t;

#define LINEAR_SEARCH_SIZE 0

/**
 * @defgroup Entry Entry
 * @ingroup S4
 * @brief The in-memory database.
 *
 * @{
 */

/**
 * @{
 * @internal
 */

/**
 * Searches an entry for key
 *
 * @param entry The entry to search
 * @param key The key to search for
 * @return The index of the item before the first item with key=key,
 * or the index of the first item with key=key.
 */
static int _entry_search (entry_t *entry, const char *key)
{
	int lo = 0;
	int hi = entry->size;

	while ((hi - lo) > LINEAR_SEARCH_SIZE) {
		int m = (hi + lo) / 2;

		if (entry->data[m].key < key)
			lo = m + 1;
		else
			hi = m;
	}

	for (; lo < hi && entry->data[lo].key < key; lo++);

	return lo;
}

/**
 * Inserts a key,value,source tuple into an entry
 *
 * @param entry The entry to insert into
 * @param key The key to insert
 * @param val The value to insert
 * @param src The source to insert
 * @return 0 if the tuple already exists, non-zero otherwise
 */
static int _entry_insert (entry_t *entry, const char *key, const s4_val_t *val, const char *src)
{
	int i = _entry_search (entry, key);

	for (; i < entry->size && entry->data[i].key == key; i++) {
		if (entry->data[i].src == src && !s4_val_cmp (entry->data[i].val, val, S4_CMP_BINARY))
			return 0;
	}

	if (entry->size >= entry->alloc) {
		entry->alloc *= 2;
		entry->data = realloc (entry->data, sizeof (entry_data_t) * entry->alloc);
	}

	memmove (entry->data + i + 1, entry->data + i, (entry->size - i) * sizeof (entry_data_t));
	entry->size++;

	entry->data[i].key = key;
	entry->data[i].val = val;
	entry->data[i].src = src;

	return 1;
}

/**
 * Deletes a key,value,source tuple from an entry
 *
 * @param entry The entry to delete from
 * @param key The key to delete
 * @param val The value to delete
 * @param src The source to delete
 * @return 0 if the tuple was not found, 1 otherwise
 */
static int _entry_delete (entry_t *entry, const char *key, const s4_val_t *val, const char *src)
{
	int i = _entry_search (entry, key);
	int found = 0;

	for (; i < entry->size && entry->data[i].key == key; i++) {
		if (entry->data[i].src == src && !s4_val_cmp (entry->data[i].val, val, S4_CMP_BINARY)) {
			found = 1;
			break;
		}
	}

	if (!found)
		return 0;

	memmove (entry->data + i, entry->data + i + 1, (entry->size - i - 1) * sizeof (entry_data_t));
	entry->size--;

	return 1;
}

/**
 * Creates a new entry
 *
 * @param key The key of the entry
 * @param val The value of the entry
 * @return A new empty entry
 */
static entry_t *_entry_create (const char *key, const s4_val_t *val)
{
	entry_t *entry = malloc (sizeof (entry_t));

	entry->lock = _lock_alloc ();
	entry->key = key;
	entry->val = val;
	entry->size = 0;
	entry->alloc = 1;

	entry->data = malloc (sizeof (entry_data_t) * entry->alloc);

	return entry;
}

static int _entry_lock_shared (entry_t *entry, s4_transaction_t *trans)
{
	return _lock_shared (entry->lock, trans);
}

static int _entry_lock_exclusive (entry_t *entry, s4_transaction_t *trans)
{
	return _lock_exclusive (entry->lock, trans);
}

/**
 * @}
 */

/**
 * Adds a new relation to a database
 *
 * @param s4 The database to add to
 * @param key_a The key of the first entry
 * @param val_a The value of the first entry
 * @param key_b The key of the second entry
 * @param val_b The value of the second entry
 * @param src The source that made the relation
 * @return non-zero if everything went alrite, 0 otherwise
 */
int _s4_add (s4_transaction_t *trans, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src)
{
	s4_index_t *index;
	entry_t *entry;
	GList *entries;
	int ret;
	s4_t *s4 = _transaction_get_db (trans);

	index = _index_get_a (s4, key_a, 1);
	if (!_index_lock_shared (index, trans)) goto deadlocked;
	entries = _index_search (index, NULL, (void*)val_a);

	if (entries == NULL) {
		entry = _entry_create (key_a, val_a);
		if (!_index_lock_exclusive (index, trans)) goto deadlocked;
		_index_insert (index, val_a, entry);
	} else {
		entry = entries->data;
		g_list_free (entries);
	}

	if (!_entry_lock_exclusive (entry, trans)) goto deadlocked;
	ret = _entry_insert (entry, key_b, val_b, src);

	if (ret) {
		index = _index_get_b (s4, key_b);

		if (index != NULL) {
			if (!_index_lock_exclusive (index, trans)) goto deadlocked;
			_index_insert (index, val_b, entry);
		}
	}

	return ret;

deadlocked:
	_transaction_set_deadlocked (trans);
	return 0;
}

/* An internal function of the above functions. It expects all keys
 * and values passed to be internal, so no copying have to be done.
 * It also exploits that pairs with the same key_a and value_a are
 * written next to each other on file, and it can therefore save
 * many index searches.
 */
int _s4_add_internal (s4_t *s4, const char *key_a, const s4_val_t *value_a,
		const char *key_b, const s4_val_t *value_b, const char *src)
{
	int ret;
	s4_index_t *index;
	static entry_t *entry;
	static const char *prev_key = NULL;
	static const s4_val_t *prev_val = NULL;

	/* If key_a and value_a are equal to the key and value of entry
	 * it don't have to search the index to find entry
	 */
	if (prev_key != key_a || prev_val != value_a) {
		GList *entries;

		index = _index_get_a (s4, key_a, 1);
		entries = _index_search (index, NULL, (void*)value_a);

		if (entries == NULL) {
			entry = _entry_create (key_a, value_a);
			_index_insert (index, value_a, entry);
		} else {
			entry = entries->data;
			g_list_free (entries);
		}

		prev_key = key_a;
		prev_val = value_a;
	}

	ret = _entry_insert (entry, key_b, value_b, src);

	if (ret) {
		index = _index_get_b (s4, key_b);

		if (index != NULL) {
			_index_insert (index, value_b, entry);
		}
	}

	return ret;
}

/**
 * Deletes a relation from a database
 *
 * @param s4 The database to delete from
 * @param key_a The key of the first entry
 * @param val_a The value of the first entry
 * @param key_b The key of the second entry
 * @param val_b The value of the second entry
 * @param src The source that made the relation
 * @return non-zero if everything went alrite, 0 otherwise
 */
int _s4_del (s4_transaction_t *trans, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src)
{
	s4_index_t *index;
	entry_t *entry;
	GList *entries;
	int ret;
	s4_t *s4 = _transaction_get_db (trans);

	index = _index_get_a (s4, key_a, 0);
	if (index == NULL) {
		return 0;
	}

	if (!_index_lock_shared (index, trans)) goto deadlocked;
	entries = _index_search (index, NULL, (void*)val_a);

	if (entries == NULL) {
		return 0;
	} else {
		entry = entries->data;
		g_list_free (entries);
	}

	if (!_entry_lock_exclusive (entry, trans)) goto deadlocked;
	ret = _entry_delete (entry, key_b, val_b, src);

	if (ret) {
		index = g_hash_table_lookup (s4->index_table, key_b);

		if (index != NULL) {
			if (!_index_lock_exclusive (index, trans)) goto deadlocked;
			_index_delete (index, val_b, entry);
		}
	}

	return ret;

deadlocked:
	_transaction_set_deadlocked (trans);
	return 0;
}

/**
 * @{
 * @internal
 */

/**
 * An index function that matches everything
 *
 * @return 0
 */
static int _everything (void)
{
	return 0;
}

/**
 * Frees all relations in a database
 *
 * @param s4 The database to free in
 */
void _free_relations (s4_t *s4)
{
	GHashTableIter iter;
	s4_index_t *index;
	GList *entries;

	g_static_mutex_lock (&s4->rel_lock);
	g_hash_table_iter_init (&iter, s4->rel_table);

	while (g_hash_table_iter_next (&iter, NULL, (void**)&index)) {
		entries = _index_search (index, (index_function_t)_everything, NULL);

		for (; entries != NULL; entries = g_list_delete_link (entries, entries)) {
			entry_t *entry = entries->data;

			_lock_free (entry->lock);
			free (entry->data);
			free (entry);
		}
	}

	g_static_mutex_unlock (&s4->rel_lock);
}

typedef struct {
	s4_t *s4;
	entry_t *l;
} check_data_t;

/**
 * Checks an entry against a condition
 *
 * @param cond The condition to check
 * @param d The database and entry to check
 * @return 0 if the entry matched, non-zero otherwise
 */
static int _check_cond (s4_condition_t *cond, void *d)
{
	check_data_t *data = d;
	entry_t *l = data->l;
	int ret = 1;
	int i;

	if (s4_cond_is_combiner (cond)) {
		ret = s4_cond_get_combine_function (cond)(cond, _check_cond, d);
	} else if (s4_cond_is_filter (cond)) {
		const char *key = s4_cond_get_key (cond);
		int null = key == NULL;

		if (s4_cond_get_flags (cond) && S4_COND_PARENT) {
			if (key == l->key || null) {
				ret = s4_cond_get_filter_function (cond)(l->val, cond);
			}
		} else {
			i = 0;
			do {
				s4_sourcepref_t *sp = s4_cond_get_sourcepref (cond);
				int start, src, best_src = INT_MAX;

				if (null) {
					key = l->data[i].key;
				}

				start = _entry_search (l, key);

				for (i = start; i < l->size && l->data[i].key == key; i++) {
					if ((src = s4_sourcepref_get_priority (sp, l->data[i].src)) < best_src) {
						best_src = src;
					}
				}
				for (i = start; i < l->size && ret && l->data[i].key == key; i++) {
					if (best_src < INT_MAX &&
							s4_sourcepref_get_priority (sp, l->data[i].src) == best_src) {
						ret = s4_cond_get_filter_function (cond)(l->data[i].val, cond);
					}
				}
			} while (i < l->size && ret && null);
		}
	}

	return ret;
}

/**
 * Fetches values from an entry
 *
 * @param s4 The database the entry lives in
 * @param l The entry to fetch from
 * @param fs The fetchspec that tells us what to fetch
 * @return An array of results
 */
static s4_resultrow_t *_fetch (s4_t *s4, entry_t *l, s4_fetchspec_t *fs)
{
	s4_resultrow_t *row;
	int k,f;
	int fetch_size = s4_fetchspec_size (fs);

	row = s4_resultrow_create (fetch_size);

	for (k = 0; k < fetch_size; k++) {
		const char *fkey = s4_fetchspec_get_key (fs, k);
		int flags = s4_fetchspec_get_flags (fs, k);
		int null = fkey == NULL;
		s4_result_t *result;
		s4_sourcepref_t *sp = s4_fetchspec_get_sourcepref (fs, k);

		result = NULL;
		f = 0;

		if ((flags & S4_FETCH_PARENT) && (fkey == l->key || null)) {
			result = s4_result_create (result, l->key, l->val, NULL);
		}

		if (flags & S4_FETCH_DATA) {
			do {
				int src, start, best_src = INT_MAX;

				if (null && l->size > 0) {
					fkey = l->data[f].key;
				}

				start = _entry_search (l, fkey);

				for (f = start; f < l->size && l->data[f].key == fkey; f++) {
					if ((src = s4_sourcepref_get_priority (sp, l->data[f].src)) < best_src) {
						best_src = src;
					}
				}
				for (f = start; f < l->size && l->data[f].key == fkey; f++) {
					if (best_src < INT_MAX &&
							s4_sourcepref_get_priority (sp, l->data[f].src) == best_src) {
						result = s4_result_create (result, l->data[f].key,
								l->data[f].val, l->data[f].src);
					}
				}
			} while (f < l->size && null);
		}

		s4_resultrow_set_col (row, k, result);
	}

	return row;
}

/**
 * @}
 */

/**
 * Queries a database for all entries matching a condition,
 * then fetches data from them.
 *
 * @param trans The transaction this query belongs to.
 * @param fs The fetchspec to use when fetching data
 * @param cond The condition to check entries against
 * @return A resultset with a row for every entry that matched
 */
s4_resultset_t *_s4_query (
		s4_transaction_t *trans,
		s4_fetchspec_t *fs,
		s4_condition_t *cond)
{
	check_data_t data;
	GList *entries;
	s4_index_t *index;
	s4_resultset_t *ret = s4_resultset_create (s4_fetchspec_size (fs));
	s4_t *s4 = _transaction_get_db (trans);

	s4_cond_update_key (cond, s4);
	s4_fetchspec_update_key (s4, fs);

	if (s4_cond_is_filter (cond)
			&& (s4_cond_get_flags (cond) & S4_COND_PARENT)
			&& s4_cond_get_key (cond) != NULL) {
		index = _index_get_a (s4, s4_cond_get_key (cond), 0);

		if (index == NULL) {
			entries = NULL;
		} else {
			if (!_index_lock_shared (index, trans)) goto deadlocked;
			if (s4_cond_is_monotonic (cond)) {
				entries = _index_search (index, (index_function_t)s4_cond_get_filter_function (cond), cond);
			} else {
				entries = _index_lsearch (index, (index_function_t)s4_cond_get_filter_function (cond), cond);
			}
		}
	} else if (s4_cond_is_filter (cond)
			&& s4_cond_get_key (cond) != NULL
			&& (index = _index_get_b (s4, s4_cond_get_key (cond))) != NULL) {
		if (!_index_lock_shared (index, trans)) goto deadlocked;
		if (s4_cond_is_monotonic (cond)) {
			entries = _index_search (index, (index_function_t)s4_cond_get_filter_function (cond), cond);
		} else {
			entries = _index_lsearch (index, (index_function_t)s4_cond_get_filter_function (cond), cond);
		}
	} else {
		GList *indices;
		indices = _index_get_all_a (s4);

		for (entries = NULL; indices != NULL; indices = g_list_delete_link (indices, indices)) {
			if (!_index_lock_shared (indices->data, trans)) goto deadlocked;
			entries = g_list_concat (entries, _index_lsearch (indices->data, (index_function_t)_everything, NULL));
		}
	}

	data.s4 = s4;
	for (; entries != NULL; entries = g_list_delete_link (entries, entries)) {
		entry_t *entry = entries->data;
		data.l = entry;

		if (!_entry_lock_shared (entry, trans)) goto deadlocked;
		if (entry->size != 0 && !_check_cond (cond, &data))
			s4_resultset_add_row (ret, _fetch (s4, entry, fs));
	}

	return ret;

deadlocked:
	_transaction_set_deadlocked (trans);
	return ret;
}

/**
 * @}
 */
