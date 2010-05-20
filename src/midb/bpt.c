/*  S4 - An XMMS2 medialib backend
 *  Copyright (C) 2009 Sivert Berg
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

#include "midb.h"
#include "bpt.h"
#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LEAF_MAGIC 0x12345678
#define INT_MAGIC  0x87654321

/* The node size, it must be odd or some of the calculations will be off
 * Lowest size possible = 5
 * Tweak until performance is good
 */
#define SIZE 101 /* This leads to a node size of 2036 bytes, pretty close to
					2048 and thus we don't waste too much space */

/**
 *
 * @defgroup Btree B+tree
 * @ingroup Homegrown
 * @brief A B+ tree for the homegrown backend
 *
 * For more information on B+ trees I recommend
 * <a href=http://en.wikipedia.org/wiki/B+_tree>http://en.wikipedia.org/wiki/B+_tree</a> or
 * <a href=http://www.cecs.csulb.edu/~monge/classes/share/B+TreeIndexes.html>http://www.cecs.csulb.edu/~monge/classes/share/B+TreeIndexes.html</a>
 *
 * @{
 */

/* Internal node */
struct bpt_node_St {
	int32_t magic;
	int32_t key_count;
	bpt_node_t *parent;
	bpt_node_t *next;

	bpt_record_t keys[SIZE];
	bpt_node_t *pointers[0];
};


/* Prototypes */
static bpt_node_t *_bpt_insert_internal (bpt_node_t *node,
		bpt_node_t *child, bpt_record_t record);
static void _bpt_underflow (bpt_t *bpt, bpt_node_t *node);


/* Handy debug function */
static void _print_tree (bpt_node_t *p, int depth)
{
	int i,j;

	if (p == NULL) {
		printf ("(null)\n");
		return;
	}


	for (i = 0; i < p->key_count; i++) {
		if (p->magic != LEAF_MAGIC)
			_print_tree (p->pointers[i], depth + 1);

		for (j = 0; j < depth; j++)
			printf ("-");
		printf ("%.2i (%i %i %i %i %i)\n", i, p->keys[i].key_a, p->keys[i].val_a,
				p->keys[i].key_b, p->keys[i].val_b, p->keys[i].src);

	}
	if (p->magic != LEAF_MAGIC)
		_print_tree (p->pointers[i], depth + 1);

	if (depth == 0)
		printf("\n");
}

/* Set the root node in the bpt structure */
static void _bpt_set_root (bpt_t *bpt, bpt_node_t *root)
{
	bpt->root = root;
}

/* Get the root node from the bpt structure */
static bpt_node_t *_bpt_get_root (bpt_t *bpt)
{
	return bpt->root;
}

/* Set the first leaf in the bpt structure */
static void _bpt_set_leaves (bpt_t *bpt, bpt_node_t *leaves)
{
	bpt->leaves = leaves;
}

/* Get the first leaf for this tree */
static bpt_node_t *_bpt_get_leaves (bpt_t *bpt)
{
	return bpt->leaves;
}

/* Create a new leaf node */
static bpt_node_t *_bpt_create_leaf (void)
{
	bpt_node_t *p = malloc (sizeof (bpt_node_t));

	p->parent = NULL;
	p->magic = LEAF_MAGIC;
	p->key_count = 0;
	p->next = NULL;

	return p;
}


/* Create a new internal node */
static bpt_node_t *_bpt_create_internal (void)
{
	bpt_node_t *p = malloc (sizeof (bpt_node_t) +
			sizeof(bpt_node_t*) * (SIZE + 1));

	p->parent = NULL;
	p->magic = INT_MAGIC;
	p->key_count = 0;
	p->next = NULL;

	return p;
}

/* Compare to records,
 * return <0 if a<b, 0 if a=b and >0 if a>b
 */
static int _bpt_comp (bpt_record_t *a, bpt_record_t *b)
{
	int ret = 0;
	ret = (a->key_a < b->key_a)?-1:(a->key_a > b->key_a);
	if (!ret)
		ret = (a->val_a < b->val_a)?-1:(a->val_a > b->val_a);
	if (!ret)
		ret = (a->key_b < b->key_b)?-1:(a->key_b > b->key_b);
	if (!ret)
		ret = (a->val_b < b->val_b)?-1:(a->val_b > b->val_b);
	if (!ret)
		ret = (a->src < b->src)?-1:(a->src > b->src);

	return ret;
}


/* Search the node n for the first key bigger than r */
static int _bpt_search (bpt_node_t *n, bpt_record_t *r)
{
	int lower = 0;
	int upper = n->key_count;

	while (upper > lower) {
		int middle = (lower + upper) / 2;
		int c = _bpt_comp (r, &n->keys[middle]);

		if (c < 0) {
			upper = middle;
		} else if (c > 0) {
			lower = middle + 1;
		} else {
			return middle + 1;
		}
	}

	return upper;
}


/* Searches for the leaf that might contain record */
static bpt_node_t *_bpt_find_leaf (bpt_t *bpt, bpt_record_t *record)
{
	bpt_node_t *cur = _bpt_get_root(bpt);
	int index = 0;

	while (cur != NULL && cur->magic != LEAF_MAGIC) {
		index = _bpt_search (cur, record);
		cur = cur->pointers[index];
	}

	return cur;
}


/* Helper function for _bpt_split
 * Copies half of the keys (and pointers if it's an internal node)
 * over in the new node.
 */
static void _bpt_move_keys (bpt_node_t *pl, bpt_node_t *pn,
		int32_t index, bpt_node_t *child, bpt_record_t record)
{
	int i, j;
	int upper = index > (SIZE / 2);
	int leaf = child == NULL;

	/* Copy half of the keys in pl (and record if it should be placed
	 * in the upper half of pl) into pn.
	 */
	for (j = 0, i = SIZE / 2 + upper; j < (SIZE/2 + 1); j++) {
		if (i == index && upper) {
			index = -1;
			pn->keys[j] = record;
			if (!leaf)
				pn->pointers[j + 1] = child;
		} else {
			pn->keys[j] = pl->keys[i++];
			if (!leaf)
				pn->pointers[j + 1] = pl->pointers[i];
		}
		if (!leaf) {
			pn->pointers[j + 1]->parent = pn;
		}
	}

	/* Insert record in pl if it's not in the upper half */
	if (!upper) {
		for (i = SIZE / 2; i > index; i--) {
			pl->keys[i] = pl->keys[i - 1];
			if (!leaf)
				pl->pointers[i + 1] = pl->pointers[i];
		}

		pl->keys[index] = record;
		if (!leaf)
			pl->pointers[index + 1] = child;
	}

	/* If this is not a leaf we need to update the parent pointers
	 * of all keys copied to pn
	 */
	if (!leaf) {
		pn->pointers[0] = pl->pointers[SIZE / 2 + 1];
		pn->pointers[0]->parent = pn;
	}

	pl->key_count = SIZE / 2 + leaf;
	pn->key_count = SIZE / 2 + 1;
}

/* Static: split a node
 *
 * child should be NULL if node is a leaf
 *
 * Return NULL on error, 0 on success or a positive number
 * that is the new root.
 */
static bpt_node_t *_bpt_split (bpt_node_t *node,
		bpt_record_t record, bpt_node_t *child)
{
	int index;
	bpt_node_t *new;
	int leaf = child == NULL;

	index = _bpt_search (node, &record);

	/* If the key is already in the leaf we can't insert it */
	if (index > 0 && _bpt_comp (&node->keys[index - 1], &record) == 0) {
		return (void*)-1;
	}

	new = (leaf) ? (_bpt_create_leaf ()) : (_bpt_create_internal ());

	new->parent = node->parent;
	new->next = node->next;
	node->next = new;

	_bpt_move_keys (node, new, index, child, record);

	/* If we're the root we need to create a new root and insert pl and pn */
	if (node->parent == NULL) {
		bpt_node_t *parent = _bpt_create_internal();
		parent->pointers[0] = node;
		new->parent = node->parent = parent;
	}

	/* Insert the new node in the parent */
	return _bpt_insert_internal (new->parent, new,
			 leaf?(new->keys[0]):(node->keys[SIZE/2]));
}


/* Insert a key into an internal node
 * Return 0 on success, the node if key_count == 0
 */
static bpt_node_t *_bpt_insert_internal (bpt_node_t *node,
		bpt_node_t *child, bpt_record_t record)
{
	bpt_node_t *ret = NULL;

	if (node->key_count < SIZE) {
		int index = _bpt_search (node, &record);
		int i;

		/* TODO: memmove ? */
		for (i = node->key_count; i > index; i--) {
			node->keys[i] = node->keys[i - 1];
			node->pointers[i + 1] = node->pointers[i];
		}

		node->keys[index] = record;
		node->pointers[index + 1] = child;
		node->key_count++;

		if (node->key_count == 1) {
			ret = node;
		}
	} else {
		ret = _bpt_split (node, record, child);
	}

	return ret;
}


/* A helper function for _bpt_left_sibling */
static bpt_node_t *_bpt_left_helper (int depth,
		bpt_node_t *node, bpt_record_t *key, int down)
{
	if (node == NULL)
		return NULL;

	int index = down ? (node->key_count) : (_bpt_search (node, key) - 1);
	bpt_node_t *ret;

	/* If key is smaller than everyone in node we need to go one step up,
	 * otherwise we go one step down or return the pointer we found.
	 */
	if (index < 0)
		ret = _bpt_left_helper (depth + 1, node->parent, key, 0);
	else if (depth == 0)
		ret = node->pointers[index];
	else
		ret = _bpt_left_helper (depth - 1, node->pointers[index], key, 1);

	return ret;
}


/* Return the left sibling of node, or -1 if it doesn't exist */
static bpt_node_t *_bpt_left_sibling (bpt_node_t *node)
{
	return _bpt_left_helper (0, node->parent, &node->keys[0], 0);
}


/* Replace the old key with the new key in the parent (or the parents parent) */
static bpt_record_t _bpt_update_parent (bpt_node_t *parent,
		bpt_record_t old, bpt_record_t new)
{
	int index;
	bpt_record_t ret;

	if (parent == NULL)
		return old;

	index = _bpt_search (parent, &old) - 1;

	if (index < 0) {
		ret = _bpt_update_parent (parent->parent, old, new);
	} else {
		ret = parent->keys[index];
		parent->keys[index] = new;
	}

	return ret;
}


/* Remove the given key (or the one closest) from the internal node */
static bpt_record_t _bpt_remove_internal (bpt_t *bpt,
		bpt_node_t *node, bpt_record_t key)
{
	int index = _bpt_search (node, &key) - 1;
	bpt_record_t ret;
	int i;

	/* If key is smaller than everyone in node we remove the smallest
	 * one in this node and replaces key with that one in the parent
	 */
	if (index < 0) {
		bpt_record_t tmp = node->keys[0];
		/*TODO: memmove? */
		for (i = 1; i < node->key_count; i++)
			node->keys[i - 1] = node->keys[i];
		for (i = 1; i <= node->key_count; i++)
			node->pointers[i - 1] = node->pointers[i];

		ret =  _bpt_update_parent (node->parent, key, tmp);
	} else {
		ret = node->keys[index];

		/* TODO: memmove? */
		for (i = index; i < node->key_count; i++) {
			node->keys[i] = node->keys[i + 1];
			node->pointers[i + 1] = node->pointers[i + 2];
		}
	}

	node->key_count--;

	if (node->key_count < SIZE/2 && node->parent != NULL)
		_bpt_underflow (bpt, node);
	else if (node->key_count == 0) {
		bpt_node_t *child = node->pointers[0];
		_bpt_set_root (bpt, child);
		child->parent = NULL;
		free (node);
	}

	return ret;
}


/* Merge two nodes and delete one of them */
static void _bpt_merge_nodes (bpt_t *bpt, int leaf,
		bpt_node_t *lo, bpt_node_t *hi)
{
	int i;
	int add = !leaf;

	/* Copy all keys and pointers of hi into lo */
	for (i = 0; i < hi->key_count; i++)
		lo->keys[i + lo->key_count + add] = hi->keys[i];
	for (i = 0; i <= hi->key_count && !leaf; i++) {
		lo->pointers[i + lo->key_count + 1] = hi->pointers[i];
		hi->pointers[i]->parent = lo;
	}

	lo->key_count += hi->key_count + add;
	lo->next = hi->next;

	/* Delete the key pointing to hi and insert the key into lo.
	 * (If we're a leaf we simply overwrite the same key).
	 */
	lo->keys[lo->key_count - hi->key_count - add] =
		_bpt_remove_internal (bpt, hi->parent, hi->keys[0]);

	free (hi);
}


/* Take keys from one node and put them in the other so that there
 * is the same amount of keys in both (+- 1).
 */
static void _bpt_blend_nodes (int leaf,
		bpt_node_t *lo, bpt_node_t *hi)
{
	int diff = (hi->key_count - lo->key_count) / 2;
	int i;
	int add = !leaf;

	if (diff > 0) {
		/* Update the parent with the new key and save the old key in plo */
		lo->keys[lo->key_count] = _bpt_update_parent (hi->parent,
				hi->keys[0], hi->keys[diff - add]);

		/* Copy keys and pointers from high to low node */
		for (i = 0; i < (diff - 1); i++)
			lo->keys[lo->key_count + i + 1] = hi->keys[i + leaf];
		for (i = 0; i < diff && !leaf; i++) {
			lo->pointers[lo->key_count + i + 1] = hi->pointers[i];
			hi->pointers[i]->parent = lo;
		}
		/* Move keys and pointers inside the high node */
		for (i = diff; i < hi->key_count; i++)
			hi->keys[i - diff] = hi->keys[i];
		for (i = diff; i <= hi->key_count && !leaf; i++)
			hi->pointers[i - diff] = hi->pointers[i];

		hi->key_count -= diff;
		lo->key_count += diff;
	} else if (diff < 0) {
		diff = -diff;

		/* Move the keys and pointers in the high node to make room */
		for (i = hi->key_count - 1; i >= 0; i--)
			hi->keys[i + diff] = hi->keys[i];
		for (i = hi->key_count; i >= 0 && !leaf; i--)
			hi->pointers[i + diff] = hi->pointers[i];

		/* Copy keys and pointers from low to high */
		for (i = 0; i < (diff - add); i++)
			hi->keys[i] = lo->keys[i + lo->key_count - (diff - add)];
		for (i = 0; i < diff && !leaf; i++) {
			hi->pointers[i] = lo->pointers[i + lo->key_count - diff + 1];
			hi->pointers[i]->parent = hi;
		}

		/* Update the parent with the new key and insert the old key in hi */
		hi->keys[diff - add] = _bpt_update_parent (hi->parent,
				hi->keys[diff], lo->keys[lo->key_count - diff]);

		hi->key_count += diff;
		lo->key_count -= diff;
	}
}


/* Deal with underflow in a node */
static void _bpt_underflow (bpt_t *bpt, bpt_node_t *node)
{
	int leaf = node->magic == LEAF_MAGIC;
	bpt_node_t *right = node->next;
	bpt_node_t *left = _bpt_left_sibling (node);
	bpt_node_t *hi, *lo;

	if (right == NULL || (left != NULL && left->key_count > right->key_count)) {
		hi = node;
		lo = left;
	} else {
		hi = right;
		lo = node;
	}

	if ((lo->key_count + hi->key_count) < (SIZE + leaf))
		_bpt_merge_nodes (bpt, leaf, lo, hi);
	else
		_bpt_blend_nodes (leaf, lo, hi);
}


/**
 * Insert a new record into the tree
 *
 * @param be The database handle
 * @param bpt The tree to insert into
 * @param record The record to insert
 * @return 0 on success, -1 on error
 */
int bpt_insert (bpt_t *bpt, bpt_record_t *record)
{
	bpt_node_t *leaf = _bpt_find_leaf (bpt, record);

	if (leaf == NULL) {
		/* There are no root, we create one */
		leaf = _bpt_create_leaf ();
		leaf->key_count = 1;
		leaf->keys[0] = *record;
		_bpt_set_root (bpt, leaf);
		_bpt_set_leaves (bpt, leaf);
	} else if (leaf->key_count < SIZE) {
		/* There's room for more keys in this leaf, we simpy add it */
		int index = _bpt_search (leaf, record);

		/* Check if it already exists */
		if (index > 0 && _bpt_comp (&leaf->keys[index - 1], record) == 0) {
			return -1;
		}

		memmove (leaf->keys + index + 1, leaf->keys + index,
				(leaf->key_count - index) * sizeof (bpt_record_t));

		leaf->keys[index] = *record;
		leaf->key_count++;
	} else {
		/* The leaf is full, we need to split it */
		if ((leaf = _bpt_split (leaf, *record, NULL)) != NULL) {
			if (leaf == (void*)-1) {
				return -1;
			}
			_bpt_set_root (bpt, leaf);
		}
	}

	return 0;
}


/**
 * Remove the record given from the tree
 *
 * @param be The database handle
 * @param bpt The tree
 * @param record The record to remove
 * @return 0 on success, -1 on error.
 */
int bpt_remove (bpt_t *bpt, bpt_record_t *record)
{
	bpt_node_t *leaf = _bpt_find_leaf (bpt, record);
	int index;

	if (leaf == NULL)
		return -1;

	index = _bpt_search (leaf, record) - 1;

	/* Bail if the node doesn't exist */
	if (index < 0 || _bpt_comp (&leaf->keys[index], record) != 0)
		return -1;

	if (index == 0)
		_bpt_update_parent (leaf->parent, leaf->keys[0], leaf->keys[1]);

	leaf->key_count--;
	if (index < leaf->key_count) {
		memmove (leaf->keys + index, leaf->keys + index + 1,
				(leaf->key_count - index) * sizeof (bpt_record_t));
	}

	/* Check for underflow emptiness (if we're the root) */
	if (leaf->key_count <= SIZE / 2 && leaf->parent != NULL)
		_bpt_underflow (bpt, leaf);
	else if (leaf->key_count == 0) {
		free (leaf);
		_bpt_set_root (bpt, NULL);
		_bpt_set_leaves (bpt, NULL);
	}

	return 0;
}


/**
 * Find all entries between start (inclusive) and stop (exclusive).
 *
 * @param be The database handle
 * @param bpt The B+ tree
 * @param start Where to start the search
 * @param stop Where to stop the search
 * @param key Set to 1 if you want to save the index key instead of the key data.
 * @return A set with all entries between start and stop
 */
s4_set_t *bpt_find (bpt_t *bpt, bpt_record_t *start, bpt_record_t *stop, int key)
{
	s4_set_t *set = NULL;
	s4_entry_t entry;
	int index;
	int32_t val;
	bpt_node_t *leaf;

	val = start->val_a;

	leaf = _bpt_find_leaf (bpt, start);

	if (leaf == NULL)
		return NULL;

	index = _bpt_search (leaf, start);

	if (index > 0)
		index--;

	while (leaf != NULL && _bpt_comp (stop, &leaf->keys[index]) > 0) {
		/* Only add this record if it's higher or equal to start */
		if (_bpt_comp (start, &leaf->keys[index]) <= 0) {
			if (set == NULL) {
				set = s4_set_new (0);
			}

			entry.src_s = entry.key_s = entry.val_s = NULL;
			if (key) {
				entry.key_i = leaf->keys[index].key_a;
				entry.val_i = leaf->keys[index].val_a;
			} else {
				entry.key_i = leaf->keys[index].key_b;
				entry.val_i = leaf->keys[index].val_b;
			}
			entry.src_i = leaf->keys[index].src;
			if (entry.key_i < 0)
				entry.type = ENTRY_INT;
			else
				entry.type = ENTRY_STR;

			s4_set_insert (set, &entry);
		}

		if (++index >= leaf->key_count) {
			leaf = leaf->next;
			index = 0;
		}
	}

	return set;
}


/**
 * Calls func for all the entries it find.
 *
 * @param be The database to search through.
 * @param bpt The tree to go through.
 * @param func The function to call.
 * @param userdata Userdata passed to the function.
 *
 */
void bpt_foreach (bpt_t *bpt,
		void (*func)(bpt_record_t, void *userdata),
		void *userdata)
{
	bpt_node_t *leaf = _bpt_get_leaves (bpt);

	while (leaf != NULL && leaf->magic == LEAF_MAGIC) {
		int i;
		for (i = 0; i < leaf->key_count; i++)
			func (leaf->keys[i], userdata);

		leaf = leaf->next;
	}
}

struct verify_info {
	int outside;
	int magic;
	int key_small;
	int key_great;
	int key_order;
	int diff_height;
};

/* Helper function for bpt_verify, return 0 if it's invalid,
 * the height if it's okay
 */
static int _bpt_verify (bpt_node_t *node,
		bpt_record_t *lo, bpt_record_t *hi,
		struct verify_info *info)
{
	int i;
	int ret = 1;
	int height = 0;
	bpt_record_t *a, *b;

	if (node == NULL) {
		info->outside++;
		return 0;
	}

	if (node->magic != INT_MAGIC && node->magic != LEAF_MAGIC) {
		info->magic++;
		return 0;
	}

	for (i = 0; i < node->key_count; i++) {
		if (_bpt_comp (lo, &node->keys[i]) > 0) {
			info->key_small++;
			ret = 0;
		} else if (_bpt_comp (hi, &node->keys[i]) <= 0) {
			info->key_great++;
			ret = 0;
		} else if (i > 0 && _bpt_comp (&node->keys[i -1], &node->keys[i]) >= 0) {
			info->key_order++;
			ret = 0;
		}
	}

	for (i = 0, a = lo, b = &node->keys[0]
			; i <= node->key_count && node->magic == INT_MAGIC
			; i++) {
		int tmp = _bpt_verify (node->pointers[i], a, b, info);

		if (tmp == 0) {
			ret = 0;
		} else if (i == 0) {
			height = tmp;
		} else if (tmp != height) {
			info->diff_height++;
			ret = 0;
		}

		a = b;

		if ((i + 1) >= node->key_count)
			b = hi;
		else
			b = &node->keys[i + 1];
	}

	return (ret)?(height + 1):0;
}

/**
 * Check the given tree for inconsistenties.
 *
 * @param be The database the tree is in
 * @param bpt The tree
 * @return 1 if everything is okay, 0 otherwise
 *
 */
int bpt_verify (bpt_t *bpt)
{
	bpt_record_t lo, hi;
	struct verify_info info;
	int ret;

	memset (&info, 0, sizeof (struct verify_info));

	lo.key_a = lo.key_b = lo.val_a = lo.val_b = lo.src = INT32_MIN;
	hi.key_a = hi.key_b = hi.val_a = hi.val_b = hi.src = INT32_MAX;

	ret = !!_bpt_verify (_bpt_get_root (bpt), &lo, &hi, &info);

	if (!ret) {
		S4_ERROR ("B+ tree inconsistent!");
		S4_ERROR ("%i pointers point outside the database", info.outside);
		S4_ERROR ("%i nodes have wrong magic numbers", info.magic);
		S4_ERROR ("%i keys are too small", info.key_small);
		S4_ERROR ("%i keys are too big", info.key_great);
		S4_ERROR ("%i keys are in the wrong order", info.key_order);
		S4_ERROR ("%i subtrees have different height", info.diff_height);
	}

	return ret;
}

bpt_t *bpt_create (void)
{
	bpt_t *bpt = malloc (sizeof (bpt_t));
	bpt->root = NULL;
	bpt->leaves = NULL;

	return bpt;
}

static void _bpt_destroy_helper (bpt_node_t *node)
{
	int i;

	if (node->magic == INT_MAGIC) {
		for (i = 0; i <= node->key_count; i++) {
			_bpt_destroy_helper (node->pointers[i]);
		}
	}

	free (node);
}

void bpt_destroy (bpt_t *bpt)
{
	if (bpt->root != NULL) {
		_bpt_destroy_helper (bpt->root);
	}

	free (bpt);
}

/**
 * @}
 */
