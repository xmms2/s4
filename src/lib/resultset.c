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
#include "logging.h"
#include <stdlib.h>
#include <glib.h>

struct s4_resultset_St {
	int col_count;
	int row_count;
	int ref_count;

	GPtrArray *results;
};

struct s4_resultrow_St {
	int refs;
	int col_count;
	s4_result_t *cols[0];
};

struct s4_order_entry_St {
	enum {
		ORDER_TYPE_COLUMN,
		ORDER_TYPE_RANDOM
	} type;

	/* order by columns */
	int *columns;
	s4_order_direction_t direction;
	s4_cmp_mode_t collation;
	int size;

	/* order by random */
	GRand *random;
};

struct s4_order_St {
	s4_order_entry_t *columns;
	int size;
};

#define ORDER_RAND_MIN -1 /* closed bound */
#define ORDER_RAND_MAX 2 /* open bound */

/**
 * @defgroup ResultSet Result Set
 * @ingroup S4
 * @brief A set of results
 *
 * @{
 */

/**
 * @{
 * @internal
 */

/**
 * Creates a new resultset
 *
 * @param col_count The number of columns in every result
 * @return A new resultset with the column count set to col_count
 */
s4_resultset_t *s4_resultset_create (int col_count)
{
	s4_resultset_t *ret = malloc (sizeof (s4_resultset_t));

	ret->ref_count = 1;
	ret->col_count = col_count;
	ret->row_count = 0;

	ret->results = g_ptr_array_new_with_free_func ((GDestroyNotify)s4_resultrow_unref);

	return ret;
}

/**
 * @}
 */

/**
 * Adds a row to a resultset
 * @param set The resultset to add to
 * @param row The row to add
 */
void s4_resultset_add_row (s4_resultset_t *set, const s4_resultrow_t *row)
{
	s4_resultrow_ref ((s4_resultrow_t*)row);
	g_ptr_array_add (set->results, (void*)row);
	set->row_count++;
}

/**
 * Gets a row from a resultset
 * @param set The resultset to get the row from
 * @param row_no The index of the row to fetch
 * @param row A pointer to where the row will be saved
 * @return 0 if row_no was out of bounds, 1 otherwise
 */
int s4_resultset_get_row (const s4_resultset_t *set, int row_no, const s4_resultrow_t **row)
{
	if (row_no < 0 || row_no >= set->row_count)
		return 0;

	*row = g_ptr_array_index (set->results, row_no);
	return 1;
}

/**
 * Gets a result from a resultset
 * @param set The set to get the result from
 * @param row The row
 * @param col The column
 * @return The result at (row,col), or NULL if it does not exist
 */
const s4_result_t *s4_resultset_get_result (const s4_resultset_t *set, int row, int col)
{
	s4_resultrow_t *r;

	if (row >= set->row_count || row < 0 || col >= set->col_count || col < 0)
		return NULL;

	r = g_ptr_array_index (set->results, row);
	return r->cols[col];
}

/**
 * Gets the column count for a resultset
 * @param set The set to find the column count of
 * @return The column count
 */
int s4_resultset_get_colcount (const s4_resultset_t *set)
{
	return set->col_count;
}

/**
 * Gets the row count for a resultset
 * @param set The set to find the row count of
 * @return The row count
 */
int s4_resultset_get_rowcount (const s4_resultset_t *set)
{
	return set->row_count;
}

static int _compare_rows (const s4_resultrow_t **row1, const s4_resultrow_t **row2, s4_order_t *order)
{
	int i, j, ret = 0;
	for (i = 0; !ret && i < order->size; i++) {
		s4_order_entry_t *entry = &order->columns[i];
		if (entry->type == ORDER_TYPE_COLUMN) {
			const s4_val_t *val1 = NULL;
			const s4_val_t *val2 = NULL;

			for (j = 0; j < entry->size; j++) {
				int col = entry->columns[j];
				if ((*row1)->cols[col] != NULL) {
					val1 = s4_result_get_val ((*row1)->cols[col]);
					break;
				}
			}

			for (j = 0; j < entry->size; j++) {
				int col = entry->columns[j];
				if ((*row2)->cols[col] != NULL) {
					val2 = s4_result_get_val ((*row2)->cols[col]);
					break;
				}
			}

			if (val1 == NULL || val2 == NULL) {
				if (val1 == NULL)
					ret--;
				if (val2 == NULL)
					ret++;
			} else {
				ret = s4_val_cmp (val1, val2, entry->collation);
			}

			if (entry->direction == S4_ORDER_DESCENDING)
				ret = -ret;
		} else {
			ret = g_rand_int_range (entry->random, ORDER_RAND_MIN, ORDER_RAND_MAX);
		}
	}

	return (ret == 0) ? (row1 - row2) : ret;
}

/**
 * Sorts a resultset.
 * @param set The set to sort
 * @param order The columns to order the result by
 */
void s4_resultset_sort (s4_resultset_t *set, s4_order_t *order)
{
	if (order->size > 0) {
		g_ptr_array_sort_with_data (set->results, (GCompareDataFunc)_compare_rows, (void*)order);
	}
}

/**
 * Shuffles the resultset into a pseudo-random order
 * @param set The resultset to shuffle
 */
void s4_resultset_shuffle (s4_resultset_t *set)
{
	g_ptr_array_sort (set->results, (GCompareFunc)g_random_int);
}

/**
 * Frees a resultset and all the results in it
 * @param set The set to free
 */
void s4_resultset_free (s4_resultset_t *set)
{
	g_ptr_array_free (set->results, TRUE);
	free (set);
}

s4_resultset_t *s4_resultset_ref (s4_resultset_t *set)
{
	if (set != NULL)
		set->ref_count++;
	return set;

}
void s4_resultset_unref (s4_resultset_t *set)
{
	if (set->ref_count <= 0) {
		S4_ERROR ("s4_resultset_unref: ref_count <= 0");
		return;
	}
	set->ref_count--;
	if (set->ref_count == 0)
		s4_resultset_free (set);
}

/**
 * Creates a new row
 * @param col_count The number of columns in the row
 * @return A new resultrow
 */
s4_resultrow_t *s4_resultrow_create (int col_count)
{
	int i;
	s4_resultrow_t *row = malloc (sizeof (s4_resultrow_t)
			+ sizeof (s4_result_t*) * col_count);

	for (i = 0; i < col_count; i++) {
		row->cols[i] = NULL;
	}
	row->refs = 0;
	row->col_count = col_count;

	return row;
}

/**
 * Sets a column in a resultrow
 * @param row The row to set the column in
 * @param col_no The index of the column to set
 * @param col The new value of the column
 */
void s4_resultrow_set_col (s4_resultrow_t *row, int col_no, s4_result_t *col)
{
	if (col_no < 0 || col_no >= row->col_count)
		return;

	row->cols[col_no] = col;
}

/**
 * Gets the value of a column in a resultrow
 * @param row The row to get the column from
 * @param col_no The index of the column to get
 * @param col A pointer to where the column will be saved
 * @return 0 if col_no is out of bounds or the column is NULL, 1 otherwise
 */
int s4_resultrow_get_col (const s4_resultrow_t *row, int col_no, const s4_result_t **col)
{
	if (col_no < 0 || col_no >= row->col_count || row->cols[col_no] == NULL)
		return 0;

	*col = row->cols[col_no];
	return 1;
}

/**
 * References a resultrow
 * @param row The row to reference
 */
s4_resultrow_t *s4_resultrow_ref (s4_resultrow_t *row)
{
	if (row != NULL)
		row->refs++;
	return row;
}

/**
 * Unreferences a resultrow. If the refcount hits 0 the row will be freed
 * @param row The row to unrefperence
 */
void s4_resultrow_unref (s4_resultrow_t *row)
{
	if (row->refs <= 0) {
		S4_ERROR ("s4_resultrow_unref: ref_count <= 0");
		return;
	}
	row->refs--;

	if (row->refs <= 0) {
		int i;
		for (i = 0; i < row->col_count; i++) {
			if (row->cols[i] != NULL) {
				s4_result_t *prev,*res = row->cols[i];
				while (res != NULL) {
					prev = res;
					res = (s4_result_t*)s4_result_next (res);
					s4_result_free (prev);
				}
			}
		}
		free (row);
	}
}

static s4_order_entry_t *_s4_order_entry_init (s4_order_entry_t *entry,
                                               s4_cmp_mode_t collation,
                                               s4_order_direction_t direction,
                                               GRand *random)
{
	entry->type = (random == NULL) ? ORDER_TYPE_COLUMN : ORDER_TYPE_RANDOM;
	entry->columns = NULL;
	entry->random = random;
	entry->direction = direction;
	entry->collation = collation;
	entry->size = 0;
	return entry;
}

s4_order_t *s4_order_create (void)
{
	s4_order_t *order = malloc (sizeof(s4_order_t));
	order->size = 0;
	order->columns = NULL;
	return order;
}

s4_order_entry_t *s4_order_add_column (s4_order_t *order, s4_cmp_mode_t collation,
                                       s4_order_direction_t direction)
{
	order->size += 1;
	order->columns = realloc (order->columns,
	                          sizeof (s4_order_entry_t) * order->size);
	return _s4_order_entry_init (&order->columns[order->size - 1],
	                             collation, direction, NULL);
}

s4_order_entry_t *s4_order_add_random (s4_order_t *order, int seed)
{
	order->size += 1;
	order->columns = realloc (order->columns,
	                          sizeof (s4_order_entry_t) * order->size);
	return _s4_order_entry_init (&order->columns[order->size - 1],
	                             0, 0, g_rand_new_with_seed (seed));
}

void s4_order_entry_add_choice (s4_order_entry_t *entry, int column)
{
	entry->size += 1;
	entry->columns = realloc (entry->columns,
	                          sizeof (int) * entry->size);
	entry->columns[entry->size - 1] = column;
}

void s4_order_free (s4_order_t *order)
{
	int i;
	for (i = 0; i < order->size; i++) {
		if (order->columns[i].columns  != NULL)
			free (order->columns[i].columns);
		if (order->columns[i].random != NULL)
			g_rand_free (order->columns[i].random);
	}
	free (order->columns);
	free (order);
}

/**
 * @}
 */
