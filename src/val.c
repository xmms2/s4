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
#include <string.h>
#include <stdlib.h>

typedef enum {
	S4_VAL_STR,
	S4_VAL_STR_INTERNAL,
	S4_VAL_INT
} s4_val_type_t;

struct s4_val_St {
	s4_val_type_t type;
	union {
		struct {
			const char *s;
			const char *n;
		} str;
		int32_t i;
	} v;
};

/**
 *
 * @defgroup Value Value
 * @ingroup S4
 * @brief The way values are represented in S4
 *
 * @{
 */

/**
 * Creates a new string value
 *
 * @param str The string to use as the value
 * @return A new string value, must be freed with s4_val_free
 */
s4_val_t *s4_val_new_string (const char *str)
{
	s4_val_t *val = malloc (sizeof (s4_val_t));
	val->type = S4_VAL_STR;
	val->v.str.s = strdup (str);
	val->v.str.n = NULL;

	return val;
}

/**
 * @{
 * @internal
 */

/**
 * Creates a new internal string value.
 * Internal string values are different from normal string values in that
 * the string is not copied.
 *
 * @param str The string to use as the value
 * @param normalized_str The normalized version of str
 * @return A new internal string value, must be freed with s4_val_free
 */
s4_val_t *s4_val_new_internal_string (const char *str, const char *normalized_str)
{
	s4_val_t *val = malloc (sizeof (s4_val_t));
	val->type = S4_VAL_STR_INTERNAL;
	val->v.str.s = str;
	val->v.str.n = normalized_str;

	return val;
}

/**
 * @}
 */

/**
 * Creates a new integer value
 *
 * @param i The integer to use as the value
 * @return A new integer value, must be freed with s4_val_free
 */
s4_val_t *s4_val_new_int (int32_t i)
{
	s4_val_t *val = malloc (sizeof (s4_val_t));
	val->type = S4_VAL_INT;
	val->v.i = i;

	return val;
}

/**
 * Copies a value
 *
 * @param val The value to copy
 * @return A new value that's a copy of val, must be freed with s4_val_free
 */
s4_val_t *s4_val_copy (const s4_val_t *val)
{
	const char *s;
	int32_t i;

	if (s4_val_get_str (val, &s)) {
		return s4_val_new_string (s);
	} else if (s4_val_get_int (val, &i)) {
		return s4_val_new_int (i);
	} else {
		return NULL;
	}
}

/**
 * Frees a value
 *
 * @param val The value to free
 */
void s4_val_free (s4_val_t *val)
{
	if (val->type == S4_VAL_STR) {
		free ((void*)val->v.str.s);
		if (val->v.str.n != NULL)
			g_free ((void*)val->v.str.n);
	}
	free (val);
}

/**
 * Checks is a value is a string value
 *
 * @param val The value to check
 * @return non-zero if val is a string value, 0 otherwise
 */
int s4_val_is_str (const s4_val_t *val)
{
	return val->type == S4_VAL_STR || val->type == S4_VAL_STR_INTERNAL;
}

/**
 * Checks is a value is an integer value
 *
 * @param val The value to check
 * @return non-zero if val is an integer value, 0 otherwise
 */
int s4_val_is_int (const s4_val_t *val)
{
	return val->type == S4_VAL_INT;
}

/**
 * Tries to get the string in a string value
 *
 * @param val The value to get the string of
 * @param str A pointer to a pointer where the string pointer will be stored
 * @return 0 if val is not a string value, non-zero otherwise
 */
int s4_val_get_str (const s4_val_t *val, const char **str)
{
	if (!s4_val_is_str (val))
		return 0;

	*str = val->v.str.s;
	return 1;
}

/**
 * Tries to get the normalized string in a string value
 *
 * @param val The value to get the string of
 * @param str A pointer to a pointer where the string pointer will be stored
 * @return 0 if val is not a string value, non-zero otherwise
 */
int s4_val_get_normalized_str (const s4_val_t *val, const char **str)
{
	if (!s4_val_is_str (val))
		return 0;

	if (val->v.str.n == NULL) {
		((s4_val_t*)val)->v.str.n = s4_normalize_string (val->v.str.s);
	}
	*str = val->v.str.n;
	return 1;
}

/**
 * Tries to get the integer in an integer value
 *
 * @param val The value to get the integer of
 * @param i A pointer to an integer where the integer will be stored
 * @return 0 if val is not an integer value, non-zero otherwise
 */
int s4_val_get_int (const s4_val_t *val, int32_t *i)
{
	if (!s4_val_is_int (val))
		return 0;

	*i = val->v.i;
	return 1;
}

/**
 * Compares two values
 *
 * @param v1 The first value
 * @param v2 The second value
 * @param casesens Non-zero if the values should be compared casesensitively, 0 otherwise
 * @return <0 if v1<v2, 0 if v1==v2 and >0 if v1>v2
 */
int s4_val_cmp (const s4_val_t *v1, const s4_val_t *v2, int casesens)
{
	int32_t i1,i2;
	const char *s1,*s2;

	if (s4_val_get_int (v1, &i1) && s4_val_get_int (v2, &i2))
		return (i1 > i2)?1:((i1 < i2)?-1:0);
	else if (casesens && s4_val_get_str (v1, &s1) && s4_val_get_str (v2, &s2))
		return strcmp (s1, s2);
	else if (!casesens && s4_val_get_normalized_str (v1, &s1) && s4_val_get_normalized_str (v2, &s2))
		return strcmp (s1, s2);
	else if (s4_val_is_int (v1))
		return -1;
	else
		return 1;
}

/**
 * @}
 */
