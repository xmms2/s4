#include "s4_priv.h"
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
			int monotonic;
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
	int ret = 1;
	GList *i = cond->u.combine.operands;
	for (;ret && i != NULL; i = g_list_next (i)) {
		ret = func (i->data, check_data);
	}

	return ret;
}

/*
 * A shortcutting AND combiner. It returns as soon as one of the operands is not met
 */
static int and_combiner (s4_condition_t *cond, check_function_t func, void *check_data)
{
	int ret = 0;
	GList *i = cond->u.combine.operands;
	for (;!ret && i != NULL; i = g_list_next (i)) {
		ret = func (i->data, check_data);
	}

	return ret;
}

/*
 * A NOT combiner, it only takes the first operand into account
 */
static int not_combiner (s4_condition_t *cond, check_function_t func, void *check_data)
{
	return !func(cond->u.combine.operands->data, check_data);
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
	}
	return (combine_function_t)never;
}

/*
 * A filter that checks if the given and checked value is equal
 */
static int equal_filter (s4_val_t *value, s4_condition_t *cond)
{
	s4_val_t *d = cond->u.filter.funcdata;
	return s4_val_cmp (value, d, cond->u.filter.flags & S4_COND_CASESENS);
}
/*
 * A filter that checks if the checked value is greater than the given value
 */
static int greater_filter (s4_val_t *value, s4_condition_t* cond)
{
	s4_val_t *d = cond->u.filter.funcdata;
	return s4_val_cmp (value, d, cond->u.filter.flags & S4_COND_CASESENS) <= 0;
}
/*
 * A filter that checks if the checked value is smaller than the given value
 */
static int smaller_filter (s4_val_t *value, s4_condition_t *cond)
{
	s4_val_t *d = cond->u.filter.funcdata;
	return s4_val_cmp (value, d, cond->u.filter.flags & S4_COND_CASESENS) >= 0;
}
/*
 * A filter that checks if the checked value matches (glob-like pattern)
 * the given value
 */
static int match_filter (s4_val_t *value, s4_condition_t *cond)
{
	GPatternSpec *spec = cond->u.filter.funcdata;
	const char *s;

	if ((cond->u.filter.flags & S4_COND_CASESENS) && s4_val_get_str (value, &s)) {
		return !g_pattern_match_string (spec, s);
	} else if (!(cond->u.filter.flags & S4_COND_CASESENS) && s4_val_get_normalized_str (value, &s)) {
		return !g_pattern_match_string (spec, s);
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
		case S4_FILTER_MATCH:
			{
				const char *s;
				if ((cond->u.filter.flags & S4_COND_CASESENS) && s4_val_get_str (val, &s)) {
					cond->u.filter.func = match_filter;
					cond->u.filter.funcdata = g_pattern_spec_new (s);
					cond->u.filter.free_func = (free_func_t)g_pattern_spec_free;
				} else if (!(cond->u.filter.flags & S4_COND_CASESENS) && s4_val_get_normalized_str (val, &s)) {
					cond->u.filter.func = match_filter;
					cond->u.filter.funcdata = g_pattern_spec_new (s);
					cond->u.filter.free_func = (free_func_t)g_pattern_spec_free;
				} else {
					cond->u.filter.func = (filter_function_t)never;
					cond->u.filter.funcdata = NULL;
					cond->u.filter.free_func = NULL;
				}
				cond->u.filter.monotonic = 0;
			}
			break;
		case S4_FILTER_EXISTS:
			cond->u.filter.func = (filter_function_t)everything;
			cond->u.filter.funcdata = NULL;
			cond->u.filter.free_func = NULL;
			cond->u.filter.monotonic = 1;
			break;
	}
}

/**
 * Creates a new combiner.
 *
 * @param type The combiner type
 * @param operands The operands of the combiner
 * @return A new combiner
 */
s4_condition_t *s4_cond_new_combiner (s4_combine_type_t type, GList *operands)
{
	s4_condition_t *cond = malloc (sizeof (s4_condition_t));

	cond->type = S4_COND_COMBINER;
	cond->u.combine.operands = operands;
	cond->u.combine.func = _get_combine_function (type);

	return cond;
}

/**
 * Creates a new combiner with a user specified combiner function.
 *
 * @param func The combiner function to use
 * @param operands The operands to the combiner
 * @return A new custom combiner
 */
s4_condition_t *s4_cond_new_custom_combiner (combine_function_t func, GList *operands)
{
	s4_condition_t *cond = malloc (sizeof (s4_condition_t));

	cond->type = S4_COND_COMBINER;
	cond->u.combine.operands = operands;
	cond->u.combine.func = func;

	return cond;
}

/**
 * Creates a new filter condition
 *
 * @param type The type of the filter
 * @param key The key the condition should check
 * @param value The value to check against
 * @param sourcepref The source preference
 * @param flags Condition flags, or 0
 * @return A new filter condition
 */
s4_condition_t *s4_cond_new_filter (s4_filter_type_t type,
		const char *key, s4_val_t *value, s4_sourcepref_t *sourcepref, int flags)
{
	s4_condition_t *cond = malloc (sizeof (s4_condition_t));

	cond->type = S4_COND_FILTER;
	cond->u.filter.key = key;
	cond->u.filter.sp = sourcepref;
	cond->u.filter.flags = flags;

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
 * @param flags Condition flags, or 0
 * @return A new custom filter condition
 */
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
 * Change the key with a constant key for faster checking
 *
 * @param s4 The database to optimize for
 * @param cond The condition to update
 */
void s4_cond_update_key (s4_t *s4, s4_condition_t *cond)
{
	if (cond->type == S4_COND_COMBINER) {
		GList *i;
		for (i = cond->u.combine.operands; i != NULL; i = g_list_next (i))
			s4_cond_update_key (s4, i->data);
	} else if (cond->type == S4_COND_FILTER) {
		cond->u.filter.key = _string_lookup (s4, cond->u.filter.key);
	}
}

/**
 * @}
 */
