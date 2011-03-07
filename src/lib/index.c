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
	void *data;
	int count;
} index_data_t;

typedef struct {
	const s4_val_t *val;
	int size, alloc;
	index_data_t *data;
} index_t;

struct s4_index_St {
	int size, alloc;
	s4_lock_t *lock;

	index_t *data;
};

struct s4_index_data_St {
	GHashTable *indexb_table, *indexa_table;
	GStaticMutex indexb_table_lock, indexa_table_lock;
};

/**
 *
 * @internal
 * @defgroup Index Index
 * @ingroup S4
 * @brief Search indexes for S4
 *
 */

s4_index_data_t *_index_create_data ()
{
	s4_index_data_t *ret = malloc (sizeof (s4_index_data_t));

	ret->indexa_table = g_hash_table_new_full (NULL, NULL,
	                                           NULL, (GDestroyNotify)_index_free);
	ret->indexb_table = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                           free, (GDestroyNotify)_index_free);
	g_static_mutex_init (&ret->indexa_table_lock);
	g_static_mutex_init (&ret->indexb_table_lock);

	return ret;
}

void _index_free_data (s4_index_data_t *data)
{
	g_hash_table_destroy (data->indexa_table);
	g_hash_table_destroy (data->indexb_table);
	g_static_mutex_free (&data->indexa_table_lock);
	g_static_mutex_free (&data->indexb_table_lock);
}

/**
 * Gets the a-index associated with key.
 * A a-index is used to lookup entries by the a-value.
 * If an index has not yet been created for this key and
 * create is non-zero, one will be created and returned.
 *
 * @param s4 The database to look for the index in
 * @param key The key the index should be indexing
 * @param create Creates the index if it does not exist
 * @return The index, or NULL if it does not exist and create is 0.
 */
s4_index_t *_index_get_a (s4_t *s4, const char *key, int create)
{
	s4_index_t *ret;

	g_static_mutex_lock (&s4->index_data->indexa_table_lock);
	ret = g_hash_table_lookup (s4->index_data->indexa_table, key);
	if (ret == NULL && create) {
		ret = _index_create ();
		g_hash_table_insert (s4->index_data->indexa_table, (void*)key, ret);
	}
	g_static_mutex_unlock (&s4->index_data->indexa_table_lock);

	return ret;
}

/**
 * Gets the b-index associated with key.
 * A b-index is used to lookup entries by the b-value.
 *
 * @param s4 The database to look for the index in
 * @param key The key the index should be indexing
 * @return The index, or NULL if it is not found
 */
s4_index_t *_index_get_b (s4_t *s4, const char *key)
{
	s4_index_t *ret;

	g_static_mutex_lock (&s4->index_data->indexb_table_lock);
	ret = g_hash_table_lookup (s4->index_data->indexb_table, key);
	g_static_mutex_unlock (&s4->index_data->indexb_table_lock);

	return ret;
}

/* A helper function for the _index_get_all_... functions.
 * It will prepend all values in an hash-table to a list
 */
static void _prepend_value_to_list (void *key, void *value, void *list)
{
	GList **l = list;
	*l = g_list_prepend (*l, value);
}

/**
 * Gets all a-indexes.
 *
 * @param s4 The database to get the index of.
 * @return A list of indexes.
 */
GList *_index_get_all_a (s4_t *s4)
{
	GList *ret = NULL;

	g_static_mutex_lock (&s4->index_data->indexa_table_lock);
	g_hash_table_foreach (s4->index_data->indexa_table,
	                      _prepend_value_to_list, &ret);
	g_static_mutex_unlock (&s4->index_data->indexa_table_lock);

	return ret;
}

/**
 * Gets all b-indexes.
 *
 * @param s4 The database to get the index of.
 * @return A list of indexes.
 */
GList *_index_get_all_b (s4_t *s4)
{
	GList *ret = NULL;

	g_static_mutex_lock (&s4->index_data->indexb_table_lock);
	g_hash_table_foreach (s4->index_data->indexb_table,
	                      _prepend_value_to_list, &ret);
	g_static_mutex_unlock (&s4->index_data->indexb_table_lock);

	return ret;
}

/**
 * Creates a new index
 *
 * @return A new index
 */
s4_index_t *_index_create ()
{
	s4_index_t *ret = malloc (sizeof (s4_index_t));
	ret->size = 0;
	ret->alloc = 1;
	ret->data = malloc (sizeof (index_t) * ret->alloc);
	ret->lock = _lock_alloc ();

	return ret;
}

/**
 * Adds an index to a database
 *
 * @param s4 The database to add the index to
 * @param key The key to associate the index with
 * @param index The index to insert
 * @return 0 if the key already has an index, non-zero otherwise
 */
int _index_add (s4_t *s4, const char *key, s4_index_t *index)
{
	int ret = 0;

	g_static_mutex_lock (&s4->index_data->indexb_table_lock);
	if (g_hash_table_lookup (s4->index_data->indexb_table, key) == NULL) {
		g_hash_table_insert (s4->index_data->indexb_table, strdup (key), index);
		ret = 1;
	}
	g_static_mutex_unlock (&s4->index_data->indexb_table_lock);

	return ret;
}

static int _data_search (index_t *index, void *data)
{
	int lo = 0;
	int hi = index->size;

	while ((hi -lo) > 0) {
		int m = (hi + lo) / 2;

		if (data == index->data[m].data)
			return m;
		if (data < index->data[m].data)
			lo = m + 1;
		else
			hi = m;
	}

	return lo;
}

static int _bsearch (s4_index_t *index, index_function_t func, void *funcdata)
{
	int lo = 0;
	int hi = index->size;

	while ((hi -lo) > 0) {
		int m = (hi + lo) / 2;
		int c = func (index->data[m].val, funcdata);

		if (c == 0)
			return m;
		if (c < 0)
			lo = m + 1;
		else
			hi = m;
	}

	return lo;
}

static int _val_cmp (const s4_val_t *v1, const s4_val_t *v2)
{
	return s4_val_cmp (v1, v2, S4_CMP_CASELESS);
}

/**
 * Inserts a new value-data pair into the index
 *
 * @param index The index to insert into
 * @param val The value to associate the data with
 * @param new_data The data
 * @return 1
 */
int _index_insert (s4_index_t *index, const s4_val_t *val, void *new_data)
{
	int i,j;

	i = _bsearch (index, (index_function_t)_val_cmp, (void*)val);

	if (i >= index->size || _val_cmp (val, index->data[i].val)) {
		if (index->size >= index->alloc) {
			index->alloc *= 2;
			index->data = realloc (index->data, sizeof (index_t) * index->alloc);
		}
		memmove (index->data + i + 1, index->data + i, (index->size - i) * sizeof (index_t));
		index->data[i].val = val;
		index->data[i].size = 0;
		index->data[i].alloc = 1;
		index->data[i].data = malloc (sizeof (index_data_t) * index->data[i].alloc);

		index->size++;
	}

	j = _data_search (index->data + i, new_data);

	if (j >= index->data[i].size || new_data != index->data[i].data[j].data) {
		if (index->data[i].size >= index->data[i].alloc) {
			index->data[i].alloc *= 2;
			index->data[i].data = realloc (index->data[i].data, sizeof (index_data_t) * index->data[i].alloc);
		}
		memmove (index->data[i].data + j + 1, index->data[i].data + j,
				(index->data[i].size - j) * sizeof (index_data_t));
		index->data[i].data[j].data = new_data;
		index->data[i].data[j].count = 1;

		index->data[i].size++;
	} else {
		index->data[i].data[j].count++;
	}

	return 1;
}

/**
 * Removes a value-data pair from the index
 *
 * @param index The index to remove from
 * @param val The value to remove
 * @param data The data to remove
 * @return 0 if the value-data pair is not found, 1 otherwise
 */
int _index_delete (s4_index_t *index, const s4_val_t *val, void *data)
{
	int i,j;

	i = _bsearch (index, (index_function_t)_val_cmp, (void*)val);
	if (i >= index->size || _val_cmp (val, index->data[i].val)) {
		return 0;
	}

	j = _data_search (index->data + i, data);
	if (j >= index->size || data != index->data[i].data) {
		return 0;
	}

	if (--index->data[i].data[j].count <= 0) {
		memmove (index->data[i].data + j, index->data[i].data + j + 1,
				(index->data[i].size - j - 1) * sizeof (index_data_t));
		index->data[i].size--;
	}

	if (index->data[i].size <= 0) {
		free (index->data[i].data);
		memmove (index->data + i, index->data + i + i, (index->size - i - 1) * sizeof (index_t));
		index->size--;
	}

	return 1;
}

/**
 * Searches an index
 *
 * @param index The index to search
 * @param func The function to use when searching. It must be monotonic,
 * It should return 0  if the value matches, -1 if the value is too small
 * and 1 if the value is too big,
 * @param func_data Data passed as the second argument to func
 * @return A GList where list->data is the data found matching
 */
GList *_index_search (s4_index_t *index, index_function_t func, void *func_data)
{
	int i,j;
	GHashTable *found;
	GList *ret = NULL;

	if (func == NULL)
		func = (index_function_t)_val_cmp;

	i = _bsearch (index, func, func_data);

	if (i >= index->size || func (index->data[i].val, func_data)) {
		return NULL;
	}

	found = g_hash_table_new (NULL, NULL);

	for (; i >= 0 && !func (index->data[i].val, func_data); i--); i++;
	for (; i < index->size && !func (index->data[i].val, func_data); i++) {
		for (j = 0; j < index->data[i].size; j++) {
			if (g_hash_table_lookup (found, index->data[i].data[j].data) == NULL) {
				g_hash_table_insert (found, index->data[i].data[j].data, (void*)1);
				ret = g_list_prepend (ret, index->data[i].data[j].data);
			}
		}
	}

	g_hash_table_destroy (found);

	return ret;
}

/**
 * Searches an index using a linear search.
 *
 * @param index The index to search
 * @param func The function to use when searching.
 * It should return 0  if the value matches, -1 if the value is too small
 * and 1 if the value is too big,
 * @param func_data Data passed as the second argument to func
 * @return A GList where list->data is the data found matching
 */
GList *_index_lsearch (s4_index_t *index, index_function_t func, void *func_data)
{
	int i, j;
	GList *ret = NULL;
	GHashTable *found;

	found = g_hash_table_new (NULL, NULL);

	for (i = 0; i < index->size; i++) {
		if (!func (index->data[i].val, func_data)) {
			for (j = 0; j < index->data[i].size; j++) {
				if (g_hash_table_lookup (found, index->data[i].data[j].data) == NULL) {
					g_hash_table_insert (found, index->data[i].data[j].data, (void*)1);
					ret = g_list_prepend (ret, index->data[i].data[j].data);
				}
			}
		}
	}

	g_hash_table_unref (found);

	return ret;
}

/**
 * Frees an index. The values and data is NOT freed
 *
 * @param index The index to free
 */
void _index_free (s4_index_t *index)
{
	int i;

	for (i = 0; i < index->size; i++) {
		free (index->data[i].data);
	}

	_lock_free (index->lock);
	free (index->data);
	free (index);
}

int _index_lock_shared (s4_index_t *index, s4_transaction_t *trans)
{
	return _lock_shared (index->lock, trans);
}

int _index_lock_exclusive (s4_index_t *index, s4_transaction_t *trans)
{
	return _lock_exclusive (index->lock, trans);
}

/**
 * @}
 */
