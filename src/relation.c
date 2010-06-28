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
	s4_val_t *val;
	const char *src;
} entry_data_t;

typedef struct {
	const char *key;
	s4_val_t *val;
	int size, alloc;
	GStaticMutex lock;

	entry_data_t *data;
} entry_t;

#define LINEAR_SEARCH_SIZE 0

/**
 * @addtogroup S4
 * @{
 */

/**
 * @{
 * @internal
 */

/**
 * Creates an internal copy of val
 *
 * @param s4 The database to find constant strings in
 * @param val The value to copy
 * @return A new internal value that is equal to val
 */
static s4_val_t *_nocopy_val_copy (s4_t *s4, const s4_val_t *val)
{
	const char *str, *norm;

	if (s4_val_is_int (val)) {
		return s4_val_copy (val);
	}

	s4_val_get_str (val, &str);
	str = _string_lookup (s4, str);
	norm = _string_lookup_normalized (s4, str);

	return s4_val_new_internal_string (str, norm);
}

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

	for (; entry->data[lo].key < key && lo < hi; lo++);

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
static int _entry_insert (entry_t *entry, const char *key, s4_val_t *val, const char *src)
{
	int i = _entry_search (entry, key);

	for (; i < entry->size && entry->data[i].key == key; i++) {
		if (entry->data[i].src == src && !s4_val_cmp (entry->data[i].val, val, 1))
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
		if (entry->data[i].src == src && !s4_val_cmp (entry->data[i].val, val, 1)) {
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
static entry_t *_entry_create (const char *key, s4_val_t *val)
{
	entry_t *entry = malloc (sizeof (entry_t));

	entry->key = key;
	entry->val = val;
	entry->size = 0;
	entry->alloc = 1;
	g_static_mutex_init (&entry->lock);

	entry->data = malloc (sizeof (entry_data_t) * entry->alloc);

	return entry;
}

/**
 * @}
 */

/**
 * Adds a new relation to a database
 *
 * @param s4 The database to add to
 * @param key_a The key of the first entry
 * @param value_a The value of the first entry
 * @param key_b The key of the second entry
 * @param value_b The value of the second entry
 * @param src The source that made the relation
 * @return non-zero if everything went alrite, 0 otherwise
 */
int s4_add (s4_t *s4, const char *key_a, const s4_val_t *value_a,
		const char *key_b, const s4_val_t *value_b, const char *src)
{
	s4_index_t *index;
	entry_t *entry;
	GList *entries;
	int ret;
	s4_val_t *val_a, *val_b;

	key_a = _string_lookup (s4, key_a);
	key_b = _string_lookup (s4, key_b);
	src = _string_lookup (s4, src);

	g_static_mutex_lock (&s4->rel_lock);
	index = g_hash_table_lookup (s4->rel_table, key_a);

	if (index == NULL) {
		index = _index_create ();
		g_hash_table_insert (s4->rel_table, (void*)key_a, index);
	}
	g_static_mutex_unlock (&s4->rel_lock);

	entries = _index_search (index, NULL, (void*)value_a);

	if (entries == NULL) {
		val_a = _nocopy_val_copy (s4, value_a);
		entry = _entry_create (key_a, val_a);
		g_static_mutex_lock (&entry->lock);
		_index_insert (index, val_a, entry);
	} else {
		entry = entries->data;
		g_static_mutex_lock (&entry->lock);
		g_list_free (entries);
	}

	val_b = _nocopy_val_copy (s4, value_b);
	ret = _entry_insert (entry, key_b, val_b, src);
	g_static_mutex_unlock (&entry->lock);

	if (ret) {
		index = _index_get (s4, key_b);

		if (index != NULL) {
			_index_insert (index, val_b, entry);
		}

		_log_add (s4, key_a, value_a, key_b, value_b, src);
	} else {
		s4_val_free (val_b);
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
int s4_del (s4_t *s4, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src)
{
	s4_index_t *index;
	entry_t *entry;
	GList *entries;
	int ret;

	key_a = _string_lookup (s4, key_a);
	key_b = _string_lookup (s4, key_b);
	src = _string_lookup (s4, src);

	g_static_mutex_lock (&s4->rel_lock);
	index = g_hash_table_lookup (s4->rel_table, key_a);
	g_static_mutex_unlock (&s4->rel_lock);

	if (index == NULL) {
		return 0;
	}

	entries = _index_search (index, NULL, (void*)val_a);

	if (entries == NULL) {
		return 0;
	} else {
		entry = entries->data;
		g_list_free (entries);
	}

	g_static_mutex_lock (&entry->lock);
	ret = _entry_delete (entry, key_b, val_b, src);
	g_static_mutex_unlock (&entry->lock);

	if (ret) {
		index = g_hash_table_lookup (s4->index_table, key_b);

		if (index != NULL) {
			_index_delete (index, val_b, entry);
		}
		_log_del (s4, key_a, val_a, key_b, val_b, src);
	}

	return ret;
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
			int i;

			for (i = 0; i < entry->size; i++) {
				s4_val_free (entry->data[i].val);
			}

			s4_val_free (entry->val);

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

		if (s4_cond_get_flags (cond) && S4_COND_PARENT) {
			if (key == l->key) {
				ret = s4_cond_get_filter_function (cond)(l->val, cond);
			}
		} else {
			s4_sourcepref_t *sp = s4_cond_get_sourcepref (cond);
			int start, src, best_src = INT_MAX;

			start = _entry_search (l, key);
			for (i = start; l->data[i].key == key; i++) {
				if ((src = s4_sourcepref_get_priority (sp, l->data[i].src)) < best_src) {
					best_src = src;
				}
			}
			for (i = start; ret && l->data[i].key == key; i++) {
				if (s4_sourcepref_get_priority (sp, l->data[i].src) == best_src) {
					ret = s4_cond_get_filter_function (cond)(l->data[i].val, cond);
				}
			}
		}
	}

	return ret;
}

/**
 * Fetches values from an entry
 *
 * @param s4 The database the entry lives in
 * @param entry The entry to fetch from
 * @param fs The fetchspec that tells us what to fetch
 * @return An array of results
 */
static s4_result_t **_fetch (s4_t *s4, entry_t *l, s4_fetchspec_t *fs)
{
	s4_result_t **result;
	int k,f;
	int fetch_size = s4_fetchspec_size (fs);

	result = malloc (sizeof (s4_result_t*) * fetch_size);

	for (k = 0; k < fetch_size; k++) {
		const char *fkey = s4_fetchspec_get_key (fs, k);
		s4_sourcepref_t *sp = s4_fetchspec_get_sourcepref (fs, k);

		result[k] = NULL;

		if (fkey == NULL) {
			result[k] = s4_result_create (result[k], l->key, l->val, NULL);

			for (f = 0; f < l->size; f++) {
				result[k] = s4_result_create (result[k], l->data[f].key, l->data[f].val, l->data[f].src);
			}
		} else {
			int src, start, best_src = INT_MAX;
			if (fkey == l->key) {
				result[k] = s4_result_create (result[k], l->key, l->val, NULL);
			}

			start = _entry_search (l, fkey);

			for (f = start; l->data[f].key == fkey; f++) {
				if ((src = s4_sourcepref_get_priority (sp, l->data[f].src)) < best_src) {
					best_src = src;
				}
			}
			for (f = start; l->data[f].key == fkey; f++) {
				if (s4_sourcepref_get_priority (sp, l->data[f].src) == best_src) {
					result[k] = s4_result_create (result[k], l->data[f].key,
							l->data[f].val, l->data[f].src);
				}
			}
		}

	}

	return result;
}

/**
 * @}
 */

/**
 * Queries a database for all entries matching a condition,
 * then fetches data from them.
 *
 * @param s4 The database to search
 * @param fs The fetchspec to use when fetching data
 * @param cond The condition to check entries against
 * @return A resultset with a row for every entry that matched
 */
s4_resultset_t *s4_query (s4_t *s4, s4_fetchspec_t *fs, s4_condition_t *cond)
{
	check_data_t data;
	GList *entries;
	s4_index_t *index;
	s4_resultset_t *ret = NULL;

	s4_cond_update_key (s4, cond);
	s4_fetchspec_update_key (s4, fs);

	if (s4_cond_is_filter (cond) && (s4_cond_get_flags (cond) & S4_COND_PARENT)) {
		const char *key = s4_cond_get_key (cond);
		g_static_mutex_lock (&s4->rel_lock);
		index = g_hash_table_lookup (s4->rel_table, key);
		g_static_mutex_unlock (&s4->rel_lock);

		if (index == NULL)
			entries = NULL;
		else
			entries = _index_search (index, (index_function_t)s4_cond_get_filter_function (cond), cond);
	} else if (s4_cond_is_filter (cond) &&
			s4_cond_is_monotonic (cond) &&
			(index = _index_get (s4, s4_cond_get_key (cond))) != NULL) {
		entries = _index_search (index, (index_function_t)s4_cond_get_filter_function (cond), cond);
	} else {
		GList *indices = NULL;
		GHashTableIter iter;

		g_static_mutex_lock (&s4->rel_lock);
		g_hash_table_iter_init (&iter, s4->rel_table);
		while (g_hash_table_iter_next (&iter, NULL, (void**)&index)) {
			indices = g_list_prepend (indices, index);
		}
		g_static_mutex_unlock (&s4->rel_lock);

		for (entries = NULL; indices != NULL; indices = g_list_delete_link (indices, indices)) {
			entries = g_list_concat (entries, _index_search (index, (index_function_t)_everything, NULL));
		}
	}

	if (entries != NULL) {
		ret = s4_resultset_create (s4_fetchspec_size (fs));
	}

	data.s4 = s4;
	for (; entries != NULL; entries = g_list_delete_link (entries, entries)) {
		entry_t *entry = entries->data;
		data.l = entry;

		g_static_mutex_lock (&entry->lock);
		if (!_check_cond (cond, &data))
			s4_resultset_add_row (ret, _fetch (s4, entry, fs));
		g_static_mutex_unlock (&entry->lock);
	}

	return ret;
}

/**
 * Calls the given function for every relation in the database
 *
 * @param s4 The database to iterate over
 * @param func The function to call
 * @param data The data to be passed as the last argument to func
 */
void s4_foreach (s4_t *s4, void (*func)(s4_t *s4, const char *key, const s4_val_t *val_a,
			const char *key_b, const s4_val_t *val_b, const char *src, void *data), void *data)
{
	GHashTableIter iter;
	s4_index_t *index;
	GList *indices = NULL, *entries;

	g_static_mutex_lock (&s4->rel_lock);
	g_hash_table_iter_init (&iter, s4->rel_table);
	while (g_hash_table_iter_next (&iter, NULL, (void**)&index)) {
		indices = g_list_prepend (indices, index);
	}
	g_static_mutex_unlock (&s4->rel_lock);

	for (; indices != NULL; indices = g_list_delete_link (indices, indices)) {
		entries = _index_search (indices->data, (index_function_t)_everything, NULL);
		for (; entries != NULL; entries = g_list_delete_link (entries, entries)) {
			entry_t *entry = entries->data;
			int i;

			g_static_mutex_lock (&entry->lock);
			for (i = 0; i < entry->size; i++) {
				func (s4, entry->key, entry->val, entry->data[i].key, entry->data[i].val, entry->data[i].src, data);
			}
			g_static_mutex_unlock (&entry->lock);
		}
	}
}

/**
 * @}
 */
