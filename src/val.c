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

s4_val_t *s4_val_new_string (const char *str)
{
	s4_val_t *val = malloc (sizeof (s4_val_t));
	val->type = S4_VAL_STR;
	val->v.str.s = strdup (str);
	val->v.str.n = NULL;

	return val;
}

s4_val_t *s4_val_new_internal_string (const char *str, const char *normalized_str)
{
	s4_val_t *val = malloc (sizeof (s4_val_t));
	val->type = S4_VAL_STR_INTERNAL;
	val->v.str.s = str;
	val->v.str.n = normalized_str;

	return val;
}

s4_val_t *s4_val_new_int (int32_t i)
{
	s4_val_t *val = malloc (sizeof (s4_val_t));
	val->type = S4_VAL_INT;
	val->v.i = i;

	return val;
}

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

void s4_val_free (s4_val_t *val)
{
	if (val->type == S4_VAL_STR) {
		free ((void*)val->v.str.s);
		if (val->v.str.n != NULL)
			g_free ((void*)val->v.str.n);
	}
	free (val);
}

int s4_val_is_str (const s4_val_t *val)
{
	return val->type == S4_VAL_STR || val->type == S4_VAL_STR_INTERNAL;
}

int s4_val_is_int (const s4_val_t *val)
{
	return val->type == S4_VAL_INT;
}

int s4_val_get_str (const s4_val_t *val, const char **str)
{
	if (!s4_val_is_str (val))
		return 0;

	*str = val->v.str.s;
	return 1;
}

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

int s4_val_get_int (const s4_val_t *val, int32_t *i)
{
	if (!s4_val_is_int (val))
		return 0;

	*i = val->v.i;
	return 1;
}

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
