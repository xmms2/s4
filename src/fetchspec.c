#include "s4_priv.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
	const char *key;
	s4_sourcepref_t *pref;
} fetch_data_t;

struct s4_fetchspec_St {
	GArray *array;
};

/**
 * @defgroup Fetchspec Fetch Specifications
 * @ingroup S4
 * @brief Specifies what to fetch in a query
 *
 * @{
 */

/**
 * Creates a new fetch specification
 *
 * @return A new fetchspec
 */
s4_fetchspec_t *s4_fetchspec_create (void)
{
	s4_fetchspec_t *ret = malloc (sizeof (s4_fetchspec_t));
	ret->array = g_array_new (FALSE, FALSE, sizeof (fetch_data_t));

	return ret;
}

/**
 * Adds something to fetch to the fetch specification. If key is NULL, it will fetch
 * everything in an entry
 *
 * @param s4 The database to lookup the key in (for faster fetching)
 * @param spec The specification to add to
 * @param key The key to fetch
 * @param sourcepref The sourcepref to use when deciding which key-value pair to fetch
 */
void s4_fetchspec_add (s4_fetchspec_t *spec, const char *key, s4_sourcepref_t *sourcepref)
{
	fetch_data_t data;
	data.key = key;
	data.pref = sourcepref;

	g_array_append_val (spec->array, data);
}

void s4_fetchspec_update_key (s4_t *s4, s4_fetchspec_t *spec)
{
	int i;
	for (i = 0; i < spec->array->len; i++)
		g_array_index (spec->array, fetch_data_t, i).key =
			_string_lookup (s4, g_array_index (spec->array, fetch_data_t, i).key);
}

/**
 * Frees a fetchspec
 * @param spec The fetchspec to free
 */
void s4_fetchspec_free (s4_fetchspec_t *spec)
{
	g_array_free (spec->array, TRUE);
	free (spec);
}

/**
 * Gets the size of the fetchspec
 * @param spec The fetchspec to find the size of
 * @return The size of spec
 */
int s4_fetchspec_size (s4_fetchspec_t *spec)
{
	return spec->array->len;
}

/**
 * Gets the key at a give index
 * @param spec The fetchspec to find the key in
 * @param index The index of the key
 * @return The key, or NULL if index is out of bounds
 */
const char *s4_fetchspec_get_key (s4_fetchspec_t *spec, int index)
{
	if (index < 0 || index >= spec->array->len)
		return NULL;

	return g_array_index (spec->array, fetch_data_t, index).key;
}

/**
 * Gets the sourcepref at a give index
 * @param spec The fetchspec to find the sourcepref in
 * @param index The index of the sourcepref
 * @return The key, or NULL if index is out of bounds
 */
s4_sourcepref_t *s4_fetchspec_get_sourcepref (s4_fetchspec_t *spec, int index)
{
	if (index < 0 || index >= spec->array->len)
		return NULL;

	return g_array_index (spec->array, fetch_data_t, index).pref;
}

/**
 * @}
 */
