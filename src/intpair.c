#include "s4_priv.h"
#include <string.h>
#include <stdlib.h>

#define LINEAR_SEARCH_SIZE 10

typedef struct ip_link_St {
	int32_t val;
	void *data;
} ip_link_t;

typedef struct ip_node_St {
	int size, alloc;
	ip_link_t *links;
} ip_node_t;

typedef struct ip_data_St {
	int32_t key, val, src;
} ip_data_t;

typedef struct ip_leaf_St {
	int size, alloc;
	int32_t key, val;
	ip_data_t *data;
} ip_leaf_t;

ip_node_t root = {.size = 0, .alloc = 1, .links = NULL};

static int _ip_search (ip_link_t *links, int size, int32_t val)
{
	int lo, hi;
	lo = 0; hi = size;

	while ((hi - lo) > LINEAR_SEARCH_SIZE) {
		int middle = (hi + lo) / 2;
		int32_t lval = links[middle].val;
		if (lval == val)
			return middle;
		if (lval > val)
			hi = middle;
		else
			lo = middle + 1;
	}

	for (;lo < hi; lo++) {
		if (links[lo].val >= val)
			return lo;
	}
	return lo;
}

static void *_ip_lookup (ip_node_t *node, int32_t val)
{
	int index = _ip_search (node->links, node->size, val);

	if (index == node->size || node->links[index].val != val)
		return NULL;

	return node->links[index].data;
}

static void *_ip_insert (ip_node_t *node, int32_t val, void *data)
{
	int index = _ip_search (node->links, node->size, val);

	if (index == node->size || node->links[index].val != val) {
		if (node->size == node->alloc || node->links == NULL) {
			node->alloc *= 2;
			node->links = realloc (node->links,  sizeof (ip_link_t) * node->alloc);
		}

		memmove (node->links + index + 1, node->links + index,
				(node->size - index) * sizeof (ip_link_t));

		node->links[index].val = val;
		node->links[index].data = data;

		node->size++;
	}

	return data;
}

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

static ip_node_t *_ip_create_node (void)
{
	ip_node_t *ret = malloc (sizeof (ip_leaf_t));
	ret->size = 0;
	ret->alloc = 8;
	ret->links = malloc (sizeof (ip_link_t) * ret->alloc);

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
	ip_node_t *n;
	ip_leaf_t *l;

	n = _ip_lookup (&root, pair->key_a);
	if (n == NULL)
		n = _ip_insert (&root, pair->key_a, _ip_create_node());

	l = _ip_lookup (n, pair->val_a);
	if (l == NULL)
		l = _ip_insert (n, pair->val_a, _ip_create_leaf (pair->key_a, pair->val_a));

	return _ip_leaf_insert (l, pair->key_b, pair->val_b, pair->src);
}

int _ip_del (s4_t *be, s4_intpair_t *pair)
{
	ip_node_t *n;
	ip_leaf_t *l;

	n = _ip_lookup (&root, pair->key_a);
	if (n == NULL)
		return -1;

	l = _ip_lookup (n, pair->val_a);
	if (l == NULL)
		return -1;

	return _ip_leaf_delete (l, pair->key_b, pair->val_b, pair->src);
}

void _ip_foreach (s4_t *be, void (*func) (s4_intpair_t *pair, void *data), void *data)
{
	ip_node_t *n;
	ip_leaf_t *l;
	int i, j, k;
	s4_intpair_t pair;

	for (i = 0; i < root.size; i++) {
		n = root.links[i].data;
		for (j = 0; j < n->size; j++) {
			l = n->links[j].data;

			pair.key_a = l->key;
			pair.val_a = l->val;

			for (k = 0; k < l->size; k++) {
				pair.key_b = l->data[k].key;
				pair.val_b = l->data[k].val;
				pair.src = l->data[k].src;

				func (&pair, data);
			}
		}
	}
}
/*
int s4be_ip_add (s4be_t *be, s4_entry_t *entry, s4_entry_t *prop)
{
	ip_node_t *n;
	ip_leaf_t *l;

	n = ip_lookup (&root, entry->key_i);
	if (n == NULL)
		n = ip_insert (&root, entry->key_i, ip_create_node());

	l = ip_lookup (n, entry->val_i);
	if (l == NULL)
		l = ip_insert (n, entry->val_i, ip_create_leaf (entry->key_i, entry->val_i));

	sp_add (be, prop->src_i);

	return ip_leaf_insert (l, prop->key_i, prop->val_i, prop->src_i);
}

int s4be_ip_del (s4be_t *be, s4_entry_t *entry, s4_entry_t *prop)
{
	ip_node_t *n;
	ip_leaf_t *l;

	n = ip_lookup (&root, entry->key_i);
	if (n == NULL)
		return -1;

	l = ip_lookup (n, entry->val_i);
	if (l == NULL)
		return -1;

	return ip_leaf_delete (l, prop->key_i, prop->val_i, prop->src_i);
}

s4_set_t *s4be_ip_get (s4be_t *be, s4_entry_t *entry, int32_t key)
{
	s4_set_t *ret = NULL;
	ip_node_t *n;
	ip_leaf_t *l;
	s4_entry_t e;
	int best_src = INT_MAX;
	int i, src;

	n = ip_lookup (&root, entry->key_i);
	if (n == NULL)
		return NULL;

	l = ip_lookup (n, entry->val_i);
	if (l == NULL)
		return NULL;

	ret = s4_set_new (0);

	for (i = 0; i < l->size; i++) {
		if ((l->data[i].key == key || l->data[i].key == -key) &&
				(src = sp_get (be, l->data[i].src)) < best_src) {
			e.key_s = e.val_s = e.src_s = NULL;
			e.key_i = l->data[i].key;
			e.val_i = l->data[i].val;
			e.src_i = l->data[i].src;
			e.type = (e.key_i > 0)?ENTRY_STR:ENTRY_INT;
			best_src = src;
		}
	}

	if (best_src != INT_MAX)
		s4_set_insert (ret, &e);

	return ret;
}

static int ip_check (int32_t val, int32_t should_be)
{
	return val == should_be;
}

s4_set_t *s4be_ip_has_this (s4be_t *be, s4_entry_t *entry)
{
	ip_node_t *n;
	ip_leaf_t *l;
	int i, j, k;
	s4_set_t *ret = s4_set_new (0);

	for (i = 0; i < root.size; i++) {
		n = root.links[i].data;
		for (j = 0; j < n->size; j++) {
			int best = -1;
			int src, best_src = INT_MAX;
			l = n->links[j].data;

			for (k = 0; k < l->size; k++) {
				if (l->data[k].key == entry->key_i && (src = sp_get (be, l->data[k].src)) < best_src) {
					best = k;
					best_src = src;
				}
			}

			if (best != -1 && ip_check (l->data[best].val, entry->val_i)) {
				s4_entry_t e;
				e.val_s = e.key_s = e.src_s = NULL;
				e.key_i = l->key;
				e.val_i = l->val;
				e.src_i = l->data[best].src;
				e.type = (e.key_i > 0)?ENTRY_STR:ENTRY_INT;
				s4_set_insert (ret, &e);
			}
		}
	}

	return ret;
}

s4_set_t *s4be_ip_this_has (s4be_t *be, s4_entry_t *entry)
{
	s4_set_t *ret = NULL;
	ip_node_t *n;
	ip_leaf_t *l;
	s4_entry_t e;
	int i;

	n = ip_lookup (&root, entry->key_i);
	if (n == NULL)
		return NULL;

	l = ip_lookup (n, entry->val_i);
	if (l == NULL)
		return NULL;

	ret = s4_set_new (0);

	for (i = 0; i < l->size; i++) {
		e.key_s = e.val_s = e.src_s = NULL;
		e.key_i = l->data[i].key;
		e.val_i = l->data[i].val;
		e.src_i = l->data[i].src;
		e.type = (e.key_i > 0)?ENTRY_STR:ENTRY_INT;
		s4_set_insert (ret, &e);
	}

	return ret;
}

s4_set_t *s4be_ip_smaller (s4be_t *be, s4_entry_t *entry, int key)
{
	ip_node_t *n;
	ip_leaf_t *l;
	int i, j, k;
	s4_set_t *ret = s4_set_new (0);

	for (i = 0; i < root.size; i++) {
		n = root.links[i].data;
		for (j = 0; j < n->size; j++) {
			int best = -1;
			int src, best_src = INT_MAX;
			l = n->links[j].data;

			for (k = 0; k < l->size; k++) {
				if (l->data[k].key == entry->key_i && (src = sp_get (be, l->data[k].src)) < best_src) {
					best = k;
					best_src = src;
				}
			}

			if (best != -1 && l->data[best].val < entry->val_i) {
				s4_entry_t e;
				e.val_s = e.key_s = e.src_s = NULL;
				if (key) {
					e.key_i = l->data[best].key;
					e.val_i = l->data[best].val;
				} else {
					e.key_i = l->key;
					e.val_i = l->val;
				}
				e.src_i = l->data[best].src;
				e.type = (e.key_i > 0)?ENTRY_STR:ENTRY_INT;
				s4_set_insert (ret, &e);
			}
		}
	}

	return ret;
}

s4_set_t *s4be_ip_greater (s4be_t *be, s4_entry_t *entry, int key)
{
	ip_node_t *n;
	ip_leaf_t *l;
	int i, j, k;
	s4_set_t *ret = s4_set_new (0);

	for (i = 0; i < root.size; i++) {
		n = root.links[i].data;
		for (j = 0; j < n->size; j++) {
			int best = -1;
			int src, best_src = INT_MAX;
			l = n->links[j].data;

			for (k = 0; k < l->size; k++) {
				if (l->data[k].key == entry->key_i && (src = sp_get (be, l->data[k].src)) < best_src) {
					best = k;
					best_src = src;
				}
			}

			if (best != -1 && l->data[best].val > entry->val_i) {
				s4_entry_t e;
				e.val_s = e.key_s = e.src_s = NULL;
				if (key) {
					e.key_i = l->data[best].key;
					e.val_i = l->data[best].val;
				} else {
					e.key_i = l->key;
					e.val_i = l->val;
				}
				e.src_i = l->data[best].src;
				e.type = (e.key_i > 0)?ENTRY_STR:ENTRY_INT;
				s4_set_insert (ret, &e);
			}
		}
	}

	return ret;
}

void s4be_ip_foreach (s4be_t *be,
		void (*func) (s4_entry_t *e, s4_entry_t *p, void* userdata),
		void *userdata)
{
	s4_entry_t e, p;
	ip_node_t *n;
	ip_leaf_t *l;
	int i, j, k;

	for (i = 0; i < root.size; i++) {
		n = root.links[i].data;
		for (j = 0; j < n->size; j++) {
			l = n->links[j].data;

			e.val_s = e.key_s = e.src_s = NULL;
			e.key_i = l->key;
			e.val_i = l->val;
			e.src_i = 0;
			e.type = (e.key_i > 0)?ENTRY_STR:ENTRY_INT;

			for (k = 0; k < l->size; k++) {
				p.val_s = e.key_s = e.src_s = NULL;
				p.key_i = l->data[k].key;
				p.val_i = l->data[k].val;
				p.src_i = l->data[k].src;
				e.src_i = p.src_i;
				p.type = (p.key_i > 0)?ENTRY_STR:ENTRY_INT;

				func (&e, &p, userdata);
			}
		}
	}
}

GList *s4be_ip_fetch (s4be_t *be, s4_set_t *set, int32_t fetch[], int size)
{
	GList *ret = NULL;
	ip_node_t *n;
	ip_leaf_t *l;
	int i, j;
	int32_t src, prev_key = INT32_MIN;
	s4_entry_t *e;
	int32_t *best_src = malloc (sizeof (int32_t) * size);
	int32_t *best_pos = malloc (sizeof (int32_t) * size);
	s4_val_t **vals;
	int32_t id_val = s4be_st_lookup (be, "id");

	for (e = s4_set_next (set); e != NULL; e = s4_set_next (set)) {
		if (prev_key != e->key_i) {
			n = ip_lookup (&root, e->key_i);
			if (n == NULL)
				continue;
			prev_key = e->key_i;
		}
		l = ip_lookup (n, e->val_i);
		if (l == NULL)
			continue;

		for (i = 0; i < size; i++)
			best_src[i] = INT32_MAX;

		for (i = 0, j = 0; i < l->size; i++) {
			while (l->data[i].key > fetch[j] && j < size) j++;

			if (j >= size)
				break;

			if ((l->data[i].key == fetch[j] || l->data[i].key == -fetch[j]) &&
					(src = sp_get (be, l->data[i].src)) < best_src[j]) {
				best_pos[j] = i;
				best_src[j] = src;
			}
		}

		vals = malloc (sizeof (s4_val_t*) * size);

		for (i = 0; i < size; i++) {
			if (fetch[i] == id_val) {
				s4_val_t *val = malloc (sizeof (s4_val_t));

				val->val.i = l->val;
				val->type = S4_VAL_INT;
				vals[i] = val;
			}
			else if (best_src[i] != INT32_MAX) {
				s4_val_t *val = malloc (sizeof (s4_val_t));

				if (l->data[best_pos[i]].key > 0) {
					val->val.s = s4be_st_reverse (be, l->data[best_pos[i]].val);
					val->type = S4_VAL_STR;
				} else {
					val->val.i = l->data[best_pos[i]].val;
					val->type = S4_VAL_INT;
				}

				vals[i] = val;
			} else {
				vals[i] = NULL;
			}
		}

		ret = g_list_prepend (ret, vals);
	}

	free (best_pos);
	free (best_src);

	return ret;
}
*/
typedef struct {
	s4_t *s4;
	ip_leaf_t *l;
} check_data_t;

static int check_cond (s4_condition_t *cond, void *d)
{
	check_data_t *data = d;
	ip_leaf_t *l = data->l;
	int ret = 0;
	int i;

	if (s4_cond_is_combiner (cond)) {
		ret = s4_cond_get_combine_function (cond)(cond, check_cond, d);
	} else if (s4_cond_is_filter (cond)) {
		s4_val_t *val = NULL;
		int32_t key = _st_lookup (data->s4, s4_cond_get_key (cond));

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
		if (val != NULL)
			ret = s4_cond_get_filter_function (cond)(val, cond);
	}

	return ret;
}

static s4_val_t **_fetch (s4_t *s4, ip_leaf_t *l, int fetch_size, int32_t *ifetch)
{
	s4_val_t **vals;
	int k,f;

	vals = malloc (sizeof (s4_val_t*) * fetch_size);

	for (k = 0; k < fetch_size; k++) {
		if (ifetch[k] == 0) {
			vals[k] = s4_val_new_int (l->val);

			continue;
		}

		int src, best_pos, best_src = INT_MAX;
		for (f = 0; f < l->size; f++) {
			if ((l->data[f].key == ifetch[k] || l->data[f].key == -ifetch[k]) &&
					(src = 1) < best_src) {
				best_src = src;
				best_pos = f;
			}
		}
		if (best_src == INT_MAX) {
			vals[k] = NULL;
		} else {
			if (l->data[best_pos].key > 0) {
				vals[k] = s4_val_new_string (_st_reverse (s4, l->data[best_pos].val));
			} else {
				vals[k] = s4_val_new_int (l->data[best_pos].val);
			}
		}
	}

	return vals;
}

GList *s4_query (s4_t *s4, const char **fetch, s4_condition_t *cond)
{
	GList *ret = NULL;
	ip_node_t *n;
	ip_leaf_t *l;
	int i, j;
	check_data_t data;
	int fetch_size;
	int32_t *ifetch;
	int par_filt = 0;

	for (fetch_size = 0; fetch[fetch_size] != NULL; fetch_size++);

	ifetch = malloc (sizeof (int32_t) * fetch_size);

	for (i = 0; i < fetch_size; i++) {
		ifetch[i] = _st_lookup (s4, fetch[i]);
	}

	data.s4 = s4;

	if (s4_cond_is_filter (cond) && (s4_cond_get_flags (cond) & S4_COND_PARENT))
		par_filt = 1;

	for (i = 0; i < root.size; i++) {
		n = root.links[i].data;
		for (j = 0; j < n->size; j++) {
			l = n->links[j].data;
			data.l = l;
			if (!check_cond (cond, &data))
				continue;

			ret = g_list_prepend (ret, _fetch (s4, l, fetch_size, ifetch));
		}
	}

	return ret;
}
