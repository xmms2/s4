#include "s4_be.h"
#include "query.h"
#include "midb.h"
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

static int ip_search (ip_link_t *links, int size, int32_t val)
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

static void *ip_lookup (ip_node_t *node, int32_t val)
{
	int index = ip_search (node->links, node->size, val);

	if (index == node->size || node->links[index].val != val)
		return NULL;

	return node->links[index].data;
}

static void *ip_insert (ip_node_t *node, int32_t val, void *data)
{
	int index = ip_search (node->links, node->size, val);

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

static ip_leaf_t *ip_create_leaf (int32_t key, int32_t val)
{
	ip_leaf_t *ret = malloc (sizeof (ip_leaf_t));
	ret->key = key;
	ret->val = val;
	ret->size = 0;
	ret->alloc = 8;
	ret->data = malloc (sizeof (ip_data_t) * ret->alloc);

	return ret;
}

static ip_node_t *ip_create_node (void)
{
	ip_node_t *ret = malloc (sizeof (ip_leaf_t));
	ret->size = 0;
	ret->alloc = 8;
	ret->links = malloc (sizeof (ip_link_t) * ret->alloc);

	return ret;
}

static int ip_leaf_insert (ip_leaf_t *leaf, int32_t key, int32_t val, int32_t src)
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

static int ip_leaf_delete (ip_leaf_t *leaf, int32_t key, int32_t val, int32_t src)
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

			if (best != -1 && l->data[best].val == entry->val_i) {
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

static int check_cond (s4be_t *be, ip_leaf_t *l, s4_condition_t *cond)
{
	int ret = 0;
	int i;
	switch (cond->type) {
		case S4_COND_UNION:
			for (i = 0; !ret && cond->cond.operands[i] != NULL; i++)
				ret = check_cond (be, l, cond->cond.operands[i]);
			break;
		case S4_COND_INTERSECTION:
			for (ret = 1, i = 0; ret && cond->cond.operands[i] != NULL; i++)
				ret = check_cond (be, l, cond->cond.operands[i]);
			break;
		case S4_COND_COMPLEMENT:
			for (i = 0; ret && cond->cond.operands[i] != NULL; i++)
				ret = !check_cond (be, l, cond->cond.operands[i]);
			break;

		default:
			if (cond->cond.filter.key == 0) {
				ret = cond->cond.filter.func (l->val, cond->cond.filter.funcdata);
			} else {
				int src, best_pos, best_src = INT_MAX;
				for (i = 0; i < l->size; i++) {
					if ((l->data[i].key == cond->cond.filter.key ||
								l->data[i].key == -cond->cond.filter.key) &&
							(src = sp_get (be, l->data[i].src)) < best_src) {
						best_src = src;
						best_pos = i;
					}
				}

				if (best_src != INT_MAX)
					ret = cond->cond.filter.func (l->data[best_pos].val, cond->cond.filter.funcdata);
			}
			break;
	}

	return ret;
}

GList *s4be_ip_query (s4be_t *be, int32_t *fetch, int fetch_size, s4_condition_t *cond)
{
	GList *ret = NULL;
	ip_node_t *n;
	ip_leaf_t *l;
	int i, j, k, f;
	s4_val_t **vals;


	for (i = 0; i < root.size; i++) {
		n = root.links[i].data;
		for (j = 0; j < n->size; j++) {
			l = n->links[j].data;
			if (!check_cond (be, l, cond))
				continue;

			vals = malloc (sizeof (s4_val_t*) * fetch_size);

			for (k = 0; k < fetch_size; k++) {
				if (fetch[k] == 0) {
					vals[k] = malloc (sizeof (s4_val_t));
					vals[k]->type = S4_VAL_INT;
					vals[k]->val.i = l->val;

					continue;
				}

				int src, best_pos, best_src = INT_MAX;
				for (f = 0; f < l->size; f++) {
					if ((l->data[f].key == fetch[k] || l->data[f].key == -fetch[k]) &&
							(src = sp_get (be, l->data[f].src)) < best_src) {
						best_src = src;
						best_pos = f;
					}
				}
				if (best_src == INT_MAX) {
					vals[k] = NULL;
				} else {
					vals[k] = malloc (sizeof (s4_val_t));
					if (l->data[best_pos].key > 0) {
						vals[k]->val.s = s4be_st_reverse (be, l->data[best_pos].val);
						vals[k]->type = S4_VAL_STR;
					} else {
						vals[k]->val.i = l->data[best_pos].val;
						vals[k]->type = S4_VAL_INT;
					}
				}
			}
			ret = g_list_prepend (ret, vals);
		}
	}

	return ret;
}
