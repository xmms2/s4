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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "s4_priv.h"

/**
 *
 * @defgroup Pattern Pattern
 * @ingroup S4
 * @brief Functions to create and match glob-like patterns.
 *
 * Patterns consist of normal characters and the special characters
 * '?' and '*', where ? matches against any character while
 * * matches against any number of characters.
 *
 * @{
 */

/* The structure used to represent a sub-pattern.
 * A sub-pattern is a string not containing any stars.
 * The pattern 'a*bc*' is split into the sub-patterns
 * 'a', 'bc' and ''
 */
typedef struct pattern_St {
	char *str; /* The string to match, for string patterns '?' is replaced by '\0' */
	int len;   /* The lenght of the string */
	struct pattern_St *next; /* The next sub-pattern */
} pattern_t;

struct s4_pattern_St {
	int casefolded;
	pattern_t *str_pattern;
	pattern_t *pos_pattern;
	pattern_t *neg_pattern;
};

/* Calculates 10^exp */
static int
_power (int exp)
{
	int ret = 1;

	/* This is not entirely correct, as 10^0 = 1, but without this
	 * 0 is treated as 0 digits, while it is actually 1 digit
	 */
	if (exp <= 0)
		return 0;

	for (; exp > 0; exp--)
		ret *= 10;

	return ret;
}

/* Returns a reversed copy of str, have to be freed */
static char *
_str_rev (const char *str)
{
	char *ret = strdup (str);
	int len = strlen (ret);
	int i, j, tmp;

	for (i = 0, j = len - 1; i < j; i++, j--) {
		/* Swap i and j */
		tmp = ret[i];
		ret[i] = ret[j];
		ret[j] = tmp;
	}

	return ret;
}

/* Creates a numeric pattern of a string.
 * The string is assumed to be a valid numeric pattern
 */
static pattern_t *
_num_pattern_create (const char *pattern)
{
	int i;
	int star;
	char *str = strdup (pattern);
	pattern_t *ret, *p;
	ret = p = calloc (1, sizeof (pattern_t));

	/* Runs through the string in reverse, splitting it up on stars */
	for (star = 0, i = strlen (str) - 1; i >= 0; i--) {
		if (str[i] == '*') {
			str[i] = '\0';

			/* Skip stars following stars */
			if (!star) {
				p->str = _str_rev (str + i + 1);
				p->len = strlen (p->str);
				p->next = calloc (1, sizeof (pattern_t));
				p = p->next;
			}
			star = 1;
		} else {
			star = 0;
		}
	}

	/* Save the rest of the string */
	p->next = NULL;
	p->str = _str_rev (str);
	p->len = strlen (p->str);

	free (str);

	return ret;
}

/* Matches the p->len first digits of num against p
 * If num is too small or doesn't match -1 is returned
 * otherwise num / (10^p->len) is returned
 * (basically num with the matched digits cut off)
 */
static int32_t
_match_num (pattern_t *p, int32_t num)
{
	int i;

	/* Check if the number has enough digits */
	if (num < _power (p->len - 1))
		return -1;

	/* Compare the first p->len of num digits against p->str */
	for (i = 0; i < p->len; i++, num /= 10) {
		if (p->str[i] != '?') {
			int c = p->str[i] - '0';
			int d = num % 10;
			if (c != d)
				return -1;
		}
	}

	return num;
}

/* Searches the number for the pattern p
 * Returns the digits after the matches pattern,
 * or -1 if the pattern was not found.
 */
static int32_t
_find_num (pattern_t *p, int32_t num)
{
	int32_t ret = 0;
	for (; (ret = _match_num (p, num)) == -1 && num; num /= 10);

	return ret;
}

/* Tries to match the pattern p against num
 * Returns non-zero if it matches, 0 otherwise
 */
static int
_num_pattern_match (pattern_t *p, int32_t num)
{
	int first = 1;

	if (p == NULL)
		return 0;

	/* Match the first n - 1 sub-patterns */
	for (; p->next != NULL; p = p->next) {
		/* The first sub-pattern have to match exactly, no star infront of it */
		if (first) {
			num = _match_num (p, num);
			first = 0;
		} else {
			num = _find_num (p, num);
		}
		if (num == -1)
			return 0;
	}

	/* If there is only one sub-pattern it must match exactly */
	if (first) {
		return !_match_num (p, num);
	} else if (p->len > 0) {
		/* We drop everything except the last p->len digits */
		for (; num >= _power (p->len); num /= 10);
		return !_match_num (p, num);
	} else {
		return 1;
	}
}

/* Makes a copy of str and saves it in the pattern structure
 * If casefold is non-zero the string is casefolded with s4_string_casefold
 */
static void
_copy_str (const char *str, int len, pattern_t *p, int casefold)
{
	GString *g_str = g_string_new ("");

	if (casefold) {
		int prev, i;

		for (i = prev = 0; i <= len; i++) {
			if (i == len || str[i] == '\0') {
				if (i > prev) {
					char *c = s4_string_casefold (str + prev);
					g_string_append (g_str, c);
					g_free (c);
				}
				if (i < len)
					g_string_append_c (g_str, '\0');
				prev = i + 1;
			}
		}

		p->len = g_str->len;
		p->str = malloc (p->len);
		memcpy (p->str, g_str->str, p->len);
	} else {
		p->str = malloc (len);
		memcpy (p->str, str, len);
		p->len = len;
	}

	g_string_free (g_str, TRUE);
}

/* Creates a string pattern.
 * If casefold is non-zero, the pattern will match against casefolded strings
 */
static pattern_t *
_str_pattern_create (const char *pattern, int casefold)
{
	int i, prev, star;
	char *str = strdup (pattern);
	pattern_t *ret, *p;
	ret = p = calloc (1, sizeof (pattern_t));

	/* Walk through the string, replacing '?' with '\0'
	 * and splitting it on '*'
	 */
	for (star = prev = i = 0; str[i]; i++) {
		if (str[i] == '?') {
			str[i] = '\0';
			star = 0;
		} else if (str[i] == '*') {
			/* Skip stars following stars */
			if (!star) {
				str[i] = '\0';
				p->next = calloc (1, sizeof (pattern_t));
				_copy_str (str + prev, i - prev, p, casefold);
				p = p->next;
				star = 1;
			}
			prev = i + 1;
		} else {
			star = 0;
		}
	}

	/* Save the tail of the string */
	p->next = NULL;
	_copy_str (str + prev, i - prev, p, casefold);

	free (str);

	return ret;
}

/* Match a pattern against a string
 * Returns 1 if it matches, 0 otherwise
 */
static int
_match_pattern (const char *str, pattern_t *p)
{
	int i;

	for (i = 0; i < p->len; i++) {
		if (p->str[i] != 0 && p->str[i] != str[i])
			return 0;
	}

	return 1;
}

/* Searches the string for the pattern p
 * Returns the index of the first place matching, or -1 if it's not found
 */
static int
_find_pattern (const char *str, int len, pattern_t *p)
{
	int i = 0;
	int stop = len - p->len;

	for (i = 0; i <= stop && !_match_pattern (str + i, p); i++);

	if (i > stop)
		return -1;
	return i;
}

/* Tries to match the pattern against the given string
 * Returns non-zero if it matches, 0 otherwise
 */
static int
_str_pattern_match (pattern_t *p, const char *str)
{
	int first = 1;
	int len = strlen (str);
	int i, j;

	/* Match the first n - 1 subpatterns against the string */
	for (i = 0; i < len && p->next != NULL; p = p->next) {
		/* The first sub-pattern must start at index 0 */
		if (first) {
			if (p->len > len || !_match_pattern (str, p))
				return 0;
			i += p->len;
			first = 0;
		/* The rest can start anywhere >= i */
		} else {
			j = _find_pattern (str + i, len - i, p);
			if (j == -1)
				return 0;
			i += j + p->len;
		}
	}

	if (first) {
		/* If this is the first (and last) sub-pattern it must be an exact match */
		if (p->len != len)
			return 0;
		return _match_pattern (str, p);
	} else if (p->len <= (len - i)) {
		/* Otherwise we match the pattern against the end of the string */
		return _match_pattern (str + len - p->len, p);
	} else {
		/* In case the string is too short */
		return 0;
	}
}

/* Checks if the string is a numerical pattern
 * A numerical pattern is a pattern consisting
 * of only digits, '?' and '*'
 */
static int
_is_num_pattern (const char *s)
{
	/* Skip leading minus */
	if (*s == '-') s++;

	for (; *s; s++) {
		if (!isdigit (*s) && *s != '?' && *s != '*')
			return 0;
	}

	return 1;
}

/* Free a pattern structure */
static void
_free_pattern (pattern_t *pattern)
{
	pattern_t *tmp;
	while (pattern != NULL) {
		tmp = pattern;
		pattern = pattern->next;

		free (tmp->str);
		free (tmp);
	}
}

/**
 * Creates a new pattern.
 *
 * @param pattern The string to use as the pattern
 * @param casefold Match against both upper and lower case
 * @return A new pattern that may be used with s4_pattern_match.
 * Must be freed with s4_pattern_free
 */
s4_pattern_t *
s4_pattern_create (const char *pattern, int casefold)
{
	s4_pattern_t *ret = malloc (sizeof (s4_pattern_t));

	ret->casefolded = casefold;
	ret->str_pattern = _str_pattern_create (pattern, casefold);
	ret->pos_pattern = NULL;
	ret->neg_pattern = NULL;

	if (_is_num_pattern (pattern)) {
		/* No minus sign - we treat it as a positive pattern */
		if (*pattern != '-') {
			ret->pos_pattern = _num_pattern_create (pattern);
		}
		/* If there is a minus sign or a ? that could match the minus sign
		 * we cut off the ? or - and create a pattern out of the rest
		 */
		if (*pattern == '-' || *pattern == '?') {
			ret->neg_pattern = _num_pattern_create (pattern + 1);
		/* A star could match the minus sign */
		} else if (*pattern == '*') {
			ret->neg_pattern = _num_pattern_create (pattern);
		}
	}

	return ret;
}

/**
 * Matches a pattern against a value
 * @param p The pattern to use
 * @param val The value to match
 * @return non-zero if the pattern matches the value, 0 otherwise
 */
int
s4_pattern_match (s4_pattern_t *p, const s4_val_t *val)
{
	int32_t i;
	const char *str;

	if (s4_val_is_str (val)) {
		if (p->casefolded) {
			s4_val_get_casefolded_str (val, &str);
			return _str_pattern_match (p->str_pattern, str);
		} else {
			s4_val_get_str (val, &str);
			return _str_pattern_match (p->str_pattern, str);
		}
	} else if (s4_val_get_int (val, &i)) {
		if (i >= 0) {
			return _num_pattern_match (p->pos_pattern, i);
		} else {
			return _num_pattern_match (p->neg_pattern, -i);
		}
	}
	return 0;
}

/**
 * Frees a pattern created with s4_pattern_create
 * @param pattern The pattern to free
 */
void
s4_pattern_free (s4_pattern_t *pattern)
{
	_free_pattern (pattern->str_pattern);
	_free_pattern (pattern->pos_pattern);
	_free_pattern (pattern->neg_pattern);
	free (pattern);
}

/**
 * @}
 */
