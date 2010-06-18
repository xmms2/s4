#include <s4.h>
#include "s4_priv.h"
#include <stdlib.h>
#include <glib.h>

struct s4_resultset_St {
	int col_count;
	int row_count;

	GPtrArray *results;
};

s4_resultset_t *s4_resultset_create (int col_count)
{
	s4_resultset_t *ret = malloc (sizeof (s4_resultset_t));

	ret->col_count = col_count;
	ret->row_count = 0;

	ret->results = g_ptr_array_new ();

	return ret;
}

void s4_resultset_add_row (s4_resultset_t *set, s4_result_t **results)
{
	g_ptr_array_add (set->results, results);
	set->row_count++;
}

const s4_result_t *s4_resultset_get_result (const s4_resultset_t *set, int row, int col)
{
	s4_result_t **res;
	if (row >= set->row_count || row < 0 || col >= set->col_count || col < 0)
		return NULL;

	res = g_ptr_array_index (set->results, row);
	return res[col];
}

int s4_resultset_get_colcount (const s4_resultset_t *set)
{
	return set->col_count;
}

int s4_resultset_get_rowcount (const s4_resultset_t *set)
{
	return set->row_count;
}

void s4_resultset_free (s4_resultset_t *set)
{
	int i,j;

	for (i = 0; i < set->row_count; i++) {
		s4_result_t **results = g_ptr_array_index (set->results, i);
		for (j = 0; j < set->col_count; j++) {
			s4_result_t *prev,*res = results[j];

			while (res != NULL) {
				prev = res;
				res = (s4_result_t*)s4_result_next (res);
				s4_result_free (prev);
			}
		}

		free (results);
	}

	g_ptr_array_free (set->results, TRUE);
	free (set);
}
