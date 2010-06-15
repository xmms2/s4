#include "s4_priv.h"
#include <stdlib.h>
#include <string.h>

struct s4_sourcepref_St {
	GHashTable *table;
	GStaticMutex lock;
	const char **prefs;
	s4_t *s4;
	int last_update;
};

static int src_id = 0;

void s4_add_source (s4_t *s4, const char *src)
{
	g_static_mutex_lock (&s4->src_list_lock);

	if (g_list_find_custom (s4->src_list, src, g_str_equal) == NULL) {
		s4->src_list = g_list_prepend (s4->src_list, strdup (src));
		src_id++;
	}

	g_static_mutex_unlock (&s4->src_list_lock);
}

static GHashTable *_create_table (s4_t *s4, const char **srcprefs)
{
	GHashTable *table = g_hash_table_new_full (g_int_hash, g_int_equal, free, free);

	g_static_mutex_lock (&s4->src_list_lock);

	GList *sources = s4->src_list;
	int i = 0;
	for (; *srcprefs != NULL; srcprefs++) {
		GPatternSpec *spec = g_pattern_spec_new (*srcprefs);
		GList *list = sources;

		for (;list != NULL; list = g_list_next (list)) {
			if (g_pattern_match_string (spec, list->data)) {
				int32_t id = _st_lookup (s4, list->data);
				if (g_hash_table_lookup (table, &id) == NULL) {
					int32_t *key = malloc (sizeof (int32_t));
					int *ptr = malloc (sizeof (int));
					*ptr = i++;
					*key = id;
					g_hash_table_insert (table, key, ptr);
				}
			}
		}

		g_pattern_spec_free (spec);
	}

	g_static_mutex_unlock (&s4->src_list_lock);

	return table;
}

s4_sourcepref_t *s4_sourcepref_create (s4_t *s4, const char **srcprefs)
{
	s4_sourcepref_t *sp = malloc (sizeof (s4_sourcepref_t));
	sp->table = _create_table (s4, srcprefs);
	sp->s4 = s4;
	sp->last_update = src_id;
	/* TODO: Copy*/
	sp->prefs = srcprefs;
	g_static_mutex_init (&sp->lock);

	return sp;
}

void s4_sourcepref_free (s4_sourcepref_t *sp)
{
	g_hash_table_destroy (sp->table);
	g_static_mutex_free (&sp->lock);
	free (sp);
}

int s4_sourcepref_get_priority (s4_sourcepref_t *sp, int32_t src)
{
	g_static_mutex_lock (&sp->lock);

	if (sp->last_update != src_id) {
		g_hash_table_destroy (sp->table);
		sp->table = _create_table (sp->s4, sp->prefs);
		sp->last_update = src_id;
	}

	int *i = g_hash_table_lookup (sp->table, &src);
	g_static_mutex_unlock (&sp->lock);

	if (i == NULL)
		return INT_MAX;

	return *i;
}
