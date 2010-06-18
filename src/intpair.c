#include "s4_priv.h"
#include <string.h>
#include <stdlib.h>

#define LINEAR_SEARCH_SIZE 10

typedef struct ip_data_St {
	int32_t key, val, src;
} ip_data_t;

typedef struct ip_leaf_St {
	int size, alloc;
	int32_t key, val;
	ip_data_t *data;
} ip_leaf_t;


static ip_leaf_t *_ip_create_leaf (int32_t key, int32_t val)
{
	ip_leaf_t *ret = malloc (sizeof (ip_leaf_t));
	ret->key = key;
	ret->val = val;
	ret->size = 0;
	ret->alloc = 8;
	ret->data = malloc (sizeof (ip_data_t) * ret->alloc);

	return ret;
}

static int _ip_leaf_insert (ip_leaf_t *leaf, int32_t key, int32_t val, int32_t src)
{
	int i;

	for (i = 0; i < leaf->size; i++) {
		ip_data_t d = leaf->data[i];
		if (d.key == key && d.val == val && d.src == src)
			return -1;
		if (d.key > key || (d.key == key && (d.val > val || (d.val == val && (d.src > src))))) {
			break;
		}
	}

	if (leaf->size == leaf->alloc) {
		leaf->alloc *= 2;
		leaf->data = realloc (leaf->data, sizeof (ip_data_t) * leaf->alloc);
	}

	memmove (leaf->data + i + 1, leaf->data + i,
			(leaf->size - i) * sizeof (ip_data_t));

	leaf->data[i].key = key;
	leaf->data[i].val = val;
	leaf->data[i].src = src;
	leaf->size++;

	return 0;
}

static int _ip_leaf_delete (ip_leaf_t *leaf, int32_t key, int32_t val, int32_t src)
{
	int i;
	for (i = 0; i < leaf->size; i++) {
		ip_data_t d = leaf->data[i];
		if (d.key == key &&	d.val == val &&	d.src == src) {
			memmove (leaf->data + i, leaf->data + i + 1,
					(leaf->size - i - 1) * sizeof (ip_data_t));
			leaf->size--;
			return 0;
		}
	}

	return -1;
}

int _ip_add (s4_t *be, s4_intpair_t *pair)
{
	GList *res;
	ip_leaf_t *l;
	int ret;
	const char *key_str = _st_reverse (be, ABS(pair->key_b));
	s4_index_t *index;
	s4_val_t *val;

	g_static_rw_lock_writer_lock (&be->intpair_lock);

	index = g_hash_table_lookup (be->intpair_table, GINT_TO_POINTER (ABS (pair->key_a)));
	if (index == NULL) {
		index = _index_create ();
		g_hash_table_insert (be->intpair_table, GINT_TO_POINTER (ABS (pair->key_a)), index);
	}

	if (pair->key_a > 0) {
		val = s4_val_new_string_nocopy (_st_reverse (be, pair->val_a));
	} else {
		val = s4_val_new_int (pair->val_a);
	}

	res = _index_search (index, NULL, val);

	if (res == NULL) {
		l = _ip_create_leaf (pair->key_a, pair->val_a);
		_index_insert (index, val, l);
	} else {
		l = res->data;
		g_list_free (res);
	}

	s4_val_free (val);

	ret = _ip_leaf_insert (l, pair->key_b, pair->val_b, pair->src);

	if (!ret && (index = _index_get (be, key_str)) != NULL) {
		s4_val_t *val;
		if (pair->key_b > 0) {
			val = s4_val_new_string_nocopy (_st_reverse (be, pair->val_b));
		} else {
			val = s4_val_new_int (pair->val_b);
		}

		_index_insert (index, val, l);

		s4_val_free (val);
	}

	g_static_rw_lock_writer_unlock (&be->intpair_lock);

	return ret;
}

int _ip_del (s4_t *be, s4_intpair_t *pair)
{
	GList *res;
	ip_leaf_t *l;
	int ret;
	const char *key_str = _st_reverse (be, ABS(pair->key_b));
	s4_index_t *index;
	s4_val_t *val;

	g_static_rw_lock_writer_lock (&be->intpair_lock);

	index = g_hash_table_lookup (be->intpair_table, GINT_TO_POINTER (ABS (pair->key_a)));
	if (index == NULL) {
		g_static_rw_lock_writer_unlock (&be->intpair_lock);
		return -1;
	}

	if (pair->key_a > 0) {
		val = s4_val_new_string_nocopy (_st_reverse (be, pair->val_a));
	} else {
		val = s4_val_new_int (pair->val_a);
	}

	res = _index_search (index, NULL, val);

	if (res == NULL) {
		g_static_rw_lock_writer_unlock (&be->intpair_lock);
		return -1;
	}

	l = res->data;
	g_list_free (res);
	s4_val_free (val);

	ret = _ip_leaf_delete (l, pair->key_b, pair->val_b, pair->src);

	if (!ret && (index = _index_get (be, key_str)) != NULL) {
		s4_val_t *val;
		if (pair->key_b > 0) {
			val = s4_val_new_string_nocopy (_st_reverse (be, pair->val_b));
		} else {
			val = s4_val_new_int (pair->val_b);
		}

		_index_delete (index, val, l);

		s4_val_free (val);
	}
	g_static_rw_lock_writer_unlock (&be->intpair_lock);

	return ret;
}

static int _everything (void)
{
	return 0;
}

void _ip_foreach (s4_t *be, void (*func) (s4_intpair_t *pair, void *data), void *data)
{
	int i;
	ip_leaf_t *l;
	s4_intpair_t pair;
	GHashTableIter iter;
	s4_index_t *index_a;
	GList *leaves;

	g_hash_table_iter_init (&iter, be->intpair_table);

	while (g_hash_table_iter_next (&iter, NULL, (void**)&index_a)) {
		leaves = _index_search (index_a, (index_function_t)_everything, NULL);

		for (; leaves != NULL; leaves = g_list_delete_link (leaves, leaves)) {
			l = leaves->data;
			for (i = 0; i < l->size; i++) {
				pair.key_a = l->key;
				pair.val_a = l->val;
				pair.key_b = l->data[i].key;
				pair.val_b = l->data[i].val;
				pair.src = l->data[i].src;

				func (&pair, data);
			}
		}
	}
}

typedef struct {
	s4_t *s4;
	ip_leaf_t *l;
} check_data_t;

static int check_cond (s4_condition_t *cond, void *d)
{
	check_data_t *data = d;
	ip_leaf_t *l = data->l;
	int ret = 1;
	int i;

	if (s4_cond_is_combiner (cond)) {
		ret = s4_cond_get_combine_function (cond)(cond, check_cond, d);
	} else if (s4_cond_is_filter (cond)) {
		s4_val_t *val = NULL;
		int32_t key = s4_cond_get_ikey (cond);
		if (key == 0) {
			int32_t ikey = _st_lookup (data->s4, s4_cond_get_key (cond));
			s4_cond_set_ikey (cond, ikey);
			key = ikey;
		}

		if (s4_cond_get_flags (cond) && S4_COND_PARENT) {
			if (key == ABS(l->key)) {
				if (l->key < 0) {
					val = s4_val_new_int (l->val);
				} else {
					val = s4_val_new_string_nocopy (_st_reverse (data->s4, l->val));
				}
			}
		} else {
			int src, best_pos, best_src = INT_MAX;
			for (i = 0; i < l->size; i++) {
				if (ABS(l->data[i].key) == key &&
						(src = s4_sourcepref_get_priority (s4_cond_get_sourcepref (cond), l->data[i].src)) < best_src) {
					best_src = src;
					best_pos = i;
				}
			}

			if (best_src != INT_MAX) {
				if (l->data[best_pos].key < 0) {
					val = s4_val_new_int (l->data[best_pos].val);
				} else {
					val = s4_val_new_string (_st_reverse (data->s4, l->data[best_pos].val));
				}
			}
		}
		if (val != NULL) {
			ret = s4_cond_get_filter_function (cond)(val, cond);
			s4_val_free (val);
		}
	}

	return ret;
}

static s4_result_t **_fetch (s4_t *s4, ip_leaf_t *l, s4_fetchspec_t *fs)
{
	s4_result_t **result;
	int k,f;
	int fetch_size = s4_fetchspec_size (fs);
	s4_val_t *val;

	result = malloc (sizeof (s4_result_t*) * fetch_size);

	for (k = 0; k < fetch_size; k++) {
		const char *key = _st_reverse (s4, ABS (l->key));
		const char *fkey = s4_fetchspec_get_key (fs, k);
		int32_t ikey = (fkey != NULL)? _st_lookup (s4, fkey):0;
		s4_sourcepref_t *sp = s4_fetchspec_get_sourcepref (fs, k);

		result[k] = NULL;

		if (fkey == NULL || ikey == ABS (l->key)) {
			val = s4_val_new_int (l->val);
			result[k] = s4_result_create (result[k], key, val, NULL);
		}

		int src, best_pos, best_src = INT_MAX;
		for (f = 0; f < l->size; f++) {
			if (fkey == NULL) {
				if (l->data[f].key > 0) {
					val = s4_val_new_string (_st_reverse (s4, l->data[f].val));
				} else {
					val = s4_val_new_int (l->data[f].val);
				}
				result[k] = s4_result_create (result[k], _st_reverse (s4, ABS(l->data[f].key)),
						val, _st_reverse (s4, ABS(l->data[f].src)));
			} else {
				if (ABS(l->data[f].key) == ikey &&
						(src = s4_sourcepref_get_priority (sp, l->data[f].src)) < best_src) {
					best_src = src;
					best_pos = f;
				}
			}
		}
		if (best_src != INT_MAX) {
			if (l->data[best_pos].key > 0) {
				val = s4_val_new_string (_st_reverse (s4, l->data[best_pos].val));
			} else {
				val = s4_val_new_int (l->data[best_pos].val);
			}
			result[k] = s4_result_create (result[k], _st_reverse (s4, ABS(l->data[best_pos].key)),
							val, _st_reverse (s4, ABS(l->data[best_pos].src)));
		}
	}

	return result;
}

s4_resultset_t *s4_query (s4_t *s4, s4_fetchspec_t *fs, s4_condition_t *cond)
{
	check_data_t data;
	GList *leaves;
	s4_index_t *index;
	s4_resultset_t *ret = NULL;

	g_static_rw_lock_reader_lock (&s4->intpair_lock);

	if (s4_cond_is_filter (cond) && (s4_cond_get_flags (cond) & S4_COND_PARENT)) {
		int32_t key = _st_lookup (s4, s4_cond_get_key (cond));
		index = g_hash_table_lookup (s4->intpair_table, GINT_TO_POINTER (ABS (key)));

		if (index == NULL)
			leaves = NULL;
		else
			leaves = _index_search (index, (index_function_t)s4_cond_get_filter_function (cond), cond);
	} else if (s4_cond_is_filter (cond) &&
			s4_cond_is_continuous(cond) &&
			(index = _index_get (s4, s4_cond_get_key (cond))) != NULL) {
		leaves = _index_search (index, (index_function_t)s4_cond_get_filter_function (cond), cond);
	} else {
		int32_t key = _st_lookup (s4, "song_id");
		index = g_hash_table_lookup (s4->intpair_table, GINT_TO_POINTER (ABS (key)));

		if (index == NULL)
			leaves = NULL;
		else
			leaves = _index_search (index, (index_function_t)_everything, cond);
	}

	if (leaves != NULL) {
		ret = s4_resultset_create (s4_fetchspec_size (fs));
	}

	data.s4 = s4;
	for (; leaves != NULL; leaves = g_list_delete_link (leaves, leaves)) {
		data.l = leaves->data;
		if (check_cond (cond, &data))
			continue;

		s4_resultset_add_row (ret, _fetch (s4, data.l, fs));
	}

	g_static_rw_lock_reader_unlock (&s4->intpair_lock);
	return ret;
}
