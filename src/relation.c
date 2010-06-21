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

static s4_val_t *_nocopy_val_copy (s4_t *s4, const s4_val_t *val)
{
	const char *str;

	if (s4_val_is_int (val)) {
		return s4_val_copy (val);
	}

	s4_val_get_str (val, &str);
	return s4_val_new_string_nocopy (_string_lookup (s4, str));
}

static int _entry_search (entry_t *entry, const char *key)
{
	int lo = 0;
	int hi = entry->size;

	while (hi > lo) {
		int m = (hi + lo) / 2;

		if (entry->data[m].key < key)
			lo = m + 1;
		else
			hi = m;
	}

	return lo;
}

static int _entry_insert (entry_t *entry, const char *key, s4_val_t *val, const char *src)
{
	int i = _entry_search (entry, key);

	if (i < entry->size && entry->data[i].key < key)
		i++;

	for (; i < entry->size && entry->data[i].key == key; i++) {
		if (entry->data[i].src == src && !s4_val_comp (entry->data[i].val, val))
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

static int _entry_delete (entry_t *entry, const char *key, const s4_val_t *val, const char *src)
{
	int i = _entry_search (entry, key);
	int found = 0;

	if (i < entry->size && entry->data[i].key < key)
		i++;

	for (; i < entry->size && entry->data[i].key == key; i++) {
		if (entry->data[i].src == src && !s4_val_comp (entry->data[i].val, val)) {
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

	g_static_rw_lock_writer_lock (&s4->rel_lock);
	index = g_hash_table_lookup (s4->rel_table, key_a);

	if (index == NULL) {
		index = _index_create ();
		g_hash_table_insert (s4->rel_table, (void*)key_a, index);
	}
	g_static_rw_lock_writer_unlock (&s4->rel_lock);

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
	} else {
		s4_val_free (val_b);
	}

	return ret;
}

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

	g_static_rw_lock_writer_lock (&s4->rel_lock);
	index = g_hash_table_lookup (s4->rel_table, key_a);
	g_static_rw_lock_writer_unlock (&s4->rel_lock);

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
	}

	return ret;
}

typedef struct {
	s4_t *s4;
	entry_t *l;
} check_data_t;

static int check_cond (s4_condition_t *cond, void *d)
{
	check_data_t *data = d;
	entry_t *l = data->l;
	int ret = 1;
	int i;

	if (s4_cond_is_combiner (cond)) {
		ret = s4_cond_get_combine_function (cond)(cond, check_cond, d);
	} else if (s4_cond_is_filter (cond)) {
		s4_val_t *val = NULL;
		const char *key = _string_lookup (data->s4, s4_cond_get_key (cond));

		if (s4_cond_get_flags (cond) && S4_COND_PARENT) {
			if (key == l->key) {
				val = l->val;
			}
		} else {
			int src, best_pos, best_src = INT_MAX;
			for (i = 0; i < l->size; i++) {
				if (l->data[i].key == key &&
						(src = s4_sourcepref_get_priority (s4_cond_get_sourcepref (cond), l->data[i].src)) < best_src) {
					best_src = src;
					best_pos = i;
				}
			}

			if (best_src != INT_MAX) {
				val = l->data[best_pos].val;
			}
		}
		if (val != NULL) {
			ret = s4_cond_get_filter_function (cond)(val, cond);
		}
	}

	return ret;
}

static s4_result_t **_fetch (s4_t *s4, entry_t *l, s4_fetchspec_t *fs)
{
	s4_result_t **result;
	int k,f;
	int fetch_size = s4_fetchspec_size (fs);

	result = malloc (sizeof (s4_result_t*) * fetch_size);

	for (k = 0; k < fetch_size; k++) {
		const char *fkey = _string_lookup (s4, s4_fetchspec_get_key (fs, k));
		s4_sourcepref_t *sp = s4_fetchspec_get_sourcepref (fs, k);

		result[k] = NULL;

		if (fkey == NULL || fkey == l->key) {
			result[k] = s4_result_create (result[k], l->key, l->val, NULL);
		}

		int src, best_pos, best_src = INT_MAX;
		for (f = 0; f < l->size; f++) {
			if (fkey == NULL) {
				result[k] = s4_result_create (result[k], l->data[f].key, l->data[f].val, l->data[f].src);
			} else {
				if (l->data[f].key == fkey &&
						(src = s4_sourcepref_get_priority (sp, l->data[f].src)) < best_src) {
					best_src = src;
					best_pos = f;
				}
			}
		}
		if (best_src != INT_MAX) {
			result[k] = s4_result_create (result[k], l->data[best_pos].key,
					l->data[best_pos].val, l->data[best_pos].src);
		}
	}

	return result;
}

static int _everything (void)
{
	return 0;
}

s4_resultset_t *s4_query (s4_t *s4, s4_fetchspec_t *fs, s4_condition_t *cond)
{
	check_data_t data;
	GList *entries;
	s4_index_t *index;
	s4_resultset_t *ret = NULL;

	if (s4_cond_is_filter (cond) && (s4_cond_get_flags (cond) & S4_COND_PARENT)) {
		const char *key = _string_lookup (s4, s4_cond_get_key (cond));
		g_static_rw_lock_reader_lock (&s4->rel_lock);
		index = g_hash_table_lookup (s4->rel_table, key);
		g_static_rw_lock_reader_unlock (&s4->rel_lock);

		if (index == NULL)
			entries = NULL;
		else
			entries = _index_search (index, (index_function_t)s4_cond_get_filter_function (cond), cond);
	} else if (s4_cond_is_filter (cond) &&
			s4_cond_is_continuous(cond) &&
			(index = _index_get (s4, s4_cond_get_key (cond))) != NULL) {
		entries = _index_search (index, (index_function_t)s4_cond_get_filter_function (cond), cond);
	} else {
		index = g_hash_table_lookup (s4->rel_table, _string_lookup (s4, "song_id"));

		if (index == NULL)
			entries = NULL;
		else
			entries = _index_search (index, (index_function_t)_everything, NULL);
	}

	if (entries != NULL) {
		ret = s4_resultset_create (s4_fetchspec_size (fs));
	}

	data.s4 = s4;
	for (; entries != NULL; entries = g_list_delete_link (entries, entries)) {
		entry_t *entry = entries->data;
		data.l = entry;

		g_static_mutex_lock (&entry->lock);
		if (!check_cond (cond, &data))
			s4_resultset_add_row (ret, _fetch (s4, entry, fs));
		g_static_mutex_unlock (&entry->lock);
	}

	return ret;
}

void s4_foreach (s4_t *s4, void (*func)(s4_t *s4, const char *key, const s4_val_t *val_a,
			const char *key_b, const s4_val_t *val_b, const char *src, void *data), void *data)
{
	GHashTableIter iter;
	s4_index_t *index;
	GList *entries;

	g_static_rw_lock_reader_lock (&s4->rel_lock);
	g_hash_table_iter_init (&iter, s4->rel_table);

	while (g_hash_table_iter_next (&iter, NULL, (void**)&index)) {
		entries = _index_search (index, (index_function_t)_everything, NULL);

		for (; entries != NULL; entries = g_list_delete_link (entries, entries)) {
			entry_t *entry = entries->data;
			int i;

			for (i = 0; i < entry->size; i++) {
				func (s4, entry->key, entry->val, entry->data[i].key, entry->data[i].val, entry->data[i].src, data);
			}
		}
	}

	g_static_rw_lock_reader_unlock (&s4->rel_lock);
}

void s4_free_relations (s4_t *s4)
{
	GHashTableIter iter;
	s4_index_t *index;
	GList *entries;

	g_static_rw_lock_reader_lock (&s4->rel_lock);
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

	g_static_rw_lock_reader_unlock (&s4->rel_lock);
}
