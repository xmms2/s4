#include "s4_priv.h"
#include <stdlib.h>
#include <string.h>

struct s4_sourcepref_St {
	GHashTable *table;
	GStaticMutex lock;
	GPatternSpec **specs;
	int spec_count;
	s4_t *s4;
};

static int _get_priority (s4_sourcepref_t *sp, const char *src)
{
	int i;
	for (i = 0; i < sp->spec_count; i++) {
		if (g_pattern_match_string (sp->specs[i], src)) {
			return i;
		}
	}

	return INT_MAX;
}

s4_sourcepref_t *s4_sourcepref_create (s4_t *s4, const char **srcprefs)
{
	int i;
	s4_sourcepref_t *sp = malloc (sizeof (s4_sourcepref_t));
	sp->table = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, free);
	sp->s4 = s4;
	g_static_mutex_init (&sp->lock);

	for (i = 0; srcprefs[i] != NULL; i++);

	sp->specs = malloc (sizeof (GPatternSpec*) * i);
	sp->spec_count = i;

	for (i = 0; i < sp->spec_count; i++)
		sp->specs[i] = g_pattern_spec_new (srcprefs[i]);

	return sp;
}

void s4_sourcepref_free (s4_sourcepref_t *sp)
{
	int i;
	g_hash_table_destroy (sp->table);
	g_static_mutex_free (&sp->lock);

	for (i = 0; i < sp->spec_count; i++)
		g_pattern_spec_free (sp->specs[i]);

	free (sp->specs);

	free (sp);
}

int s4_sourcepref_get_priority (s4_sourcepref_t *sp, const char *src)
{
	g_static_mutex_lock (&sp->lock);

	int *i = g_hash_table_lookup (sp->table, src);

	if (i == NULL) {
		int pri = _get_priority (sp, src);

		i = malloc (sizeof (int));
		*i = pri;
		g_hash_table_insert (sp->table, (void*)src, i);
		g_static_mutex_unlock (&sp->lock);

		return pri;
	}
	g_static_mutex_unlock (&sp->lock);

	return *i;
}
