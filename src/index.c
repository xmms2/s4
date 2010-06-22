#include "s4_priv.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
	void *data;
	int count;
} index_data_t;

typedef struct {
	s4_val_t *val;
	int size, alloc;
	index_data_t *data;
} index_t;

struct s4_index_St {
	GStaticMutex lock;
	int size, alloc;

	index_t *data;
};

/**
 *
 * @internal
 * @defgroup Index Index
 * @ingroup S4
 * @brief Search indexes for S4
 *
 */

/**
 * Gets the index associated with key
 *
 * @param s4 The database to look for the index in
 * @param key The key the index should be indexing
 * @return The index, or NULL if it is not found
 */
s4_index_t *_index_get (s4_t *s4, const char *key)
{
	s4_index_t *ret;

	g_static_mutex_lock (&s4->index_table_lock);
	ret = g_hash_table_lookup (s4->index_table, key);
	g_static_mutex_unlock (&s4->index_table_lock);

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

	g_static_mutex_init (&ret->lock);

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
	g_static_mutex_lock (&s4->index_table_lock);
	if (g_hash_table_lookup (s4->index_table, key) == NULL) {
		g_hash_table_insert (s4->index_table, strdup (key), index);
		ret = 1;
	}
	g_static_mutex_unlock (&s4->index_table_lock);

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

static int _val_cmp (const s4_val_t *v1, s4_val_t *v2)
{
	return s4_val_cmp (v1, v2, 1);
}

/**
 * Inserts a new value-data pair into the index
 *
 * @param index The index to insert into
 * @param val The value to associate the data with
 * @param new_data The data
 * @return 1
 */
int _index_insert (s4_index_t *index, s4_val_t *val, void *new_data)
{
	int i,j;

	g_static_mutex_lock (&index->lock);

	i = _bsearch (index, (index_function_t)_val_cmp, val);

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

	if (j >= index->size || new_data != index->data[i].data[j].data) {
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

	g_static_mutex_unlock (&index->lock);

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

	g_static_mutex_lock (&index->lock);

	i = _bsearch (index, (index_function_t)_val_cmp, (void*)val);
	if (i >= index->size || _val_cmp (val, index->data[i].val)) {
		g_static_mutex_unlock (&index->lock);
		return 0;
	}

	j = _data_search (index->data + i, data);
	if (j >= index->size || data != index->data[i].data) {
		g_static_mutex_unlock (&index->lock);
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

	g_static_mutex_unlock (&index->lock);

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
	GHashTableIter iter;
	void *key;
	GList *ret = NULL;

	g_static_mutex_lock (&index->lock);

	if (func == NULL)
		func = (index_function_t)_val_cmp;

	i = _bsearch (index, func, func_data);

	if (i >= index->size || func (index->data[i].val, func_data)) {
		g_static_mutex_unlock (&index->lock);
		return NULL;
	}

	found = g_hash_table_new (NULL, NULL);

	for (i = i; i >= 0 && !func (index->data[i].val, func_data); i--); i++;
	for (; i < index->size && !func (index->data[i].val, func_data); i++) {
		for (j = 0; j < index->data[i].size; j++) {
			g_hash_table_insert (found, index->data[i].data[j].data, (void*)1);
		}
	}

	g_hash_table_iter_init (&iter, found);
	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		ret = g_list_prepend (ret, key);
	}

	g_hash_table_destroy (found);

	g_static_mutex_unlock (&index->lock);

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

	g_static_mutex_free (&index->lock);
	free (index->data);
	free (index);
}

/**
 * @}
 */
