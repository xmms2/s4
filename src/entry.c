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

#include <s4.h>
#include "s4_be.h"
#include <stdlib.h>
#include <string.h>

/**
 *
 * @defgroup Entry Entry
 * @ingroup S4
 * @brief Entries is what you insert and retrive
 *
 * Entries are essentially just a key-value pair, where the key is a string
 * and the value can be either a string or an int.
 *
 * Examples of entries (key, value):
 *  - String entry: ("title", "Foobar")
 *  - Integer entry: ("tracknr", 10)
 *
 * Entries can have other entries as properties. We can look at it
 * hierarchialy like in this graph.
 *
 * @dot
 * 	digraph entries {
 * 		node [color=lightblue2, style=filled, fontsize=12];
 * 		"\"album_id\", 1" -> "\"song_id\", 1"
 * 		"\"album_id\", 1" -> "\"album\", \"Some Album\"";
 * 		"\"album_id\", 1" -> "\"artist\", \"Some Artist\"";
 * 		"\"album_id\", 1" -> ".. other songs .."
 *
 * 		"\"song_id\", 1" -> "\"title\", \"Some Song\""
 * 		"\"song_id\", 1" -> "\"artist\", \"Some Artist\""
 * 		"\"song_id\", 1" -> "\"album\", \"Some Album\""
 * 		"\"song_id\", 1" -> "\"date\", 1999"
 * 		"\"song_id\", 1" -> "\"tracknr\", 1"
 * 	}
 * @enddot
 *
 * The top layer with a separate entry for albums are currently not used by
 * XMMS2, but there's nothing stopping us from doing it.
 *
 * @{
 */

/**
 * Create a new s4 entry and set the strings to key and val
 *
 * @param s4 The database handle
 * @param key The key
 * @param val The value
 * @return A new entry
 */
s4_entry_t *s4_entry_get_s (s4_t *s4, const char *key, const char *val)
{
	s4_entry_t *ret = malloc (sizeof (s4_entry_t));

	ret->key_s = strdup (key);
	ret->val_s = strdup (val);
	ret->key_i = ret->val_i = 0;
	ret->type = ENTRY_STR;
	ret->src_s = NULL;
	ret->src_i = 0;

	return ret;
}


/**
 * Create a new int entry.
 *
 * @param s4 The database handle
 * @param key The key
 * @param val The value
 * @return A new entry
 */
s4_entry_t *s4_entry_get_i (s4_t *s4, const char *key, int val)
{
	s4_entry_t *ret = malloc (sizeof (s4_entry_t));

	ret->key_s = strdup (key);
	ret->val_s = NULL;
	ret->key_i = 0;
	ret->val_i = val;
	ret->type = ENTRY_INT;
	ret->src_s = NULL;
	ret->src_i = 0;

	return ret;
}


/**
 * Free the strings the entry has
 *
 * @param entry The entry
 */
void s4_entry_free_strings (s4_entry_t *entry)
{
	if (entry->key_s != NULL)
		free (entry->key_s);
	if (entry->val_s != NULL)
		free (entry->val_s);
	if (entry->src_s != NULL)
		free (entry->src_s);
}


/**
 * Free an entry
 *
 * @param entry The entry to free
 */
void s4_entry_free (s4_entry_t *entry)
{
	s4_entry_free_strings (entry);

	free (entry);
}


static void _entry_unref (s4_t *s4, s4_entry_t *entry)
{
	if (s4be_st_unref (s4->be, entry->key_s) == 0)
		entry->key_i = 0;
	if (entry->type == ENTRY_STR && s4be_st_unref (s4->be, entry->val_s) == 0)
		entry->val_i = 0;
}


/* Lookup the strings in entry and fill in the int fields.
 * If ref != 0 and the strings doesn't exist yet they're created.
 * The return value has bit 1 set if they key was refed and
 * bit 2 set if the val was refed.
 * All other bits are 0.
 */
static int _entry_lookup (s4_t *s4, s4_entry_t *entry, int ref)
{
	int ret = 0;
	if (entry->key_i == 0) {
		entry->key_i = s4be_st_lookup (s4->be, entry->key_s);
		if (entry->key_i == 0) {
			if (ref) {
				ret = 1;
				entry->key_i = s4be_st_ref (s4->be, entry->key_s);
			} else
				return -1;
		}
		if (entry->type == ENTRY_INT)
			entry->key_i = -entry->key_i;
	} else if (entry->key_s == NULL) {
		entry->key_s = s4be_st_reverse (s4->be,
				(entry->type == ENTRY_STR)?(entry->key_i):(-entry->key_i));
	}
	if (entry->type == ENTRY_STR && entry->val_i == 0) {
		entry->val_i = s4be_st_lookup (s4->be, entry->val_s);
		if (entry->val_i == 0) {
			if (ref) {
				ret += 2;
				entry->val_i = s4be_st_ref (s4->be, entry->val_s);
			} else
				return -1;
		}
	} else if (entry->type == ENTRY_STR)
		entry->val_s = s4be_st_reverse (s4->be, entry->val_i);

	return ret;
}


/**
 * Fill in empty fields. If the strings are missing it looks
 * them up, if the ints are missing it looks those up.
 *
 * @param s4 The database handle
 * @param entry The entry
 */
void s4_entry_fillin (s4_t *s4, s4_entry_t *entry)
{
	if (entry->type == ENTRY_STR) {
		if (entry->key_i == 0 && entry->key_s != NULL)
			entry->key_i = s4be_st_lookup (s4->be, entry->key_s);
		else if (entry->key_s == NULL && entry->key_i != 0)
			entry->key_s = s4be_st_reverse (s4->be, entry->key_i);
		if (entry->val_i == 0 && entry->val_s != NULL)
			entry->val_i = s4be_st_lookup (s4->be, entry->val_s);
		else if (entry->val_s == NULL && entry->val_i != 0)
			entry->val_s = s4be_st_reverse (s4->be, entry->val_i);

	} else if (entry->key_i == 0 && entry->key_s != NULL) {
		entry->key_i = -s4be_st_lookup (s4->be, entry->key_s);
	} else if (entry->key_s == NULL && entry->key_i != 0) {
		entry->key_s = s4be_st_reverse (s4->be, -entry->key_i);
	}

	if (entry->src_i == 0 && entry->src_s != NULL)
		entry->src_i = s4be_st_lookup (s4->be, entry->src_s);
	else if (entry->src_s == NULL && entry->src_i != 0)
		entry->src_s = s4be_st_reverse (s4->be, entry->src_i);
}


/**
 * Add a property to the entry
 *
 * @param s4 The database handle
 * @param entry The entry
 * @param prop The property
 * @param src The source that set this property
 * @return 0 on success, -1 otherwise
 */
int s4_entry_add (s4_t *s4, s4_entry_t *entry, s4_entry_t *prop, const char *src)
{
	int ref;
	int ret;
	int32_t src_i = s4be_st_lookup (s4->be, src);

	ref = _entry_lookup (s4, entry, 1);
	ref += _entry_lookup (s4, prop, 1) * 4;

	if (src_i == 0) {
		src_i = s4be_st_ref (s4->be, src);
		ref += 16;
	}

	prop->src_s = strdup (src);
	prop->src_i = src_i;

	/* If the insertion went well we need to ref the strings */
	if (!(ret = s4be_ip_add (s4->be, entry, prop))) {
		if (!(ref & 1))
			s4be_st_ref (s4->be, entry->key_s);
		if (!(ref & 2) && entry->type == ENTRY_STR)
			s4be_st_ref (s4->be, entry->val_s);
		if (!(ref & 4))
			s4be_st_ref (s4->be, prop->key_s);
		if (!(ref & 8) && prop->type == ENTRY_STR)
			s4be_st_ref (s4->be, prop->val_s);
		if (!(ref & 16))
			s4be_st_ref (s4->be, src);
	}

	return ret;
}


/**
 * Delete a property from an entry
 *
 * @param s4 The database handle
 * @param entry The entry to remove from
 * @param prop The property to remove
 * @param src The source the removed this property
 * @return 0 on success, -1 otherwise
 */
int s4_entry_del (s4_t *s4, s4_entry_t *entry, s4_entry_t *prop, const char *src)
{
	int ret;
	int src_i = s4be_st_lookup (s4->be, src);

	if (src_i == 0 || _entry_lookup (s4, entry, 0) || _entry_lookup (s4, prop, 0))
		return -1;

	prop->src_s = strdup (src);
	prop->src_i = src_i;

	/* Unref the strings if we found them */
	if (!(ret = s4be_ip_del (s4->be, entry, prop))) {
		_entry_unref (s4, entry);
		_entry_unref (s4, prop);
		s4be_st_unref (s4->be, src);
	}

	return ret;
}


/**
 * Get all entries that contains entry
 *
 * @param s4 The database handle
 * @param entry The entry
 * @return A set with all the entries that contains entry
 */
s4_set_t *s4_entry_contains (s4_t *s4, s4_entry_t *entry)
{
	s4_entry_fillin (s4, entry);
	return s4be_ip_this_has (s4->be, entry);
}


/**
 * Get all entries that is contained in entry
 *
 * @param s4 The database handle
 * @param entry The entry
 * @return A set with all the entries contained in entry
 */
s4_set_t *s4_entry_contained(s4_t *s4, s4_entry_t *entry)
{
	s4_entry_fillin (s4, entry);
	return s4be_ip_has_this (s4->be, entry);
}


/**
 * Get all entries smaller than this one
 *
 * @param s4 The database handle
 * @param entry The entry
 * @return A set will all the entries that is contained in an entry
 * that has a smaller value than this one (but same key).
 */
s4_set_t *s4_entry_smaller (s4_t *s4, s4_entry_t *entry)
{
	s4_entry_fillin (s4, entry);
	return s4be_ip_smaller (s4->be, entry);
}


/**
 * Get all entries greater than this one
 *
 * @param s4 The database handle
 * @param entry The entry
 * @return A set will all the entries that is contained in an entry
 * that has a greater value than this one (but same key).
 */
s4_set_t *s4_entry_greater (s4_t *s4, s4_entry_t *entry)
{
	s4_entry_fillin (s4, entry);
	return s4be_ip_greater (s4->be, entry);
}

s4_set_t *s4_entry_get_property (s4_t *s4, s4_entry_t *entry, const char *prop)
{
	int32_t key = s4be_st_lookup (s4->be, prop);
	s4_entry_fillin (s4, entry);
	return s4be_ip_get (s4->be, entry, key);
}

/**
 * @}
 */
