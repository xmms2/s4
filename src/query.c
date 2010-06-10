#include <s4.h>
#include "s4_be.h"
#include <stdlib.h>
#include "query.h"


struct s4_query_St {
	int32_t *fetch;
	int fetch_size;
	s4_condition_t *cond;
};

static int ff_equal (int32_t val, void *data)
{
	return val == *(int32_t*)data;
}

static int ff_greater (int32_t val, void *data)
{
	return val > *(int32_t*)data;
}

static int ff_smaller (int32_t val, void *data)
{
	return val < *(int32_t*)data;
}

static int ff_match (int32_t val, void *data)
{
	return 0;
}

static void set_search_function (s4_t *s4, s4_condition_t *a, s4_query_condition_t *b)
{
	switch (b->type) {
		case S4_COND_EQUAL:
			a->cond.filter.func = ff_equal;
			a->cond.filter.funcdata = malloc (sizeof (int32_t));
			switch (b->cond.filter.value.type) {
				case S4_VAL_INT:
					*(int32_t*)a->cond.filter.funcdata = b->cond.filter.value.val.i;
					break;
				case S4_VAL_STR:
					*(int32_t*)a->cond.filter.funcdata = s4be_st_lookup (s4->be, b->cond.filter.value.val.s);
					break;
			}
			break;
		case S4_COND_GREATER:
			a->cond.filter.func = ff_greater;
			a->cond.filter.funcdata = malloc (sizeof (int32_t));
			*(int32_t*)a->cond.filter.funcdata = b->cond.filter.value.val.i;
			break;
		case S4_COND_SMALLER:
			a->cond.filter.func = ff_smaller;
			a->cond.filter.funcdata = malloc (sizeof (int32_t));
			*(int32_t*)a->cond.filter.funcdata = b->cond.filter.value.val.i;
			break;
		case S4_COND_MATCH:
			a->cond.filter.func = ff_match;
			a->cond.filter.funcdata = strdup (b->cond.filter.value.val.s);
			break;
	}
}

static s4_condition_t *new_condition (s4_t *s4, s4_query_condition_t *cond)
{
	s4_condition_t *ret = malloc (sizeof (s4_condition_t));
	int i, operand_count;

	ret->type = cond->type;

	switch (cond->type) {
		case S4_COND_UNION:
		case S4_COND_INTERSECTION:
		case S4_COND_COMPLEMENT:
			for (operand_count = 0; cond->cond.operands[operand_count] != NULL; operand_count++);
			ret->cond.operands = malloc (sizeof (s4_condition_t*) * (operand_count + 1));
			for (i = 0; i < operand_count; i++) {
				ret->cond.operands[i] = new_condition (s4, cond->cond.operands[i]);
			}
			ret->cond.operands[i] = NULL;
			break;

		case S4_COND_EQUAL:
		case S4_COND_GREATER:
		case S4_COND_SMALLER:
		case S4_COND_MATCH:
			ret->cond.filter.key = s4be_st_lookup (s4->be, cond->cond.filter.key);
			set_search_function (s4, ret, cond);
//			ret->cond.filter.sourcepref = sp_create (s4, cond->cond.filter.sourcepref);
			break;
	}

	return ret;
}

s4_query_t *s4_query_new (s4_t *s4, const char **fetch, s4_query_condition_t *cond)
{
	s4_query_t *ret = malloc (sizeof (s4_query_t));
	int i, fetch_size;

	for (fetch_size = 0; fetch[fetch_size] != NULL; fetch_size++);

	ret->fetch = malloc (sizeof (int32_t) * fetch_size);
	ret->fetch_size = fetch_size;

	for (i = 0; i < fetch_size; i++) {
		ret->fetch[i] = s4be_st_lookup (s4->be, fetch[i]);
	}


	ret->cond = new_condition (s4, cond);


	return ret;
}

GList *s4_query_run (s4_t *s4, s4_query_t *query)
{
	return s4be_ip_query (s4->be, query->fetch, query->fetch_size, query->cond);
}

static void free_cond (s4_condition_t *cond)
{
	switch (cond->type) {
		case S4_COND_UNION:
		case S4_COND_INTERSECTION:
		case S4_COND_COMPLEMENT:
			free (cond->cond.operands);
			break;

		case S4_COND_EQUAL:
		case S4_COND_GREATER:
		case S4_COND_SMALLER:
		case S4_COND_MATCH:
			free (cond->cond.filter.funcdata);
			break;
	}

	free (cond);
}

void s4_query_free (s4_query_t *query)
{
	free_cond (query->cond);
	free (query->fetch);
}
