#include <s4.h>
#include <string.h>
#include <stdlib.h>

typedef enum {
	S4_VAL_STR,
	S4_VAL_STR_NOCOPY,
	S4_VAL_INT
} s4_val_type_t;

struct s4_val_St {
	s4_val_type_t type;
	union {
		const char *s;
		int32_t i;
	} v;
};

s4_val_t *s4_val_new_string (const char *str)
{
	s4_val_t *val = malloc (sizeof (s4_val_t));
	val->type = S4_VAL_STR;
	val->v.s = strdup (str);

	return val;
}

s4_val_t *s4_val_new_string_nocopy (const char *str)
{
	s4_val_t *val = malloc (sizeof (s4_val_t));
	val->type = S4_VAL_STR_NOCOPY;
	val->v.s = str;

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
	switch (val->type) {
		case S4_VAL_STR:
			return s4_val_new_string (val->v.s);
		case S4_VAL_STR_NOCOPY:
			return s4_val_new_string_nocopy (val->v.s);
		case S4_VAL_INT:
			return s4_val_new_int (val->v.i);
	}
	return NULL;
}

void s4_val_free (s4_val_t *val)
{
	if (val->type == S4_VAL_STR)
		free ((void*)val->v.s);
	free (val);
}

int s4_val_is_str (const s4_val_t *val)
{
	return val->type == S4_VAL_STR || val->type == S4_VAL_STR_NOCOPY;
}

int s4_val_is_int (const s4_val_t *val)
{
	return val->type == S4_VAL_INT;
}

int s4_val_get_str (const s4_val_t *val, const char **str)
{
	if (!s4_val_is_str (val))
		return 0;

	*str = val->v.s;
	return 1;
}

int s4_val_get_int (const s4_val_t *val, int32_t *i)
{
	if (!s4_val_is_int (val))
		return 0;

	*i = val->v.i;
	return 1;
}

int s4_val_comp (const s4_val_t *v1, const s4_val_t *v2)
{
	int32_t i1,i2;
	const char *s1,*s2;

	if (s4_val_get_int (v1, &i1) && s4_val_get_int (v2, &i2))
		return (i1 > i2)?1:((i1 < i2)?-1:0);
	else if (s4_val_get_str (v1, &s1) && s4_val_get_str (v2, &s2))
		return strcmp (s1, s2);
	else if (s4_val_is_int (v1))
		return -1;
	else
		return 1;
}
