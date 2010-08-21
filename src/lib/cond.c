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
#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
	S4_COND_COMBINER,
	S4_COND_FILTER,
} s4_cond_type_t;

struct s4_condition_St {
	s4_cond_type_t type;
	int ref_count;
	union {
		struct {
			s4_combine_type_t type;
			combine_function_t func;
			GPtrArray *operands;
		} combine;
		struct {
			s4_filter_type_t type;
			filter_function_t func;
			void *funcdata;
			free_func_t free_func;
			const char *key;
			s4_sourcepref_t *sp;
			s4_cmp_mode_t cmp_mode;
			int flags;
			int monotonic;
			int const_key;
		} filter;
	} u;
};

/**
 *
 * @defgroup Condition Condition
 * @ingroup S4
 * @brief Functions to create and use S4 search conditions
 *
 * @{
 */

/*
 * A filter that matches nothing
 */
static int never (void)
{
	return 1;
}

/*
 * A filter that matches everything
 */
static int everything (void)
{
	return 0;
}

/*
 * A shortcutting OR combiner. It returns as soon as one of the operands is met
 */
static int or_combiner (s4_condition_t *cond, check_function_t func, void *check_data)
{
	int i, ret = 1;

	for (i = 0; ret && i < cond->u.combine.operands->len; i++) {
		ret = func (g_ptr_array_index (cond->u.combine.operands, i), check_data);
	}

	return ret;
}

/*
 * A shortcutting AND combiner. It returns as soon as one of the operands is not met
 */
static int and_combiner (s4_condition_t *cond, check_function_t func, void *check_data)
{
	int i, ret = 0;

	for (i = 0; !ret && i < cond->u.combine.operands->len; i++) {
		ret = func (g_ptr_array_index (cond->u.combine.operands, i), check_data);
	}

	return ret;
}

/*
 * A NOT combiner, it only takes the first operand into account
 */
static int not_combiner (s4_condition_t *cond, check_function_t func, void *check_data)
{
	return !func(cond->u.combine.operands->pdata[0], check_data);
}

/*
 * Returns the correct combiner function based on combine type
 */
static combine_function_t _get_combine_function (s4_combine_type_t type) {
	switch (type) {
		case S4_COMBINE_OR:
			return or_combiner;
		case S4_COMBINE_AND:
			return and_combiner;
		case S4_COMBINE_NOT:
			return not_combiner;
		default:
			break;
	}
	return (combine_function_t)never;
}

/*
 * A filter that checks if the given and checked value is equal
 */
static int equal_filter (const s4_val_t *value, s4_condition_t *cond)
{
	s4_val_t *d = cond->u.filter.funcdata;
	return s4_val_cmp (value, d, cond->u.filter.cmp_mode);
}
/*
 * A filter that checks if the given and checked value are different
 */
static int notequal_filter (const s4_val_t *value, s4_condition_t *cond)
{
	s4_val_t *d = cond->u.filter.funcdata;
	return !s4_val_cmp (value, d, cond->u.filter.cmp_mode);
}
/*
 * A filter that checks if the checked value is greater than the given value
 */
static int greater_filter (const s4_val_t *value, s4_condition_t* cond)
{
	s4_val_t *d = cond->u.filter.funcdata;
	return -(s4_val_cmp (value, d, cond->u.filter.cmp_mode) <= 0);
}
/*
 * A filter that checks if the checked value is smaller or equal than the given value
 */
static int smallereq_filter (const s4_val_t *value, s4_condition_t *cond)
{
	s4_val_t *d = cond->u.filter.funcdata;
	return s4_val_cmp (value, d, cond->u.filter.cmp_mode) > 0;
}
/*
 * A filter that checks if the checked value is greater or equal than the given value
 */
static int greatereq_filter (const s4_val_t *value, s4_condition_t* cond)
{
	s4_val_t *d = cond->u.filter.funcdata;
	return -(s4_val_cmp (value, d, cond->u.filter.cmp_mode) < 0);
}
/*
 * A filter that checks if the checked value is smaller than the given value
 */
static int smaller_filter (const s4_val_t *value, s4_condition_t *cond)
{
	s4_val_t *d = cond->u.filter.funcdata;
	return s4_val_cmp (value, d, cond->u.filter.cmp_mode) >= 0;
}
/*
 * A filter that checks if the checked value matches (glob-like pattern)
 * the given value
 */
static int match_filter (const s4_val_t *value, s4_condition_t *cond)
{
	s4_pattern_t *p = cond->u.filter.funcdata;
	return !s4_pattern_match (p, value);
}

/* A token filter. Checks if the passed value contains a token */
static int token_filter (const s4_val_t *value, s4_condition_t *cond)
{
	const char *token = cond->u.filter.funcdata;
	const char *s;
	int32_t i;
	s4_cmp_mode_t mode = cond->u.filter.cmp_mode;

	if ((mode == S4_CMP_CASELESS && s4_val_get_casefolded_str (value, &s)) ||
			s4_val_get_str (value, &s)) {
		while (*s) {
			/* Skip whitespaces */
			for (; isspace (*s); s++);

			/* Compare token */
			for (i = 0; *s && *s == token[i] && token[i] != '*'; i++, s++);

			/* Check if it matched */
			if (token[i] == '*' || (!token[i] && (isspace (*s) || !*s))) {
				return 0;
			}

			/* Eat the rest of the token */
			for (; *s && !isspace (*s); s++);
		}
	} else if (s4_val_get_int (value, &i)) {
		char *end;
		int32_t j = strtol (token, &end, 10);

		if (end != token) {
			/* If the token is just a number we have to have an exact match */
			if (*end == '\0' && j == i) {
				return 0;
			/* If the character after the last digit is a star we have to
			 * shift the number until it has the right number of digits
			 */
			} else if (*end == '*') {
				for (; i > j; i /= 10);
				if (i == j) {
					return 0;
				}
			}
		}
	}

	return 1;
}

/*
 * Sets the correct function and function data based on filter type
 */
static void _set_filter_function (s4_condition_t *cond, s4_filter_type_t type, s4_val_t *val)
{
	switch (type) {
		case S4_FILTER_EQUAL:
			cond->u.filter.func = equal_filter;
			cond->u.filter.funcdata = s4_val_copy (val);
			cond->u.filter.free_func = (free_func_t)s4_val_free;
			cond->u.filter.monotonic = 1;
			break;
		case S4_FILTER_NOTEQUAL:
			cond->u.filter.func = notequal_filter;
			cond->u.filter.funcdata = s4_val_copy (val);
			cond->u.filter.free_func = (free_func_t)s4_val_free;
			cond->u.filter.monotonic = 0;
			break;
		case S4_FILTER_GREATER:
			cond->u.filter.func = greater_filter;
			cond->u.filter.funcdata = s4_val_copy (val);
			cond->u.filter.free_func = (free_func_t)s4_val_free;
			cond->u.filter.monotonic = 1;
			break;
		case S4_FILTER_SMALLER:
			cond->u.filter.func = smaller_filter;
			cond->u.filter.funcdata = s4_val_copy (val);
			cond->u.filter.free_func = (free_func_t)s4_val_free;
			cond->u.filter.monotonic = 1;
			break;
		case S4_FILTER_GREATEREQ:
			cond->u.filter.func = greatereq_filter;
			cond->u.filter.funcdata = s4_val_copy (val);
			cond->u.filter.free_func = (free_func_t)s4_val_free;
			cond->u.filter.monotonic = 1;
			break;
		case S4_FILTER_SMALLEREQ:
			cond->u.filter.func = smallereq_filter;
			cond->u.filter.funcdata = s4_val_copy (val);
			cond->u.filter.free_func = (free_func_t)s4_val_free;
			cond->u.filter.monotonic = 1;
			break;
		case S4_FILTER_MATCH:
			{
				const char *s;
				int32_t i;
				char c[12];

				if (!s4_val_get_str (val, &s)) {
					s4_val_get_int (val, &i);
					sprintf (c, "%i", i);
					s = c;
				}

				cond->u.filter.func = match_filter;
				cond->u.filter.funcdata = s4_pattern_create (s, cond->u.filter.cmp_mode == S4_CMP_CASELESS);
				cond->u.filter.free_func = (free_func_t)s4_pattern_free;
				cond->u.filter.monotonic = 0;
			}
			break;
		case S4_FILTER_EXISTS:
			cond->u.filter.func = (filter_function_t)everything;
			cond->u.filter.funcdata = NULL;
			cond->u.filter.free_func = NULL;
			cond->u.filter.monotonic = 1;
			break;
		case S4_FILTER_TOKEN:
			{
				const char *str;
				char *s;
				int32_t i;

				if (s4_val_get_int (val, &i)) {
					s = g_strdup_printf ("%i", i);
				} else {
					switch (cond->u.filter.cmp_mode) {
					case S4_CMP_COLLATE:
						/* Collated token matching makes no sense,
						 * we use binary matching instead
						 */
					case S4_CMP_BINARY:
						s4_val_get_str (val, &str);
						break;
					case S4_CMP_CASELESS:
						s4_val_get_casefolded_str (val, &str);
						break;
					}
					s = strdup (str);
				}

				cond->u.filter.func = token_filter;
				cond->u.filter.funcdata = s;
				cond->u.filter.free_func = (free_func_t)g_free;
				cond->u.filter.monotonic = 0;
			}
			break;
		default: /* Unknown filter type */
			cond->u.filter.func = (filter_function_t)never;
			cond->u.filter.funcdata = NULL;
			cond->u.filter.free_func = NULL;
			cond->u.filter.monotonic = 0;
			break;
	}
}

/**
 * Creates a new combiner.
 *
 * @param type The combiner type
 * @return A new combiner
 */
s4_condition_t *s4_cond_new_combiner (s4_combine_type_t type)
{
	s4_condition_t *cond = malloc (sizeof (s4_condition_t));

	cond->type = S4_COND_COMBINER;
	cond->u.combine.type = type;
	cond->ref_count = 1;
	cond->u.combine.operands = g_ptr_array_new_with_free_func ((GDestroyNotify)s4_cond_unref);
	cond->u.combine.func = _get_combine_function (type);

	return cond;
}

/**
 * Creates a new combiner with a user specified combiner function.
 *
 * @param func The combiner function to use
 * @return A new custom combiner
 */
s4_condition_t *s4_cond_new_custom_combiner (combine_function_t func)
{
	s4_condition_t *cond = malloc (sizeof (s4_condition_t));

	cond->type = S4_COND_COMBINER;
	cond->u.combine.type = S4_COMBINE_CUSTOM;
	cond->ref_count = 1;
	cond->u.combine.operands = g_ptr_array_new_with_free_func ((GDestroyNotify)s4_cond_unref);
	cond->u.combine.func = func;

	return cond;
}

/**
 * Adds and references an operand to a combiner condition.
 *
 * @param cond The condition to add to
 * @param operand The operand to add
 */
void s4_cond_add_operand (s4_condition_t *cond, s4_condition_t *operand)
{
	if (cond->type == S4_COND_COMBINER) {
		g_ptr_array_add (cond->u.combine.operands, s4_cond_ref (operand));
	}
}

/**
 * Gets an operand from a combiner condition.
 *
 * @param cond The condition to get the operand from
 * @param operand The index of the operand to get
 * @return The operand, or NULL if the index is out of bounds
 * or cond is not a combiner condition. The reference is still
 * held by cond, so you must not call s4_cond_unref on the
 * returned condition.
 */
s4_condition_t *s4_cond_get_operand (s4_condition_t *cond, int operand)
{
	s4_condition_t *ret = NULL;

	if (cond->type == S4_COND_COMBINER
			&& operand >= 0
			&& operand < cond->u.combine.operands->len) {
		ret = g_ptr_array_index (cond->u.combine.operands, operand);
	}

	return ret;
}

/**
 * Creates a new filter condition
 *
 * @param type The type of the filter
 * @param key The key the condition should check
 * @param value The value to check against
 * @param sourcepref The source preference
 * @param cmp_mode The comparison mode to use
 * @param flags Condition flags, or 0
 * @return A new filter condition
 */
s4_condition_t *s4_cond_new_filter (s4_filter_type_t type, const char *key,
		s4_val_t *value, s4_sourcepref_t *sourcepref, s4_cmp_mode_t cmp_mode, int flags)
{
	s4_condition_t *cond = malloc (sizeof (s4_condition_t));

	cond->type = S4_COND_FILTER;
	cond->u.filter.type = type;
	cond->ref_count = 1;
	cond->u.filter.key = key==NULL?NULL:strdup (key);
	cond->u.filter.flags = flags;
	cond->u.filter.cmp_mode = cmp_mode;
	cond->u.filter.const_key = 0;

	if (sourcepref != NULL) {
		cond->u.filter.sp = s4_sourcepref_ref (sourcepref);
	} else {
		cond->u.filter.sp = NULL;
	}

	_set_filter_function (cond, type, value);

	return cond;
}

/**
 * Creates a new filter condition with a user specified filter function.
 * The filter function should return 0 if the value meets the condition,
 * non-zero otherwise.
 *
 * @param func The filter function to use
 * @param userdata The data that will be passed to the function
 * along with the value to check
 * @param free The function that should be called to free userdata
 * @param key The key the condition should check
 * @param sourcepref The source preference
 * @param cmp_mode The comparison mode to use
 * @param flags Condition flags, or 0
 * @return A new custom filter condition
 */
s4_condition_t *s4_cond_new_custom_filter (filter_function_t func, void *userdata,
		free_func_t free, const char *key, s4_sourcepref_t *sourcepref,
		s4_cmp_mode_t cmp_mode, int flags)
{
	s4_condition_t *cond = malloc (sizeof (s4_condition_t));

	cond->type = S4_COND_FILTER;
	cond->u.filter.type = S4_FILTER_CUSTOM;
	cond->ref_count = 1;
	cond->u.filter.key = key==NULL?NULL:strdup (key);
	cond->u.filter.flags = flags;
	cond->u.filter.func = func;
	cond->u.filter.funcdata = userdata;
	cond->u.filter.free_func = free;
	cond->u.filter.monotonic = 0;
	cond->u.filter.cmp_mode = cmp_mode;
	cond->u.filter.const_key = 0;

	if (sourcepref != NULL) {
		cond->u.filter.sp = s4_sourcepref_ref (sourcepref);
	} else {
		cond->u.filter.sp = NULL;
	}

	return cond;
}

/**
 * Checks if this condition is a filter condition
 *
 * @param cond The condition to check
 * @return non-zero if the condition is a filter, 0 otherwise
 */
int s4_cond_is_filter (s4_condition_t *cond)
{
	return cond->type ==  S4_COND_FILTER;
}

/**
 * Checks if this condition is a combiner condition
 *
 * @param cond The condition to check
 * @return non-zero if the condition is a combiner, 0 otherwise
 */
int s4_cond_is_combiner (s4_condition_t *cond)
{
	return cond->type == S4_COND_COMBINER;
}

s4_filter_type_t s4_cond_get_filter_type (s4_condition_t *cond)
{
	return cond->u.filter.type;
}

s4_combine_type_t s4_cond_get_combiner_type (s4_condition_t *cond)
{
	return cond->u.combine.type;
}

/**
 * Gets the flags for a condition
 *
 * @param cond The condition to get the flags of
 * @return The flags
 */
int s4_cond_get_flags (s4_condition_t *cond)
{
	return cond->u.filter.flags;
}

/**
 * Gets the key for a condition
 *
 * @param cond The condition to get the key of
 * @return The key
 */
const char *s4_cond_get_key (s4_condition_t *cond)
{
	return cond->u.filter.key;
}

/**
 * Gets the source preference that should be used
 *
 * @param cond The condition to get the source preference of
 * @return The source preference
 */
s4_sourcepref_t *s4_cond_get_sourcepref (s4_condition_t *cond)
{
	return cond->u.filter.sp;
}

/**
 * Returns the data that should be fed to the filter function
 *
 * @param cond The condition to get the function data of
 * @return The function data
 */
void *s4_cond_get_funcdata (s4_condition_t *cond)
{
	return cond->u.filter.funcdata;
}

/**
 * Frees a condition and operands recursively
 *
 * @param cond The condition to free
 */
void s4_cond_free (s4_condition_t *cond)
{
	if (cond->type == S4_COND_COMBINER) {
		g_ptr_array_free (cond->u.combine.operands, TRUE);
		free (cond);
	} else if (cond->type == S4_COND_FILTER) {
		if (cond->u.filter.free_func != NULL)
			cond->u.filter.free_func (cond->u.filter.funcdata);
		if (cond->u.filter.sp != NULL)
			s4_sourcepref_unref (cond->u.filter.sp);
		if (!cond->u.filter.const_key && cond->u.filter.key != NULL)
			free ((char*)cond->u.filter.key);
		free (cond);
	}
}

/**
 * Increments the reference count of a condition.
 * @param cond The condition to reference
 * @return The condition given
 */
s4_condition_t *s4_cond_ref (s4_condition_t *cond)
{
	if (cond != NULL)
		cond->ref_count++;
	return cond;
}

/**
 * Decrements the reference count of a condition. If the reference
 * count is 0 after this, the condition is freed.
 * @param cond The condition to decrement the count of
 */
void s4_cond_unref (s4_condition_t *cond)
{
	if (cond->ref_count <= 0) {
		S4_ERROR ("s4_cond_unref: ref_count <= 0");
		return;
	}
	cond->ref_count--;
	if (cond->ref_count == 0) {
		s4_cond_free (cond);
	}
}

/**
 * Gets the filter function for the condition. This function does not
 * check if the condition is actually a filter condition, if it is
 * called on a condition that is not a filter it will return bogus data
 *
 * @param cond The condition to get the filter function of
 * @return The filter function
 */
filter_function_t s4_cond_get_filter_function (s4_condition_t *cond)
{
	return cond->u.filter.func;
}

/**
 * Gets the combine function for the condition. This function does not
 * check if the condition is actually a combine condition, if it's
 * called on a condition that is not a combiner it will return bogus data
 *
 * @param cond The condition to get the combine function of
 * @return The combine function
 */
combine_function_t s4_cond_get_combine_function (s4_condition_t *cond)
{
	return cond->u.combine.func;
}

/**
 * Checks if the condition is a monotonic filter. A monotonic
 * filter is a filter that preserves the order, and it can
 * thus be used to search in an index.
 *
 * @param cond The condition to check
 * @return non-zero if the condition is monotonic, 0 otherwise
 */
int s4_cond_is_monotonic (s4_condition_t *cond)
{
	return cond->u.filter.monotonic;
}

/**
 * Gets the comparison mode used by the filter condition
 *
 * @param cond The condition to check
 * @return The comparison mode used
 */
int s4_cond_get_cmp_mode (s4_condition_t *cond)
{
	return cond->u.filter.cmp_mode;
}

/**
 * Change the key with a constant key for faster checking
 *
 * @param s4 The database to optimize for
 * @param cond The condition to update
 */
void s4_cond_update_key (s4_condition_t *cond, s4_t *s4)
{
	if (cond->type == S4_COND_COMBINER) {
		g_ptr_array_foreach (cond->u.combine.operands, (GFunc)s4_cond_update_key, s4);
	} else if (cond->type == S4_COND_FILTER) {
		const char *new_key = _string_lookup (s4, cond->u.filter.key);
		if (!cond->u.filter.const_key)
			free ((void*)cond->u.filter.key);
		cond->u.filter.key = new_key;
		cond->u.filter.const_key = 1;
	}
}

/**
 * @}
 */
