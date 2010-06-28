/*  S4 - An XMMS2 medialib backend
 *  Copyright (C) 2009, 2010 Sivert Berg
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

#include "s4_priv.h"

/**
 *
 * @internal
 * @defgroup String String
 * @ingroup S4
 * @brief Handles string in S4
 *
 * @{
 */

/**
 * Get a pointer to a constant string that's equal to str. _string_lookup
 * will always return the same pointer for the same string
 *
 * @param s4 The database to look for the string in
 * @param str The string to find the constant string of
 * @return A pointer to a string equal to str
 */
const char *_string_lookup (s4_t *s4, const char *str)
{
	const char *ret;

	if (str == NULL)
		return NULL;

	g_static_mutex_lock (&s4->strings_lock);
	ret = g_string_chunk_insert_const (s4->strings, str);
	g_static_mutex_unlock (&s4->strings_lock);

	return ret;
}

/**
 * Get the normalized string of str
 *
 * @param str The string to normalize
 * @return A normalized version of str, must be freed with g_free
 */
char *s4_normalize_string (const char *str)
{
	char *tmp = g_utf8_casefold (str, -1);
	char *ret = g_utf8_normalize (tmp, -1, G_NORMALIZE_DEFAULT);

	if (ret == NULL) {
		ret = tmp;
	} else {
		g_free (tmp);
	}

	return ret;
}

/**
 * Get the normalized string corresponding to str. str must have
 * been obtained by calling _string_lookup.
 *
 * @param s4 The database to look in
 * @param str The string to find the normalized string of
 * @return The normalized string of str
 */
const char *_string_lookup_normalized (s4_t *s4, const char *str)
{
	const char *ret;

	g_static_mutex_lock (&s4->norm_lock);
	ret = g_hash_table_lookup (s4->norm_table, str);

	if (ret == NULL) {
		char *tmp = s4_normalize_string (str);
		ret = _string_lookup (s4, tmp);
		g_free (tmp);

		g_hash_table_insert (s4->norm_table, (void*)str, (void*)ret);
	}

	g_static_mutex_unlock (&s4->norm_lock);

	return ret;
}

/**
 * @}
 */
