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

#include <stdlib.h>
#include <string.h>
#include <s4.h>
#include "log.h"


/**
 *
 * @defgroup Set Set
 * @ingroup S4
 * @brief A set with intersection and union
 *
 * @{
 */

#ifndef MIN
#define MIN(a, b) (((a) < (b))?(a):(b))
#endif

struct s4_set_St {
	int size;
	int alloc;
	int pos;
	s4_entry_t *entries;
};

/* Return the first power of two bigger or equal to i */
static int align2 (int i)
{
	int ret = 1;

	while (i > (ret <<= 1));

	return ret;
}

/* Compare to entries, return <0 if a<b, 0 if a=b and >0 if a>b */
static int comp (s4_entry_t *a, s4_entry_t *b)
{
	int ret = 0;

	ret = (a->key_i < b->key_i)?-1:(a->key_i > b->key_i);
	if (!ret)
		ret = (a->val_i < b->val_i)?-1:(a->val_i > b->val_i);

	return ret;
}

/* Find the first entry equal to or bigger than entry */
static int search (s4_set_t *set, s4_entry_t *entry, int *equal)
{
	int lower, upper;
	lower = 0;
	upper = set->size;

	while (lower != upper) {
		int gap = (upper - lower) / 2;
		int c = comp (entry, set->entries + lower + gap);

		if (c < 0) {
			upper = lower + gap;
		} else if (c > 0) {
			lower += gap + 1;
		} else {
			*equal = 1;
			return lower + gap;
		}
	}

	*equal = 0;
	return upper;
}

/* Expand the set to make space for twice as many entries */
static void expand (s4_set_t *set)
{
	set->alloc <<= 1;

	set->entries = realloc (set->entries, set->alloc * sizeof (s4_entry_t));
}

/* Makes a copy of set */
static s4_set_t *copy_set (s4_set_t *set)
{
	s4_set_t *ret = malloc (sizeof (s4_set_t));

	ret->size = set->size;
	ret->alloc = align2 (ret->size);
	ret->entries = malloc (sizeof (s4_entry_t) * ret->alloc);
	ret->pos = 0;

	memcpy (ret->entries, set->entries, sizeof (s4_entry_t) * ret->size);

	return ret;
}

/**
 * Create a new set.
 *
 * @param size The size of the new set. Setting this to something other than 0
 * will create a set with space enough for atleast size entries. This might
 * save some reallocs.
 * @return A new set. It must be freed with ::s4_set_free when you're done with it.
 *
 */
s4_set_t *s4_set_new (int size)
{
	s4_set_t *set;

	if (size == 0)
		size = 8;
	else
		size = align2 (size);

	set = malloc (sizeof (s4_set_t));
	set->entries = malloc (sizeof (s4_entry_t) * size);
	set->alloc = size;
	set->size = 0;
	set->pos = 0;

	return set;
}

/**
 * Free the given set.
 *
 * @param set The set to free.
 */
void s4_set_free (s4_set_t *set)
{
	int i;

	if (set == NULL) {
		return;
	}

	for (i = 0; i < set->size; i++) {
		s4_entry_free_strings (set->entries + i);
	}

	free (set->entries);
	free (set);
}

/**
 * Return the size of the set.
 *
 * @param set The set to find the size of.
 * @return The number of entries in the set.
 */
int s4_set_size (s4_set_t *set)
{
	if (set == NULL)
		return 0;

	return set->size;
}

/**
 * Find the intersection of the two sets.
 *
 * @param a One of the sets.
 * @param b The other set.
 * @return The intersection of a and b. Must be freed with ::s4_set_free.
 *
 */
s4_set_t *s4_set_intersection (s4_set_t *a, s4_set_t *b)
{
	s4_set_t *min, *max, *ret;
	int i, j, equal, size;

	if (a == NULL || b == NULL)
		return NULL;

	size = MIN (a->size, b->size);
	min = a->size == size?a:b;
	max = a->size == size?b:a;
	ret = s4_set_new (size);

	for (i = j = 0; i < size; i++) {
		search (max, min->entries + i, &equal);

		if (equal)
			ret->entries[j++] = min->entries[i];
	}

	if (j == 0) {
		s4_set_free (ret);
		ret = NULL;
	} else {
		ret->size = j;
	}

	return ret;
}

/**
 * Find the union of two sets.
 *
 * @param a One of the sets.
 * @param b The other set.
 * @return The union of a and b. Must be freed with ::s4_set_free.
 *
 */
s4_set_t *s4_set_union (s4_set_t *a, s4_set_t *b)
{
	s4_set_t *ret;
	int i, j, k, size;

	i = s4_set_size (a);
	j = s4_set_size (b);

	if (i == 0 && j == 0) {
		return NULL;
	} else if (i == 0) {
		return copy_set (b);
	} else if (j == 0) {
		return copy_set (a);
	}

	size = a->size + b->size;
	ret = s4_set_new (size);

	for (i = j = k = 0; i < a->size || j < b->size;) {
		int c = (j >= b->size)?-1:
			((i >= a->size)?1:
			 comp (a->entries + i, b->entries + j));
		if (c < 0) {
			ret->entries[k++] = a->entries[i++];
		} else if (c > 0) {
			ret->entries[k++] = b->entries[j++];
		} else {
			ret->entries[k++] = a->entries[i++];
			j++;
		}
	}

	ret->size = k;

	return ret;
}

/**
 * Find the relative complement of a in b (b - a)
 *
 * @param a Set A
 * @param a Set B
 * @return The relative complement of A in B (B \ A). The returned set mus
 * be freed with ::s4_set_free.
 *
 */
s4_set_t *s4_set_complement (s4_set_t *a, s4_set_t *b)
{
	s4_set_t *ret;
	int i, j, k;

	i = s4_set_size (a);
	j = s4_set_size (b);

	if (j == 0) {
		return NULL;
	} else if (i == 0) {
		return copy_set (b);
	}

	ret = s4_set_new (j);

	for (i = j = k = 0; j < b->size;) {
		int c =	(i >= a->size)?1:
			 comp (a->entries + i, b->entries + j);
		if (c < 0) {
			i++;
		} else if (c > 0) {
			ret->entries[k++] = b->entries[j++];
		} else {
			i++;
			j++;
		}
	}

	ret->size = k;
	return ret;
}

/**
 * Return the entry at the given index.
 *
 * @param set The set to find the entry in.
 * @param index The index of the entry.
 * @return The entry, or NULL if the index is outside the boundries or set is NULL.
 *
 */
s4_entry_t *s4_set_get (s4_set_t *set, int index)
{
	if (set == NULL || index < 0 || index >= set->size)
		return NULL;

	return set->entries + index;
}

/**
 * Return the next entry into the set.
 *
 * @param set The set to find the next entry in.
 * @return The next entry, or NULL if set is NULL or you're at the end.
 *
 */
s4_entry_t *s4_set_next (s4_set_t *set)
{
	if (set == NULL || set->pos >= set->size)
		return NULL;

	return set->entries + set->pos++;
}

/**
 * Reset the position pointer so the next call to ::s4_set_next will
 * return the first entry.
 *
 * @param set The set to reset.
 *
 */
void s4_set_reset (s4_set_t *set)
{
	if (set != NULL)
		set->pos = 0;
}

/**
 * Insert the given entry into the set.
 *
 * @param set The set to insert into.
 * @param entry The entry to insert.
 * @return 1 on success, 0 if set is NULL or entry already is in the set.
 *
 */
int s4_set_insert (s4_set_t *set, s4_entry_t *entry)
{
	int equal;
	int index;
	int i;

	if (set == NULL)
		return 0;

	index = search (set, entry, &equal);

	if (equal)
		return 0;

	if ((set->size + 1) >= set->alloc) {
		expand (set);
	}

	memmove (set->entries + index + 1, set->entries + index,
			(set->size - index) * sizeof (s4_entry_t));

	set->entries[index] = *entry;
	set->size++;

	return 1;
}

/**
 * @}
 */
