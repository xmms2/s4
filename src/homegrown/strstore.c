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
#include <glib.h>


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

static char *collate_key (const char *key)
{
	char *tmp = g_utf8_casefold (key, -1);
	char *ret = g_utf8_collate_key (tmp, -1);

	g_free (tmp);

	return ret;
}

static str_info_t *get_info (s4be_t *s4, int32_t node)
{
	char *data;

	data = S4_PNT (s4, pat_node_to_key (s4, node), char);
	while (*data++);
	while (*data++);

	return (str_info_t*)data;
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

	key.data = collate_key (str);
	key.key_len = (strlen(key.data) + 1) * 8;

	be_rlock (s4);
	ret = pat_lookup (s4, S4_STRING_STORE, &key);
	be_runlock (s4);

	g_free (key.data);

	if (ret == -1)
		ret = 0;

	return ret;
}

/**
 * Look up a string that's already made ready to be collated.
 *
 * @param s4 The database handle
 * @param str The string to look up
 * @return The int, or 0 if it can't be found.
 *
 */
int32_t s4be_st_lookup_collated (s4be_t *s4, const char *str)
{
	int32_t ret;
	pat_key_t key;

	key.data = str;
	key.key_len = (strlen (str) + 1) * 8;

	be_rlock (s4);
	ret = pat_lookup (s4, S4_STRING_STORE, &key);
	be_runlock (s4);

	if (ret == -1)
		ret = 0;

	return ret;
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
	str_info_t *info;

	be_rlock (s4);

	info = get_info (s4, node);
	ret = info->refs;

	be_runlock (s4);

	return ret;
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
	int ret = 1;
	str_info_t *info;

	be_wlock (s4);

	info = get_info (s4, node);

	if (info->magic != STR_MAGIC)
		ret = 0;
	else
		info->refs = refcount;

	be_wunlock (s4);

	return ret;
}


static char *s4be_st_get_str (s4be_t *s4, int32_t node)
{
	char *ret = S4_PNT (s4, pat_node_to_key (s4, node), char);
	ret += strlen (ret) + 1;

	return ret;
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

	key.data = collate_key (str);
	key.key_len = strlen (key.data) * 8;

	ret = !pat_remove (s4, S4_STRING_STORE, &key);

	g_free (key.data);

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

	ret = s4be_st_get_str (s4, node);
	ret = strdup (ret);

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
	int lena = strlen (str) + 1;
	int lenb;
	str_info_t *info;
	char *data, *cstr = collate_key(str);

	lenb = strlen (cstr) + 1;

	be_wlock (s4);

	key.data = cstr;
	key.key_len = lenb * 8;
	node = pat_lookup (s4, S4_STRING_STORE, &key);

	if (node != -1) {
		data = S4_PNT (s4, pat_node_to_key (s4, node), char);
		info = (str_info_t*)(data + lenb + lena);

		info->refs++;
		be_wunlock (s4);
		g_free (cstr);
		return node;
	}

	data = malloc (lenb + lena + sizeof(str_info_t));
	strcpy (data, key.data);
	strcpy (data + lenb, str);
	info = (str_info_t*)(data + lena + lenb);
	info->magic = STR_MAGIC;
	info->refs = 1;

	key.data = data;
	key.data_len = lenb + lena + sizeof(str_info_t);
	node = pat_insert (s4, S4_STRING_STORE, &key);

	be_wunlock (s4);
	free (data);

	g_free (cstr);

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
	str_info_t *info;
	char *data;
	int lena = strlen (str) + 1;
	int lenb;
	pat_key_t key;

	be_wlock (s4);

	key.data = collate_key (str);
	lenb = strlen (key.data) + 1;
	key.key_len = lenb * 8;
	node = pat_lookup (s4, S4_STRING_STORE, &key);

	if (node == -1) {
		be_wunlock (s4);
		g_free (key.data);
		return -1;
	}

	data = S4_PNT (s4, pat_node_to_key (s4, node), char);
	info = (str_info_t*)(data + lena + lenb);
	info->refs--;

	if (info->refs == 0) {
		pat_remove (s4, S4_STRING_STORE, &key);
		be_wunlock (s4);
		g_free (key.data);
		return 0;
	}

	be_wunlock (s4);
	g_free (key.data);
	return info->refs;
}


/* Try to read the key and return it in pkey
 * Return 1 if it succeded, 0 otherwise
 */
static int _copy_key (s4be_t *db, int32_t key, pat_key_t *pkey)
{
	int len;
	char *data;
	str_info_t *info;

	if (key < 0 || key > db->size)
		return 0;

	data = S4_PNT (db, key, char);
	for (len = 0; data[len] && len < (db->size - key); len++);
	len++;

	pkey->key_len = len * 8;

	for (; data[len] && len < (db->size - key); len++);
	len++;


	info = S4_PNT (db, key + len, str_info_t);
	if (len >= (db->size - key - sizeof (str_info_t)) ||
			info->magic != STR_MAGIC)
		return 0;

	pkey->data = data;
	pkey->data_len = len + sizeof (str_info_t);

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

/**
 * Return a list with all strings matching the regexp.
 *
 * @param be The database handle
 * @param pat The regexp
 * @return A list with all the strings that matched
 */
GList *s4be_st_regexp (s4be_t *be, const char *pat)
{
	GError *error = NULL;
	GRegex *regex = g_regex_new (pat,
			G_REGEX_CASELESS | G_REGEX_OPTIMIZE, 0, &error);
	int32_t node = pat_first (be, S4_STRING_STORE);
	char *str;
	GList *ret = NULL;

	if (regex == NULL) {
		S4_ERROR ("Regex error: %s\n", error->message);
		return NULL;
	}

	be_rlock (be);

	while (node != -1) {
		str = s4be_st_get_str (be, node);
		if (g_regex_match (regex, str, 0, NULL)) {
			ret = g_list_prepend (ret, strdup (str));
		}

		node = pat_next (be, S4_STRING_STORE, node);
	}

	be_runlock (be);
	g_regex_unref (regex);

	return ret;
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
