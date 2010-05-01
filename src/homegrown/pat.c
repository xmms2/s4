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

#include "pat.h"
#include "log.h"
#include <string.h>

#ifndef MAX
#define MAX(a, b) (((a) > (b))?((a)):((b)))
#endif


/**
 *
 * @defgroup PatriciaTrie PatriciaTrie
 * @ingroup Homegrown
 * @brief A patricia trie implementation
 *
 * @{
 */

#define PAT_LEAF 0xf007ba11
#define PAT_INT  0xdaceca5e
#define PAT_STR  0xfab1ed00


/* The structure used for the patricie trie nodes */
typedef struct pat_node_St {
	int32_t magic;
	union {
		struct {
			uint32_t pos;
			int32_t left, right;
		} internal;
		struct {
			uint32_t len;
			int32_t key;
			int32_t first_key;
			int32_t key_count;
		} leaf;
		struct {
			uint32_t len;
			int32_t next, prev;
			int32_t parent;
			char str[0];
		} str;
	} u;
} pat_node_t;



static int get_root (s4be_t *s4, int32_t t)
{
	pat_trie_t *trie = S4_PNT(s4, t, pat_trie_t);

	return trie->root;
}

static void set_root (s4be_t *s4, int32_t t,  int32_t root)
{
	pat_trie_t *trie = S4_PNT(s4, t, pat_trie_t);

	trie->root = root;
}


static inline int is_leaf (pat_node_t *pn)
{
	return pn->magic == PAT_LEAF;
}


/* Check if a bit is set in the string key.
 * Return 0 if it is not, non-0 if it is.
 */
static inline int bit_set (pat_key_t *key, int bit)
{
	int i = bit >> 3;
	int j = bit & 7;
	char c = ((char*)key->common_key)[i];
	return (c >> j) & 1;
}


/* Find the bit position of the first bit that is different
 * between the key and the node.
 * Returns -1 when there is no difference.
 */
static inline int string_diff (s4be_t *s4, pat_key_t *key, int32_t node)
{
	int i, diff, ret;
	const char *sa = key->common_key;
	int lena = key->common_keylen;
	pat_node_t *pn = S4_PNT(s4, node, pat_node_t);
	const char *sb = S4_PNT(s4, pn->u.leaf.key, char);
	int lenb = pn->u.leaf.len;

	for (i = 0, diff = 0; i*8 < lena && i*8 < lenb; i++) {
		diff = sa[i] ^ sb[i];
		if (diff) break;
	}

	for (ret = i * 8; diff && !(diff & 1); diff >>= 1, ret++);

	if (ret >= lena && lena == lenb) {
		ret = (lena == lenb)?-1:MAX(lena, lenb);
	}

	return ret;
}


static inline int nodes_equal (s4be_t *s4, pat_key_t *key, int32_t node)
{
	return node != -1 && string_diff(s4, key, node) == -1;
}


/* Finds the next node to go to and saves it in node
 * If the current node is a leaf the function returns 0,
 * otherwise it returns 1;
 */
static inline int get_next (s4be_t *s4, pat_key_t *key, int32_t *node)
{
	pat_node_t *pn;
	pn = S4_PNT(s4, *node, pat_node_t);

	if (is_leaf (pn))
		return 0;

	if (pn->u.internal.pos < key->common_keylen) {
		if (bit_set(key, pn->u.internal.pos)) {
			*node = pn->u.internal.right;
		} else {
			*node = pn->u.internal.left;
		}
	} else {
		*node = pn->u.internal.left;
	}

	return 1;
}


/* Walk the trie using the key as direction.
 * Returns the leaf node it finds (-1 if the trie is empty)
 */
static int32_t trie_walk (s4be_t *s4, int32_t trie, pat_key_t *key)
{
	int32_t node = get_root(s4, trie);

	while (node != -1 && get_next (s4, key, &node));

	return node;
}


/* Insert an internal node at the position pos */
static void insert_internal (s4be_t *s4, int32_t trie, pat_key_t *key,
		int pos, int32_t node)
{
	int32_t internal = be_alloc (s4, sizeof(pat_node_t));
	int32_t prev = -1;
	int32_t cur = get_root(s4, trie);
	pat_node_t *pn = S4_PNT(s4, cur, pat_node_t);

	/* Look for the internal node before the new one */
	while (!is_leaf(pn) && pn->u.internal.pos < pos) {
		prev = cur;
		cur = (bit_set (key, pn->u.internal.pos))?
			pn->u.internal.right:pn->u.internal.left;
		pn = S4_PNT(s4, cur, pat_node_t);
	}

	/* Insert the internal node */
	if (prev == -1) {
		set_root(s4, trie, internal);
		prev = cur;
	} else {
		pn = S4_PNT(s4, prev, pat_node_t);
		if (bit_set (key, pn->u.internal.pos)) {
			prev = pn->u.internal.right;
			pn->u.internal.right = internal;
		} else {
			prev = pn->u.internal.left;
			pn->u.internal.left = internal;
		}
	}

	/* Add the leaf to the internal node */
	pn = S4_PNT(s4, internal, pat_node_t);
	pn->u.internal.pos = pos;
	pn->magic = PAT_INT;
	if (bit_set (key, pos)) {
		pn->u.internal.right = node;
		pn->u.internal.left = prev;
	} else {
		pn->u.internal.left = node;
		pn->u.internal.right = prev;
	}
}


/* Look for the node containing key among ileaf's children */
static int32_t find_str (s4be_t *s4, pat_key_t *key, int32_t ileaf)
{
	pat_node_t *leaf = S4_PNT (s4, ileaf, pat_node_t);
	pat_node_t *child = S4_PNT (s4, leaf->u.leaf.first_key, pat_node_t);
	int32_t node = leaf->u.leaf.first_key;
	int i;

	for (i = 0; i < leaf->u.leaf.key_count; i++) {
		if (key->unique_keylen == child->u.str.len &&
				!strncmp (key->unique_key + key->unique_keyoff,
					child->u.str.str + key->unique_keyoff,
					key->unique_keylen - key->unique_keyoff)) {
			return node;
		}

		node = child->u.str.next;
		child = S4_PNT (s4, node, pat_node_t);
	}

	return -1;
}

/*
 * Add a new string to a leaf
 *
 * Returns the new node, or -1 if it already exists
 */
static int32_t insert_str (s4be_t *s4, pat_key_t *key, int32_t trie, int32_t ileaf)
{
	pat_trie_t *ptrie;
	pat_node_t *leaf;
	pat_node_t *child;
	pat_node_t *pn;
	int32_t node;

	if (find_str (s4, key, ileaf) != -1)
		return -1;

	/* It does not exist yet, we have to create it */
	node = be_alloc (s4, sizeof (pat_node_t) + key->unique_keylen);
	leaf = S4_PNT (s4, ileaf, pat_node_t);
	child = S4_PNT (s4, leaf->u.leaf.first_key, pat_node_t);
	pn = S4_PNT (s4, node, pat_node_t);
	ptrie = S4_PNT(s4, trie, pat_trie_t);

	/* Initialize the new node */
	pn->magic = PAT_STR;
	pn->u.str.parent = ileaf;
	pn->u.str.len = key->unique_keylen;
	memcpy (pn->u.str.str, key->unique_key, key->unique_keylen);

	/* Insert the node into the linked list */
	if (leaf->u.leaf.key_count > 0) {
		pn->u.str.prev = child->u.str.prev;
		pn->u.str.next = leaf->u.leaf.first_key;

		child->u.str.prev = node;
	} else {
		pn->u.str.prev = -1;
		pn->u.str.next = ptrie->list_start;

		if (ptrie->list_start != -1) {
			pat_node_t *next = S4_PNT (s4, ptrie->list_start, pat_node_t);
			next->u.str.prev = node;
		}
	}

	/* If we are at the start of the list we must update the list pointer */
	if (pn->u.str.prev == -1) {
		ptrie->list_start = node;
	}

	leaf->u.leaf.first_key = node;
	leaf->u.leaf.key_count++;

	return node;
}

/*
 * Remove a string entry from a leaf
 *
 * Returns 0 if everything went okay, -1 if the string is not found
 */
static int remove_str (s4be_t *s4, pat_key_t *key, int32_t trie, int32_t ileaf)
{
	pat_trie_t *ptrie = S4_PNT (s4, trie, pat_trie_t);
	pat_node_t *leaf = S4_PNT (s4, ileaf, pat_node_t);
	pat_node_t *child = S4_PNT (s4, leaf->u.leaf.first_key, pat_node_t);
	int32_t node = leaf->u.leaf.first_key;

	if ((node = find_str (s4, key, ileaf)) == -1)
		return -1;

	child = S4_PNT (s4, node, pat_node_t);
	leaf->u.leaf.key_count--;

	if (leaf->u.leaf.first_key == node) {
		leaf->u.leaf.first_key = child->u.str.next;
	}
	if (child->u.str.prev == -1) {
		ptrie->list_start = child->u.str.next;
	} else {
		pat_node_t *prev = S4_PNT (s4, child->u.str.prev, pat_node_t);
		prev->u.str.next = child->u.str.next;
	}
	if (child->u.str.next != -1) {
		pat_node_t *next = S4_PNT (s4, child->u.str.next, pat_node_t);
		next->u.str.prev = child->u.str.prev;
	}

	be_free (s4, node, sizeof (pat_node_t) + child->u.str.len);

	return 0;
}


/**
 * Lookup the key in the trie
 *
 * @param s4 Database handle
 * @param trie The offset of the trie into the database
 * @param key The key to lookup
 * @return The node, or -1 if it is not found
 */
int32_t pat_lookup (s4be_t *s4, int32_t trie, pat_key_t *key)
{
	int32_t node = trie_walk (s4, trie, key);

	/* We first find the leaf that might contain this string entry */
	if (!nodes_equal (s4, key, node))
		return -1;

	/* Then we look for the string in the leaf */
	return find_str (s4, key, node);
}


/**
 * Find the parent of a string entry
 *
 * @param s4 Database handle
 * @param trie The offset of the trie into the database
 * @param key The key to lookup
 * @return The node, or -1 if it is not found
 */
int32_t pat_lookup_parent (s4be_t *s4, int32_t trie, pat_key_t *key)
{
	int32_t node = trie_walk (s4, trie, key);

	/* Look for the leaf that contains the string */
	if (!nodes_equal (s4, key, node))
		return -1;

	return node;
}


/**
 * Insert something into the trie
 *
 * @param s4 Handle for the database
 * @param trie The offset of the trie into the database
 * @param key_s The key to insert
 * @return The new node, or -1 if it already exists
 */
int32_t pat_insert (s4be_t *s4, int32_t trie, pat_key_t *key_s)
{
	int32_t key, node, comp, strnode;
	int diff;
	pat_node_t *pn;

	/* Check if the node already exist */
	comp = trie_walk (s4, trie, key_s);
	if (nodes_equal (s4, key_s, comp)) {
		/* Insert the string into the already existing leaf */
		return insert_str (s4, key_s, trie, comp);
	}

	/* Copy the key into the database */
	key = be_alloc (s4, (key_s->common_keylen + 7) / 8);
	memcpy (S4_PNT(s4, key, char), key_s->common_key, (key_s->common_keylen + 7) / 8);

	/* Allocate and setup the node */
	node = be_alloc (s4, sizeof(pat_node_t));

	pn = S4_PNT(s4, node, pat_node_t);
	pn->u.leaf.key = key;
	pn->u.leaf.len = key_s->common_keylen;
	pn->u.leaf.key_count = 0;
	pn->u.leaf.first_key = -1;
	pn->magic = PAT_LEAF;

	/* Insert the string into the new leaf */
	strnode = insert_str (s4, key_s, trie, node);

	/* If there is no root, we are the root */
	if (comp == -1) {
		set_root(s4, trie, node);
		return strnode;
	}

	diff = string_diff(s4, key_s, comp);
	insert_internal (s4, trie, key_s, diff, node);

	return strnode;
}


/**
 * Remove an entry from the trie
 *
 * @param s4 The database handle
 * @param trie The trie to remove it from
 * @param key The key to remove
 * @return -1 on error, 0 on success
 */
int pat_remove (s4be_t *s4, int32_t trie, pat_key_t *key)
{
	int32_t node, prev, pprev, sibling, tmp;
	pat_node_t *pn;

	prev = pprev = -1;
	tmp = node = get_root(s4, trie);

	/* Walk the trie and find the correct leaf, the node before the leaf
	 * and the node before that one (if they exist)
	 */
	while (node != -1 && get_next (s4, key, &tmp))
	{
		pprev = prev;
		prev = node;
		node = tmp;
	}

	/* Check if this is the right node and try to remove the string entry */
	if (node == -1 || !nodes_equal (s4, key, node) || (remove_str (s4, key, trie, node) != 0))
		return -1;

	pn = S4_PNT (s4, node, pat_node_t);

	/* If we still have string entries in this leaf we do not delete it */
	if (pn->u.leaf.key_count > 0)
		return 0;

	be_free (s4, pn->u.leaf.key, (pn->u.leaf.len + 7) / 8);
	be_free (s4, node, sizeof (pat_node_t));

	/* Find the sibling and free the previous internal node */
	if (prev == -1) {
		sibling = -1;
	} else {
		pn = S4_PNT(s4, prev, pat_node_t);
		sibling = ((pn->u.internal.left == node)?pn->u.internal.right:pn->u.internal.left);
		be_free (s4, prev, sizeof (pat_node_t));
	}

	/* Update the node before the internal node we deleted */
	if (pprev == -1) {
		set_root(s4, trie, sibling);
	} else {
		pn = S4_PNT(s4, pprev, pat_node_t);
		if (pn->u.internal.left == prev) {
			pn->u.internal.left = sibling;
		} else {
			pn->u.internal.right = sibling;
		}
	}

	return 0;
}


/**
 * Find the key for the leaf
 *
 * @param s4 The database handle
 * @param node The leaf
 * @return The key
 */
int32_t pat_node_to_key (s4be_t *s4, int32_t node)
{
	pat_node_t *pn = S4_PNT(s4, node, pat_node_t);

	if (pn->magic != PAT_LEAF)
		return -1;

	return pn->u.leaf.key;
}

/**
 * Find the leaf containing this string node
 *
 * @param s4 The database handle
 * @param node The node we want to find the parent of
 * @return The parent of the string node
 */
int32_t pat_parent (s4be_t *s4, int32_t node)
{
	pat_node_t *pn = S4_PNT(s4, node, pat_node_t);

	if (pn->magic != PAT_STR)
		return -1;

	return pn->u.str.parent;
}

/**
 * Get the string for the string node
 *
 * @param s4 The database handle
 * @param node The node we want to get the string to
 * @return The string. This is the actual string in the nod,
 * changes to this string will result in changes in the actual string.
 */
char *pat_node_to_str (s4be_t *s4, int32_t node)
{
	pat_node_t *pn = S4_PNT (s4, node, pat_node_t);

	if (pn->magic != PAT_STR)
		return NULL;

	return pn->u.str.str;
}

/**
 * Get the number of keys the leaf holds
 *
 * @param s4 The database handle
 * @param parent The leaf to get the key count for
 * @return The key count, or 0 if this is not a leaf
 */
int32_t pat_parent_key_count (s4be_t *s4, int32_t parent)
{
	pat_node_t *pn = S4_PNT (s4, parent, pat_node_t);

	if (pn->magic != PAT_LEAF)
		return 0;

	return pn->u.leaf.key_count;
}

/**
 * Get the first key of a leaf
 *
 * @param s4 The database handle
 * @param parent The leaf to find the key of
 * @return The first key for the leaf, or -1 if it is not a leaf
 */
int32_t pat_parent_first_key (s4be_t *s4, int32_t parent)
{
	pat_node_t *pn = S4_PNT (s4, parent, pat_node_t);

	if (pn->magic != PAT_LEAF)
		return -1;

	return pn->u.leaf.first_key;
}

/**
 * Return the first node in the trie (not sorted)
 *
 * @param s4 The database handle
 * @param trie The trie
 * @return The first node
 */
int32_t pat_first (s4be_t *s4, int32_t trie)
{
	pat_trie_t *ptrie = S4_PNT (s4, trie, pat_trie_t);

	return ptrie->list_start;
}


/**
 * Return the node after this one
 *
 * @param s4 The database handle
 * @param trie The trie the node is in
 * @param node The node to find the next one of
 * @return The node after node.
 */
int32_t pat_next (s4be_t *s4, int32_t trie, int32_t node)
{
	pat_node_t *pnode = S4_PNT(s4, node, pat_node_t);

	if (node == -1 || pnode->magic != PAT_STR)
		return -1;

	return pnode->u.str.next;
}

struct verify_info {
	int node_outside;
	int key_outside;
	int key_error;
	int keycount_error;
	int str_outside;
	int magic_error;
};

static int verify_helper (s4be_t *be, int32_t trie, int32_t node,
		struct verify_info *info)
{
	pat_node_t *pnode = S4_PNT (be, node, pat_node_t);
	int ret = 0;

	if (node < 0 || node > (be->size - sizeof (pat_node_t))) {
		info->node_outside++;
	} else if (pnode->magic == PAT_INT) {
		ret = verify_helper (be, trie, pnode->u.internal.left, info) &
			verify_helper (be, trie, pnode->u.internal.right, info);
	} else if (pnode->magic == PAT_LEAF) {
		if (pnode->u.leaf.key < 0 ||
				pnode->u.leaf.key > (be->size - (pnode->u.leaf.len + 7) / 8)) {
			info->key_outside++;
		} else {
			pat_key_t key;
			key.common_key = S4_PNT (be, pnode->u.leaf.key, void);
			key.common_keylen = pnode->u.leaf.len;

			int32_t tw = trie_walk (be, trie, &key);

			if (tw != node) {
				info->key_error++;
			} else if (pnode->u.leaf.key_count < 1) {
				info->keycount_error++;
			} else {
				int i = 0;
				int32_t str = pnode->u.leaf.first_key;
				int count = pnode->u.leaf.key_count;
				ret = 1;
				for (i = 0; i < count && ret; i++) {
					ret = verify_helper (be, trie, str, info);

					if (ret) {
						pnode = S4_PNT (be, str, pat_node_t);
						str = pnode->u.str.next;
					}
				}
			}
		}
	} else if (pnode->magic == PAT_STR) {
		if (node > (be->size - sizeof (pat_node_t) - pnode->u.str.len)) {
			info->str_outside++;
		} else {
			ret = 1;
		}
	} else {
		info->magic_error++;
	}

	return ret;
}

/**
 * Check if the patricia trie is consistent
 *
 * @param be The database to check
 * @param trie The trie to check
 * @return 1 if everything's good, 0 otherwise
 *
 */
int pat_verify (s4be_t *be, int32_t trie)
{
	int32_t node = get_root (be, trie);
	struct verify_info info;
	int ret = 1;

	memset (&info, 0, sizeof (struct verify_info));

	if (node != -1) {
		ret = verify_helper (be, trie, node, &info);

		if (ret == 0) {
			S4_ERROR ("Patricia trie inconsistent!");
			S4_ERROR ("%i pointers pointing outside", info.node_outside);
			S4_ERROR ("%i keys pointing outside ", info.key_outside);
			S4_ERROR ("%i keys not leading to the node", info.key_error);
			S4_ERROR ("%i leaf with incorrect key_count", info.keycount_error);
			S4_ERROR ("%i strings pointing outside ", info.str_outside);
			S4_ERROR ("%i nodes with wrong magic number", info.magic_error);
		}
	}

	return ret;
}

/**
 * Try to recover as many leafs as possible. It does this by looking for
 * the magic number for patricia leafs.
 *
 * @param be The database to recover
 * @param func The function to call with the found nodes
 * @param u The userdata to pass to the function
 *
 */
void pat_recover (s4be_t *be, void (*func) (int32_t, void*), void *u)
{
	int32_t *p = S4_PNT (be, 0, int32_t);
	int i;

	for (i = 0; i < (be->size / sizeof (int32_t)); i++) {
		if (p[i] == PAT_STR) {
			func (i * sizeof (int32_t), u);
		}
	}
}

/**
 * @}
 */
