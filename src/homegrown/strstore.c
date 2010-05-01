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
#include <stdlib.h>
#include <stdio.h>


#define STR_MAGIC 0xafa7beef

/**
 * @defgroup StringStore StringStore
 * @ingroup Homegrown
 * @brief A string-to-int module
 *
 * @{
 */

typedef struct str_info_St {
	int32_t magic;
	int32_t refs;
} str_info_t;

char *s4be_st_normalize (const char *key)
{
	char *tmp = g_utf8_casefold (key, -1);
	char *ret = g_utf8_normalize (tmp, -1, G_NORMALIZE_DEFAULT);

	if (ret == NULL) {
		ret = tmp;
	} else {
		g_free (tmp);
	}

	return ret;
}


static str_info_t *get_info (s4be_t *s4, int32_t node)
{
	const char *data;

	data = pat_node_to_str (s4, node);

	return (str_info_t*)data;
}


static void fillin_key (pat_key_t *key, const char *str)
{
	key->common_key = s4be_st_normalize (str);
	key->common_keylen = (strlen (key->common_key) + 1) * 8;
	key->unique_keylen = strlen (str) + sizeof (str_info_t) + 1;
	key->unique_key = malloc (key->unique_keylen);
	key->unique_keyoff = sizeof (str_info_t);

	strncpy (key->unique_key + key->unique_keyoff, str, strlen (str) + 1);
}

static void free_key (pat_key_t *key)
{
	g_free (key->common_key);
	free (key->unique_key);
}

/**
 * Look up a string and return the associated int
 *
 * @param s4 The database handle
 * @param str The string to look up
 * @return The int, or 0 if it can not find it
 */
int32_t s4be_st_lookup (s4be_t *s4, const char *str)
{
	int32_t ret;
	pat_key_t key;

	fillin_key (&key, str);

	be_rlock (s4);
	ret = pat_lookup (s4, S4_STRING_STORE, &key);
	be_runlock (s4);

	free_key (&key);

	if (ret == -1)
		ret = 0;

	return ret;
}

int32_t *s4be_st_lookup_all (s4be_t *be, const char *str)
{
	pat_key_t key;
	int32_t leaf, child;
	int32_t *ret;
	int count, i;

	fillin_key (&key, str);
	be_rlock (be);
	leaf = pat_lookup_parent (be, S4_STRING_STORE, &key);
	free_key (&key);

	if (leaf == -1) {
		be_runlock (be);
		return NULL;
	}

	count = pat_parent_key_count (be, leaf);
	ret = malloc (sizeof(int32_t) * (count + 1));
	child = pat_parent_first_key (be, leaf);

	for (i = 0; i < count; i++) {
		ret[i] = child;
		child = pat_next (be, S4_STRING_STORE, child);
	}
	ret[count] = -1;

	be_runlock (be);

	return ret;
}

static int get_refcount (s4be_t* s4, int32_t node)
{
	str_info_t *info = get_info (s4, node);

	return info->refs;
}


/**
 * Return the refcount of the given node
 *
 * @param s4 The database handle
 * @param node The node to find the refcount of
 * @return The refcount
 *
 */
int s4be_st_get_refcount (s4be_t *s4, int32_t node)
{
	int ret;

	be_rlock (s4);
	ret = get_refcount (s4, node);
	be_runlock (s4);

	return ret;
}

static int set_refcount (s4be_t *s4, int32_t node, int refcount)
{
	str_info_t *info = get_info (s4, node);

	if (info->magic != STR_MAGIC)
		return 0;

	info->refs = refcount;
	return 1;
}

/**
 * Set the refcount of a node. Only use this if you know what you're doing!
 *
 * @param s4 The database handle
 * @param node The node to set the refcount of
 * @param refcount The new refcount
 * @return 1 on success, 0 on error.
 *
 */
int s4be_st_set_refcount (s4be_t *s4, int32_t node, int refcount)
{
	int ret;

	be_wlock (s4);
	ret = set_refcount (s4, node, refcount);
	be_wunlock (s4);

	return ret;
}


static const char *s4be_st_get_str (s4be_t *s4, int32_t node)
{
	return pat_node_to_str (s4, node) + sizeof (str_info_t);
}
static const char *s4be_st_get_normalized_str (s4be_t *s4, int32_t node)
{

	return S4_PNT (s4, pat_node_to_key (s4, pat_parent (s4, node)), char);
}

/**
 * Remove the string
 *
 * @param s4 The database handle
 * @param str The string to remove
 * @return 1 if everything went okay, 0 otherwise
 *
 */
int s4be_st_remove (s4be_t *s4, const char *str)
{
	pat_key_t key;
	int ret;

	fillin_key (&key, str);

	be_wlock (s4);
	ret = !pat_remove (s4, S4_STRING_STORE, &key);
	be_wunlock (s4);

	free_key (&key);

	return ret;
}

/**
 * Find the string associated with the int
 *
 * @param s4 The database handle
 * @param node The int
 * @return The string
 */
char *s4be_st_reverse (s4be_t *s4, int32_t node)
{
	char *ret;
	be_rlock (s4);

	ret = strdup (s4be_st_get_str (s4, node));

	be_runlock (s4);
	return ret;
}

/**
 * Find the normalized string associated with the int
 *
 * @param s4 The database handle
 * @param node The int
 * @return The normalized string
 */
char *s4be_st_reverse_normalized (s4be_t *s4, int32_t node)
{
	char *ret;
	be_rlock (s4);

	ret = strdup (s4be_st_get_normalized_str (s4, node));

	be_runlock (s4);
	return ret;
}


/**
 * Add a reference to the string
 *
 * @param s4 The database handle
 * @param str The string to reference
 * @return The integer associated with the string
 */
int s4be_st_ref (s4be_t *s4, const char *str)
{
	pat_key_t key;
	int32_t node;
	str_info_t *info;

	fillin_key(&key, str);

	be_wlock (s4);
	node = pat_lookup (s4, S4_STRING_STORE, &key);

	if (node != -1) {
		set_refcount (s4, node, get_refcount (s4, node) + 1);
		be_wunlock (s4);

		free_key (&key);

		return node;
	}

	info = (str_info_t*)key.unique_key;
	info->magic = STR_MAGIC;
	info->refs = 1;

	node = pat_insert (s4, S4_STRING_STORE, &key);

	be_wunlock (s4);

	free_key (&key);

	return node;
}


/**
 * Remove a reference from a string
 *
 * @param s4 The database handle
 * @param str The string to unref
 * @return -1 if the string does not exist, the refcount otherwise
 */
int s4be_st_unref (s4be_t * s4, const char *str)
{
	int32_t node;
	pat_key_t key;
	int count;

	fillin_key (&key, str);

	be_wlock (s4);

	node = pat_lookup (s4, S4_STRING_STORE, &key);

	if (node == -1) {
		be_wunlock (s4);
		free_key (&key);
		return -1;
	}

	count = get_refcount (s4, node) - 1;

	if (count == 0) {
		pat_remove (s4, S4_STRING_STORE, &key);
	} else {
		set_refcount (s4, node, count);
	}

	be_wunlock (s4);
	free_key (&key);
	return count;
}


/* Try to read the key and return it in pkey
 * Return 1 if it succeded, 0 otherwise
 */
static int _copy_key (s4be_t *db, int32_t key, pat_key_t *pkey)
{
	const char *data;
	str_info_t *info;

	data = pat_node_to_str (db, key);
	info = (str_info_t*)data;

	if (key < 0 || key > (db->size - sizeof (str_info_t)) || info->magic != STR_MAGIC)
		return 0;

	fillin_key (pkey, data);

	return 1;
}

struct recovery_info {
	s4be_t *old, *new;
	int32_t trie;
};

static void recovery_helper (int32_t node, void *u)
{
	struct recovery_info *info = u;
	int32_t key;
	pat_key_t pkey;

	key = pat_node_to_key (info->old, node);

	if (_copy_key (info->old, key, &pkey)) {
		pat_insert (info->new, info->trie, &pkey);
	}
}

/* Called when the database needs to be recovered. */
int _st_recover (s4be_t *old, s4be_t *rec)
{
	struct recovery_info info;

	info.old = old;
	info.new = rec;
	info.trie = S4_STRING_STORE;

	pat_recover (old, recovery_helper, &info);

	return 0;
}

int _st_verify (s4be_t *be)
{
	return pat_verify (be, S4_STRING_STORE);
}


void s4be_st_foreach (s4be_t *be,
		void (*func) (int32_t node, void *userdata),
		void *userdata)
{
	int32_t node = pat_first (be, S4_STRING_STORE);

	while (node != -1) {
		func (node, userdata);
		node = pat_next (be, S4_STRING_STORE, node);
	}
}

/**
 * @}
 */
