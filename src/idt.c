#include "idt.h"
#include <stdlib.h>
#include <limits.h>

#define LEVELS 4
#define BITS_PER_LEVEL (IDT_TYPE_BITS / LEVELS)
#define ENTRIES_PER_LEVEL (1 << BITS_PER_LEVEL)
#define LEVEL_MASK (ENTRIES_PER_LEVEL - 1)
#define BITMAP_SIZE (ENTRIES_PER_LEVEL / 8 / sizeof (unsigned long))

typedef struct idt_node_St {
	long free;
	int level;
	IDT_TYPE first;
	struct idt_node_St *parent;

	unsigned long bitmap[BITMAP_SIZE];
	union {
		struct idt_node_St *children[ENTRIES_PER_LEVEL];
		void *data[ENTRIES_PER_LEVEL];
	} u;
} idt_node_t;

struct idt_St {
	idt_node_t *root;
};


static void _idt_destroy_helper (idt_node_t *node)
{
	if (node->level > 0) {
		int i;
		for (i = 0; i < ENTRIES_PER_LEVEL; i++) {
			if (node->u.children[i] != NULL)
				_idt_destroy_helper (node->u.children[i]);
		}
	}

	free (node);
}

static int _idt_match (idt_node_t *n, IDT_TYPE b)
{
	int mask = ~LEVEL_MASK << (n->level * BITS_PER_LEVEL);

	return (n->first & mask) == (b & mask);
}

static void _idt_set_bit (idt_node_t *node, int bit)
{
	unsigned long i = bit / (sizeof (unsigned long) * 8);
	unsigned long j = bit % (sizeof (unsigned long) * 8);

	node->bitmap[i] |= (unsigned long)1 << j;
}

static int _idt_find_free (idt_node_t *node)
{
	int i, j;
	unsigned long k;

	for (i = 0; i < BITMAP_SIZE && node->bitmap[i] == ULONG_MAX; i++);
	for (j = 0, k = node->bitmap[i]; j < (sizeof (unsigned long) * 8) && k&1; j++, k >>= 1);

	return i * sizeof (unsigned long) * 8 + j;
}


static int _idt_get_index (IDT_TYPE id, int level)
{
	return (id >> (level * BITS_PER_LEVEL)) & LEVEL_MASK;
}

static idt_node_t *_idt_create_node (idt_node_t *child)
{
	idt_node_t *node = calloc (1, sizeof (idt_node_t));
	int index;

	if (child == NULL) {
		node->free = ENTRIES_PER_LEVEL;
	} else {
		node->level = child->level + 1;
		node->free = ENTRIES_PER_LEVEL << (node->level * BITS_PER_LEVEL);
		node->first = child->first & (~LEVEL_MASK << ((node->level) * BITS_PER_LEVEL));

		node->free -= (ENTRIES_PER_LEVEL << (child->level * BITS_PER_LEVEL)) - child->free;

		index = _idt_get_index (child->first, child->level + 1);
		node->u.children[index] = child;
		_idt_set_bit (node, index);
	}

	return node;
}
static idt_node_t *_idt_create_child (idt_node_t *node, int index)
{
	idt_node_t *child = calloc (1, sizeof (idt_node_t));

	child->parent = node;
	child->level = node->level - 1;
	child->free = ENTRIES_PER_LEVEL << (child->level * BITS_PER_LEVEL);
	child->first = node->first | (index << (node->level * BITS_PER_LEVEL));

	node->u.children[index] = child;

	return child;
}


/**
 * Create a new empty id-tree
 *
 * @return A new id-tree
 */
idt_t *idt_create (void)
{
	idt_t *ret = malloc (sizeof (idt_t));
	ret->root = NULL;

	return ret;
}

/**
 * Destroy an id-tree and all its nodes
 *
 * @param idt The id-tree to destroy
 */
void idt_destroy (idt_t *idt)
{
	if (idt->root != NULL) {
		_idt_destroy_helper (idt->root);
	}

	free (idt);
}

/**
 * Insert new data in the tree, giving it a new unique id
 *
 * @param idt The id-tree to insert into
 * @param data The data to insert
 * @return A new unique id associated with the data or -1 if the tree is full
 */
IDT_TYPE idt_insert (idt_t *idt, void *data)
{
	idt_node_t *node = idt->root;
	IDT_TYPE i = -1;

	if (node == NULL || node->free == 0) {
		if (node != NULL && node->level == (LEVELS - 1))
			return -1;

		node = idt->root = _idt_create_node (node);
	}

	while (1) {
		node->free--;

		if (node->free == 0 && i != -1) {
			_idt_set_bit (node->parent, i);
		}

		i = _idt_find_free (node);

		if (node->level > 0) {
			if (node->u.children[i] == NULL) {
				node = _idt_create_child (node, i);
			} else {
				node = node->u.children[i];
			}
		} else {
			node->u.data[i] = data;
			_idt_set_bit (node, i);
			break;
		}
	}

	return i | node->first;
}

/**
 * Insert data with a given id into the id-tree. If the id
 * is already in use the data will be replaced with the new data.
 *
 * @param idt The id-tree to insert into
 * @param id The id to associate the new_data with
 * @param new_data The data to insert
 * @return the old data if something was replaced, NULL otherwise
 */
void *idt_replace (idt_t *idt, IDT_TYPE id, void *new_data)
{
	idt_node_t *node = idt->root;
	void *old_data = NULL;
	IDT_TYPE i;

	while (node == NULL || !_idt_match (node, id)) {
		node = _idt_create_node (node);
		if (idt->root == NULL) {
			node->first = id & ~LEVEL_MASK;
		}
		idt->root = node;
	}

	while (node->level > 0) {
		i = _idt_get_index (id, node->level);
		if (node->u.children[i] == NULL) {
			node = _idt_create_child (node, i);
		} else {
			node = node->u.children[i];
		}
	}

	i = _idt_get_index (id, 0);
	old_data = node->u.data[i];
	node->u.data[i] = new_data;

	while (old_data == NULL && node != NULL) {
		node->free--;
		_idt_set_bit (node, _idt_get_index (id, node->level));
		node = node->parent;
	}

	return old_data;
}

/**
 * Get the data associated with an id
 *
 * @param idt The id-tree to search in
 * @param id The id to lookup
 * @return The data associated with id, or NULL if there is none
 */
void *idt_get (idt_t *idt, IDT_TYPE id)
{
	idt_node_t *node = idt->root;

	while (node != NULL && node->level > 0) {
		node = node->u.children[_idt_get_index (id, node->level)];
	}

	if (node == NULL || !_idt_match (node, id))
		return NULL;

	return node->u.data[_idt_get_index (id, 0)];
}
