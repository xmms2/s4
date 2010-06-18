#include "s4_priv.h"
#include <stdlib.h>

typedef struct {
	const char *key;
	s4_sourcepref_t *pref;
} fetch_data_t;

struct s4_fetchspec_St {
	GArray *array;
};

s4_fetchspec_t *s4_fetchspec_create (void)
{
	s4_fetchspec_t *ret = malloc (sizeof (s4_fetchspec_t));
	ret->array = g_array_new (FALSE, FALSE, sizeof (fetch_data_t));

	return ret;
}

void s4_fetchspec_add (s4_fetchspec_t *spec, const char *key, s4_sourcepref_t *sourcepref)
{
	fetch_data_t data;
	data.key = key;
	data.pref = sourcepref;

	g_array_append_val (spec->array, data);
}

void s4_fetchspec_free (s4_fetchspec_t *spec)
{
	g_array_free (spec->array, TRUE);
	free (spec);
}

int s4_fetchspec_size (s4_fetchspec_t *spec)
{
	return spec->array->len;
}

const char *s4_fetchspec_get_key (s4_fetchspec_t *spec, int index)
{
	if (index < 0 || index >= spec->array->len)
		return NULL;

	return g_array_index (spec->array, fetch_data_t, index).key;
}

s4_sourcepref_t *s4_fetchspec_get_sourcepref (s4_fetchspec_t *spec, int index)
{
	if (index < 0 || index >= spec->array->len)
		return NULL;

	return g_array_index (spec->array, fetch_data_t, index).pref;
}
