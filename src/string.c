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

char *s4_normalize_string (const char *key)
{
	char *tmp = g_utf8_casefold (key, -1);
	char *ret = g_utf8_normalize (tmp, -1, G_NORMALIZE_DEFAULT);

	if (ret == NULL) {
		ret = tmp;
	} else {
		g_free (tmp);
	}

	return ret;
}


const char *_string_lookup_normalized (s4_t *s4, const char *str)
{
	const char *ret;

	g_static_mutex_lock (&s4->norm_lock);
	ret = g_hash_table_lookup (s4->norm_table, str);

	if (ret == NULL) {
		char *tmp = s4_normalize_string (str);
		ret = _string_lookup (s4, tmp);
		g_free (tmp);

		g_hash_table_insert (s4->norm_table, (void*)str, (void*)ret);
	}

	g_static_mutex_unlock (&s4->norm_lock);

	return ret;
}
