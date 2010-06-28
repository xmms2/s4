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

#include <s4.h>
#include "s4_priv.h"
#include <stdlib.h>

struct s4_result_St {
	s4_result_t *next;
	const char *key;
	s4_val_t *val;
	const char *src;
};

/**
 * @defgroup Result Result
 * @ingroup S4
 * @brief Handles results returned from S4
 *
 * @{
 */

/**
 * Gets the next result
 *
 * @param res The result to find the next of
 * @return The next result, or NULL if there are no more
 */
const s4_result_t *s4_result_next (const s4_result_t *res)
{
	return res->next;
}

/**
 * Gets the result key
 *
 * @param res The result to find the key of
 * @return The key of the result
 */
const char *s4_result_get_key (const s4_result_t *res)
{
	return res->key;
}

/**
 * Gets the result source
 *
 * @param res The result to find the source of
 * @return The source of the result
 */
const char *s4_result_get_src (const s4_result_t *res)
{
	return res->src;
}

/**
 * Gets the result value
 *
 * @param res The result to find the value of
 * @return The value of the result
 */
const s4_val_t *s4_result_get_val (const s4_result_t *res)
{
	return res->val;
}


/**
 * Frees a result
 *
 * @param res The result to free
 */
void s4_result_free (s4_result_t *res)
{
	free (res);
}

/**
 * @{
 * @internal
 */

/**
 * Creates a new result
 *
 * @param next The next result
 * @param key The key
 * @param val The value
 * @param src The source
 * @return A new result
 */
s4_result_t *s4_result_create (s4_result_t *next, const char *key, s4_val_t *val, const char *src)
{
	s4_result_t *ret = malloc (sizeof (s4_result_t));

	ret->next = next;
	ret->key = key;
	ret->val = val;
	ret->src = src;

	return ret;
}

/**
 * @}
 * @}
 */
