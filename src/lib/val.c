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
#include <ctype.h>

typedef enum {
	S4_VAL_STR,
	S4_VAL_STR_INTERNAL,
	S4_VAL_INT
} s4_val_type_t;

struct s4_val_St {
	s4_val_type_t type;
	union {
		struct {
			s4_t *s4;
			const char *s;
			const char *co;
			const char *ca;
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
	val->v.str.co = NULL;
	val->v.str.ca = NULL;

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
 * @param s4 The database this value belongs to
 * @return A new internal string value, must be freed with s4_val_free
 */
s4_val_t *s4_val_new_internal_string (const char *str, s4_t *s4)
{
	s4_val_t *val = malloc (sizeof (s4_val_t));
	val->type = S4_VAL_STR_INTERNAL;
	val->v.str.s = str;
	val->v.str.co = NULL;
	val->v.str.ca = NULL;
	val->v.str.s4 = s4;

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
	if (val == NULL)
		return;

	if (val->type == S4_VAL_STR) {
		free ((void*)val->v.str.s);
		if (val->v.str.ca != NULL)
			g_free ((void*)val->v.str.ca);
		if (val->v.str.co != NULL)
			g_free ((void*)val->v.str.co);
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
int s4_val_get_collated_str (const s4_val_t *val, const char **str)
{
	if (!s4_val_is_str (val))
		return 0;

	if (val->v.str.co == NULL) {
		if (val->type == S4_VAL_STR) {
			((s4_val_t*)val)->v.str.co = s4_string_collate (val->v.str.s);
		} else {
			((s4_val_t*)val)->v.str.co = _string_lookup_collated (val->v.str.s4, val->v.str.s);
		}
	}
	*str = val->v.str.co;
	return 1;
}

/**
 * Tries to get the normalized string in a string value
 *
 * @param val The value to get the string of
 * @param str A pointer to a pointer where the string pointer will be stored
 * @return 0 if val is not a string value, non-zero otherwise
 */
int s4_val_get_casefolded_str (const s4_val_t *val, const char **str)
{
	if (!s4_val_is_str (val))
		return 0;

	if (val->v.str.ca == NULL) {
		if (val->type == S4_VAL_STR) {
			((s4_val_t*)val)->v.str.ca = s4_string_casefold (val->v.str.s);
		} else {
			((s4_val_t*)val)->v.str.ca = _string_lookup_casefolded (val->v.str.s4, val->v.str.s);
		}
	}
	*str = val->v.str.ca;
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
 * Compares an int and a string
 *
 * @param i The int
 * @param s The string
 * @param coll_str A collated version of the string. If this is NULL
 * strings will always be placed before integers.
 * @return <0 if i<s, 0 if i==s and >0 if i>s
 */
static int _int_str_cmp (int32_t i, const char *s, const char *coll_str)
{
	static char *coll_num = NULL;

	if (coll_num == NULL) {
		coll_num = s4_string_collate ("1");
	}

	/* If we're comparing binary or caselessly we place integers after strings
	 * This is because with binary and caseless string-matching "123" < "23"
	 * but integers still use integer matching and 23 < 123, so we have to
	 * keep them separate (or we would break antisymmetry)
	 */
	if (coll_str == NULL) {
		return 1;
	/* But for collated matching we have "23" < "123" and we can mix string
	 * and integer matching without breaking anything
	 */
	} else {
		char *end;
		int32_t j = strtol (s, &end, 10);

		/* Strings with space in front of the number are treated like normal strings */
		if (end != s && !isspace (*s)) {
			return (i > j)?1:((i < j)?-1:-(*end != '\0'));
		} else {
			/* See if the given string is smaller or greater than a number */
			return strcmp (coll_num, coll_str);
		}
	}
}

/**
 * Compares two values
 *
 * @param v1 The first value
 * @param v2 The second value
 * @param mode How to compare the values
 * @return <0 if v1<v2, 0 if v1==v2 and >0 if v1>v2
 */
int s4_val_cmp (const s4_val_t *v1, const s4_val_t *v2, s4_cmp_mode_t mode)
{
	int32_t i1,i2;
	const char *s1,*s2;

	if (s4_val_get_int (v1, &i1) && s4_val_get_int (v2, &i2))
		return (i1 > i2)?1:((i1 < i2)?-1:0);
	else if (mode == S4_CMP_BINARY && s4_val_get_str (v1, &s1) && s4_val_get_str (v2, &s2))
		return strcmp (s1, s2);
	else if (mode == S4_CMP_CASELESS && s4_val_get_casefolded_str (v1, &s1) && s4_val_get_casefolded_str (v2, &s2))
		return strcmp (s1, s2);
	else if (mode == S4_CMP_COLLATE && s4_val_get_collated_str (v1, &s1) && s4_val_get_collated_str (v2, &s2))
		return strcmp (s1, s2);
	else if (s4_val_get_int (v1, &i1) && s4_val_get_str (v2, &s2))
		if (mode == S4_CMP_COLLATE) {
			s4_val_get_collated_str (v2, &s1);
			return _int_str_cmp (i1, s2, s1);
		} else {
			return _int_str_cmp (i1, s2, NULL);
		}
	else if (s4_val_get_int (v2, &i2) && s4_val_get_str (v1, &s1))
		if (mode == S4_CMP_COLLATE) {
			s4_val_get_collated_str (v1, &s2);
			return -_int_str_cmp (i2, s1, s2);
		} else {
			return -_int_str_cmp (i2, s1, NULL);
		}
	else /* This should never be hit */
		return 0;
}

/**
 * @}
 */
