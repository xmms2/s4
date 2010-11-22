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
 * @defgroup Constants Constants
 * @ingroup S4
 * @brief Handles constant values in S4
 *
 * @{
 */

/**
 * Gets a pointer to a constant string that's equal to str.
 * _string_lookup will always return the same pointer for the same string
 *
 * @param s4 The database to look for the string in
 * @param str The string to find the constant string of
 * @return A pointer to a string equal to str
 */
const char *_string_lookup (s4_t *s4, const char *str)
{
	const char *ret;
	const s4_val_t *val;

	if (str == NULL)
		return NULL;

	val = _string_lookup_val (s4, str);
	s4_val_get_str (val, &ret);

	return ret;
}

/**
 * Gets a pointer to a constant string value with a string equal to str.
 * _string_lookup_val will always return the same value for the same string.
 *
 * @param s4 The database to look for the string in
 * @param str The string to find the constant string of
 * @return A pointer to a string value
 */
const s4_val_t *_string_lookup_val (s4_t *s4, const char *str)
{
	s4_val_t *ret;

	g_static_mutex_lock (&s4->strings_lock);

	ret = g_hash_table_lookup (s4->strings_table, str);
	if (ret == NULL) {
		str = g_string_chunk_insert (s4->strings, str);
		ret = s4_val_new_internal_string (str, s4);
		g_hash_table_insert (s4->strings_table, (void*)str, ret);
	}

	g_static_mutex_unlock (&s4->strings_lock);

	return ret;
}

/**
 * Creates a string that orders correctly according to the locale
 *
 * @param str The string to collate
 * @return A collated version of str, must be freed with g_free
 */
char *s4_string_collate (const char *str)
{
	return g_utf8_collate_key_for_filename (str, -1);
}

/**
 * Creates a casefolded version of the string
 *
 * @param str The string to casefold
 * @return A casefolded version of str, free with g_free
 */
char *s4_string_casefold (const char *str)
{
	return g_utf8_casefold (str, -1);
}

/**
 * Gets the casefolded string corresponding to str. str must have
 * been obtained by calling _string_lookup.
 *
 * @param s4 The database to look in
 * @param str The string to find the casefold string of
 * @return The casefolded string of str
 */
const char *_string_lookup_casefolded (s4_t *s4, const char *str)
{
	const char *ret;

	g_static_mutex_lock (&s4->case_lock);
	ret = g_hash_table_lookup (s4->case_table, str);

	if (ret == NULL) {
		char *tmp = s4_string_casefold (str);
		ret = _string_lookup (s4, tmp);
		g_free (tmp);

		g_hash_table_insert (s4->case_table, (void*)str, (void*)ret);
	}

	g_static_mutex_unlock (&s4->case_lock);

	return ret;
}

/**
 * Gets the collated string corresponding to str. str must have
 * been obtained by calling _string_lookup.
 *
 * @param s4 The database to look in
 * @param str The string to find the collated string of
 * @return The collated string of str
 */
const char *_string_lookup_collated (s4_t *s4, const char *str)
{
	const char *ret;

	g_static_mutex_lock (&s4->coll_lock);
	ret = g_hash_table_lookup (s4->coll_table, str);

	if (ret == NULL) {
		char *tmp = s4_string_collate (str);
		ret = _string_lookup (s4, tmp);
		g_free (tmp);

		g_hash_table_insert (s4->coll_table, (void*)str, (void*)ret);
	}

	g_static_mutex_unlock (&s4->coll_lock);

	return ret;
}

const s4_val_t *_int_lookup_val (s4_t *s4, int32_t i)
{
	const s4_val_t *ret;

	g_static_mutex_lock (&s4->int_lock);
	ret = g_hash_table_lookup (s4->int_table, GINT_TO_POINTER (i));

	if (ret == NULL) {
		ret = s4_val_new_int (i);
		g_hash_table_insert (s4->int_table, GINT_TO_POINTER (i), (void*)ret);
	}

	g_static_mutex_unlock (&s4->int_lock);

	return ret;
}

const s4_val_t *_const_lookup (s4_t *s4, const s4_val_t *val)
{
	const char *str;
	int32_t ival;

	if (s4_val_get_int (val, &ival)) {
		return _int_lookup_val (s4, ival);
	} else if (s4_val_get_str (val, &str)) {
		return _string_lookup_val (s4, str);
	}
	return NULL;
}

/**
 * @}
 */
