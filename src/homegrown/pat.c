#include "pat.h"
#include <string.h>

#define MAX(a, b) (((a) > (b))?((a)):((b)))


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


/* The structure used for the patricie trie nodes */
typedef union pat_node_St {
	int32_t magic;
	struct {
		uint32_t pos;
		int32_t left, right;
	} internal;
	struct {
		uint32_t len;
		int32_t key;
		int32_t data_len;
	} leaf;
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

/*
static void add_to_list (s4be_t *s4, int32_t trie, int32_t node)
{
	pat_trie_t *ptrie = S4_PNT(s4, trie, pat_trie_t);
	pat_node_t *pnode = S4_PNT(s4, node, pat_node_t);
	pat_node_t *end = S4_PNT(s4, ptrie->list_end, pat_node_t);

	if (ptrie->list_end == -1) {
		ptrie->list_end = ptrie->list_start = node;
		pnode->leaf.next = pnode->leaf.prev = -1;
	} else {
		pnode->leaf.prev = ptrie->list_end;
		pnode->leaf.next = -1;
		end->leaf.next = node;
		ptrie->list_end = node;
	}
}

static void del_from_list (s4be_t *s4, int32_t trie, int32_t node)
{
	pat_trie_t *ptrie = S4_PNT(s4, trie, pat_trie_t);
	pat_node_t *pnode = S4_PNT(s4, node, pat_node_t);
	pat_node_t *tmp;

	if (pnode->leaf.prev != -1) {
		tmp = S4_PNT(s4, pnode->leaf.prev, pat_node_t);
		tmp->leaf.next = pnode->leaf.next;
	} else {
		ptrie->list_start = pnode->leaf.next;
	}

	if (pnode->leaf.next != -1) {
		tmp = S4_PNT(s4, pnode->leaf.next, pat_node_t);
		tmp->leaf.prev = pnode->leaf.prev;
	} else {
		ptrie->list_end = pnode->leaf.prev;
	}
}
*/

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
	char c = ((char*)key->data)[i];
	return (c >> j) & 1;
}


/* Find the bit position of the first bit that is different
 * between the key and the node.
 * Returns -1 when there is no difference.
 */
static inline int string_diff (s4be_t *s4, pat_key_t *key, int32_t node)
{
	int i, diff, ret;
	const char *sa = key->data;
	int lena = key->key_len;
	pat_node_t *pn = S4_PNT(s4, node, pat_node_t);
	const char *sb = S4_PNT(s4, pn->leaf.key, char);
	int lenb = pn->leaf.len;

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

	if (pn->internal.pos < key->key_len) {
		if (bit_set(key, pn->internal.pos)) {
			*node = pn->internal.right;
		} else {
			*node = pn->internal.left;
		}
	} else {
		*node = pn->internal.left;
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
	while (!is_leaf(pn) && pn->internal.pos < pos) {
		prev = cur;
		cur = (bit_set (key, pn->internal.pos))?
			pn->internal.right:pn->internal.left;
		pn = S4_PNT(s4, cur, pat_node_t);
	}

	/* Insert the internal node */
	if (prev == -1) {
		set_root(s4, trie, internal);
		prev = cur;
	} else {
		pn = S4_PNT(s4, prev, pat_node_t);
		if (bit_set (key, pn->internal.pos)) {
			prev = pn->internal.right;
			pn->internal.right = internal;
		} else {
			prev = pn->internal.left;
			pn->internal.left = internal;
		}
	}

	/* Add the leaf to the internal node */
	pn = S4_PNT(s4, internal, pat_node_t);
	pn->internal.pos = pos;
	pn->magic = PAT_INT;
	if (bit_set (key, pos)) {
		pn->internal.right = node;
		pn->internal.left = prev;
	} else {
		pn->internal.left = node;
		pn->internal.right = prev;
	}
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
	int32_t key, node, comp;
	int diff;
	pat_node_t *pn;

	/* Check if the node already exist */
	comp = trie_walk (s4, trie, key_s);
	if (nodes_equal (s4, key_s, comp)) {
		return -1;
	}

	/* Copy the key into the database */
	key = be_alloc (s4, key_s->data_len);
	memcpy (S4_PNT(s4, key, char), key_s->data, key_s->data_len);

	/* Allocate and setup the node */
	node = be_alloc (s4, sizeof(pat_node_t));

	pn = S4_PNT(s4, node, pat_node_t);
	pn->leaf.key = key;
	pn->leaf.len = key_s->key_len;
	pn->leaf.data_len = key_s->data_len;
	pn->magic = PAT_LEAF;

	/* If there is no root, we are the root */
	if (comp == -1) {
		set_root(s4, trie, node);
		return node;
	}

	diff = string_diff(s4, key_s, comp);
	insert_internal (s4, trie, key_s, diff, node);

	return node;
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

	while (node != -1 && get_next (s4, key, &tmp))
	{
		pprev = prev;
		prev = node;
		node = tmp;
	}

	/* Check if this is the right node */
	if (node == -1 || !nodes_equal (s4, key, node)) {
		return -1;
	}

	pn = S4_PNT (s4, node, pat_node_t);
	be_free (s4, pn->leaf.key, pn->leaf.data_len);
	be_free (s4, node, sizeof (pat_node_t));

	if (prev == -1) {
		sibling = -1;
	} else {
		pn = S4_PNT(s4, prev, pat_node_t);
		sibling = ((pn->internal.left == node)?pn->internal.right:pn->internal.left);
		be_free (s4, prev, sizeof (pat_node_t));
	}

	if (pprev == -1) {
		set_root(s4, trie, sibling);
	} else {
		pn = S4_PNT(s4, pprev, pat_node_t);
		if (pn->internal.left == prev) {
			pn->internal.left = sibling;
		} else {
			pn->internal.right = sibling;
		}
	}

	return 0;
}


/**
 * Find the key for the node
 *
 * @param s4 The database handle
 * @param node The node
 * @return The key
 */
int32_t pat_node_to_key (s4be_t *s4, int32_t node)
{
	pat_node_t *pn = S4_PNT(s4, node, pat_node_t);

	return pn->leaf.key;
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
	int32_t cur = get_root (s4, trie);
	pat_node_t *pnode = S4_PNT (s4, cur, pat_node_t);

	while (cur != -1 && pnode->magic == PAT_INT) {
		cur = pnode->internal.left;
		pnode = S4_PNT (s4, cur, pat_node_t);
	}

	return cur;
}


/**
 * Return the node after this one
 *
 * @param s4 The database handle
 * @param node The node to find the next one of
 * @return The node after node.
 */
int32_t pat_next (s4be_t *s4, int32_t trie, int32_t node)
{
	pat_node_t *pnode = S4_PNT(s4, node, pat_node_t);
	pat_node_t *plast = NULL;
	pat_key_t key;
	int32_t last = -1;

	if (node == -1)
		return -1;

	key.data = S4_PNT (s4, pnode->leaf.key, void*);
	key.key_len = pnode->leaf.len;

	node = get_root(s4, trie);
	pnode = S4_PNT (s4, node, pat_node_t);

	while (node != -1 && pnode->magic == PAT_INT) {
		if (bit_set (&key, pnode->internal.pos)) {
			node = pnode->internal.right;
		} else {
			last = node;
			node = pnode->internal.left;
		}

		pnode = S4_PNT (s4, node, pat_node_t);
	}

	while (plast != NULL && plast->magic == PAT_INT) {
		last = plast->internal.left;
		plast = S4_PNT (s4, last, pat_node_t);
	}

	return last;
}

int pat_verify (s4be_t *be, int32_t trie)
{
}

/**
 * @}
 */
