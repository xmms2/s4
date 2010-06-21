#include "s4_priv.h"

const char *_string_lookup (s4_t *s4, const char *str)
{
	const char *ret;

	if (str == NULL)
		return NULL;

	g_static_mutex_lock (&s4->strings_lock);
	ret = g_string_chunk_insert_const (s4->strings, str);
	g_static_mutex_unlock (&s4->strings_lock);

	return ret;
}
