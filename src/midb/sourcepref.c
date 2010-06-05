#include "midb.h"
#include "s4_be.h"
#include <stdlib.h>

GList *sources = NULL;
GHashTable *sp = NULL;
GStaticMutex sp_lock = G_STATIC_MUTEX_INIT;

const char *srcprefs[] = {"plugin/test", "server", "plugin/*", "*", NULL};

static int _int_comp (gconstpointer a, gconstpointer b)
{
	return *(int32_t*)b - *(int32_t*)a;
}

void sp_add (s4be_t *be, int32_t src)
{
	if (g_list_find_custom (sources, &src, _int_comp) == NULL) {
		int32_t *i = malloc (sizeof (int32_t));
		*i = src;
		sources = g_list_prepend (sources, i);
	}

	if (sp != NULL) {
		g_static_mutex_lock (&sp_lock);
		g_hash_table_destroy (sp);
		sp = sp_create (be, srcprefs);
		g_static_mutex_unlock (&sp_lock);
	}
}

GHashTable *sp_create (s4be_t *be, const char *srcprefs[])
{
	GHashTable *ret = g_hash_table_new_full (g_int_hash, g_int_equal, NULL, free);
	int i = 0;
	for (; *srcprefs != NULL; srcprefs++) {
		GPatternSpec *spec = g_pattern_spec_new (*srcprefs);
		GList *list = sources;

		for (;list != NULL; list = g_list_next (list)) {
			char *str = s4be_st_reverse (be, *(int32_t*)list->data);

			if (g_pattern_match_string (spec, str)) {
				if (g_hash_table_lookup (ret, list->data) == NULL) {
					int *ptr = malloc (sizeof (int));
					*ptr = i++;
					g_hash_table_insert (ret, list->data, ptr);
				}
			}

			free (str);
		}

		g_pattern_spec_free (spec);
	}

	return ret;
}

int sp_get (s4be_t *be, int32_t src)
{
	g_static_mutex_lock (&sp_lock);
	if (sp == NULL)
		sp = sp_create (be, srcprefs);

	int *i = g_hash_table_lookup (sp, &src);
	g_static_mutex_unlock (&sp_lock);

	if (i == NULL)
		return INT_MAX;

	return *i;
}

void sp_free (GHashTable *h)
{
	g_hash_table_destroy (h);
}
