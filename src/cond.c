#include <s4.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	S4_COND_COMBINER,
	S4_COND_FILTER,
} s4_cond_type_t;

struct s4_condition_St {
	s4_cond_type_t type;
	union {
		struct {
			combine_function_t func;
			GList *operands;
		} combine;
		struct {
			filter_function_t func;
			void *funcdata;
			free_func_t free_func;
			const char *key;
			s4_sourcepref_t *sp;
			int flags;
			int continuous;
		} filter;
	} u;

	int32_t ikey;
};

static int never (void)
{
	return 1;
}
static int always (void)
{
	return 0;
}

static int or_combiner (s4_condition_t *cond, check_function_t func, void *check_data)
{
	int ret = 1;
	GList *i = cond->u.combine.operands;
	for (;ret && i != NULL; i = g_list_next (i)) {
		ret = func (i->data, check_data);
	}

	return ret;
}

static int and_combiner (s4_condition_t *cond, check_function_t func, void *check_data)
{
	int ret = 0;
	GList *i = cond->u.combine.operands;
	for (;!ret && i != NULL; i = g_list_next (i)) {
		ret = func (i->data, check_data);
	}

	return ret;
}

static int not_combiner (s4_condition_t *cond, check_function_t func, void *check_data)
{
	return !func(cond->u.combine.operands->data, check_data);
}

static combine_function_t _get_combine_function (s4_combine_type_t type) {
	switch (type) {
		case S4_COMBINE_OR:
			return or_combiner;
		case S4_COMBINE_AND:
			return and_combiner;
		case S4_COMBINE_NOT:
			return not_combiner;
	}
	return (combine_function_t)never;
}

static int equal_filter (s4_val_t *value, s4_condition_t *cond)
{
	s4_val_t *d = cond->u.filter.funcdata;
	const char *s1,*s2;
	int32_t i1,i2;

	if (s4_val_get_int (value, &i1) && s4_val_get_int (d, &i2))
		return (i1 > i2)?1:((i1 < i2)?-1:0);
	if (s4_val_get_str (value, &s1) && s4_val_get_str (d, &s2))
		return strcmp (s1, s2);
	return 1;
}
static int greater_filter (s4_val_t *value, s4_condition_t* cond)
{
	s4_val_t *d = cond->u.filter.funcdata;
	int32_t i1,i2;

	if (s4_val_get_int (value, &i1) && s4_val_get_int (d, &i2))
		return i1 <= i2;
	return 0;
}
static int smaller_filter (s4_val_t *value, s4_condition_t *cond)
{
	s4_val_t *d = cond->u.filter.funcdata;
	int32_t i1,i2;

	if (s4_val_get_int (value, &i1) && s4_val_get_int (d, &i2))
		return i1 >= i2;
	return 0;
}

static int match_filter (s4_val_t *value, s4_condition_t *cond)
{
	GPatternSpec *spec = cond->u.filter.funcdata;
	const char *s;

	if (s4_val_get_str (value, &s)) {
		return !g_pattern_match_string (spec, s);
	}
	return 1;
}

static void _set_filter_function (s4_condition_t *cond, s4_filter_type_t type, s4_val_t *val)
{
	switch (type) {
		case S4_FILTER_EQUAL:
			cond->u.filter.func = equal_filter;
			cond->u.filter.funcdata = s4_val_copy (val);
			cond->u.filter.free_func = (free_func_t)s4_val_free;
			cond->u.filter.continuous = 1;
			break;
		case S4_FILTER_GREATER:
			cond->u.filter.func = greater_filter;
			cond->u.filter.funcdata = s4_val_copy (val);
			cond->u.filter.free_func = (free_func_t)s4_val_free;
			cond->u.filter.continuous = 1;
			break;
		case S4_FILTER_SMALLER:
			cond->u.filter.func = smaller_filter;
			cond->u.filter.funcdata = s4_val_copy (val);
			cond->u.filter.free_func = (free_func_t)s4_val_free;
			cond->u.filter.continuous = 1;
			break;
		case S4_FILTER_MATCH:
			{
				const char *s;
				if (s4_val_get_str (val, &s)) {
					cond->u.filter.func = match_filter;
					cond->u.filter.funcdata = g_pattern_spec_new (s);
					cond->u.filter.free_func = (free_func_t)g_pattern_spec_free;
				} else {
					cond->u.filter.func = (filter_function_t)never;
					cond->u.filter.funcdata = NULL;
					cond->u.filter.free_func = NULL;
				}
				cond->u.filter.continuous = 0;
			}
			break;
		case S4_FILTER_EXISTS:
			cond->u.filter.func = (filter_function_t)always;
			cond->u.filter.funcdata = NULL;
			cond->u.filter.free_func = NULL;
			cond->u.filter.continuous = 1;
			break;
	}
}

s4_condition_t *s4_cond_new_combiner (s4_combine_type_t type, GList *operands)
{
	s4_condition_t *cond = malloc (sizeof (s4_condition_t));

	cond->type = S4_COND_COMBINER;
	cond->u.combine.operands = operands;
	cond->u.combine.func = _get_combine_function (type);

	return cond;
}

s4_condition_t *s4_cond_new_custom_combiner (combine_function_t func, GList *operands)
{
	s4_condition_t *cond = malloc (sizeof (s4_condition_t));

	cond->type = S4_COND_COMBINER;
	cond->u.combine.operands = operands;
	cond->u.combine.func = func;

	return cond;
}


s4_condition_t *s4_cond_new_filter (s4_filter_type_t type,
		const char *key, s4_val_t *value, s4_sourcepref_t *sourcepref, int flags)
{
	s4_condition_t *cond = malloc (sizeof (s4_condition_t));

	cond->type = S4_COND_FILTER;
	cond->u.filter.key = key;
	cond->u.filter.sp = sourcepref;
	cond->u.filter.flags = flags;
	cond->ikey = 0;

	_set_filter_function (cond, type, value);

	return cond;
}

s4_condition_t *s4_cond_new_custom_filter (filter_function_t func, void *userdata,
		free_func_t free, const char *key, s4_sourcepref_t *sourcepref, int flags)
{
	s4_condition_t *cond = malloc (sizeof (s4_condition_t));

	cond->type = S4_COND_FILTER;
	cond->u.filter.key = key;
	cond->u.filter.sp = sourcepref;
	cond->u.filter.flags = flags;
	cond->u.filter.func = func;
	cond->u.filter.funcdata = userdata;
	cond->u.filter.free_func = free;
	cond->ikey = 0;

	return cond;
}

int s4_cond_is_filter (s4_condition_t *cond)
{
	return cond->type ==  S4_COND_FILTER;
}

int s4_cond_is_combiner (s4_condition_t *cond)
{
	return cond->type == S4_COND_COMBINER;
}

int s4_cond_get_flags (s4_condition_t *cond)
{
	return cond->u.filter.flags;
}

const char *s4_cond_get_key (s4_condition_t *cond)
{
	return cond->u.filter.key;
}

s4_sourcepref_t *s4_cond_get_sourcepref (s4_condition_t *cond)
{
	return cond->u.filter.sp;
}

void *s4_cond_get_funcdata (s4_condition_t *cond)
{
	return cond->u.filter.funcdata;
}

void s4_cond_free (s4_condition_t *cond)
{
	if (cond->type == S4_COND_COMBINER) {
		GList *i;
		for (i = cond->u.combine.operands; i != NULL; i = g_list_next (i))
			s4_cond_free (i->data);
		free (cond);
	} else if (cond->type == S4_COND_FILTER) {
		if (cond->u.filter.free_func != NULL)
			cond->u.filter.free_func (cond->u.filter.funcdata);
		free (cond);
	}
}

filter_function_t s4_cond_get_filter_function (s4_condition_t *cond)
{
	return cond->u.filter.func;
}

combine_function_t s4_cond_get_combine_function (s4_condition_t *cond)
{
	return cond->u.combine.func;
}

void s4_cond_set_ikey (s4_condition_t *cond, int32_t ikey)
{
	cond->ikey = ikey;
}

int32_t s4_cond_get_ikey (s4_condition_t *cond)
{
	return cond->ikey;
}

int s4_cond_is_continuous (s4_condition_t *cond)
{
	return cond->u.filter.continuous;
}
