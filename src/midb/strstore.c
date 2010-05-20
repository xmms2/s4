/*  S4 - An XMMS2 medialib backend
 *  Copyright (C) 2009, 2010 Sivert Berg
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include <s4_be.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "midb.h"

typedef struct norm_str_St {
	GList *strings;
	char *str;
} norm_str_t;

struct str_St {
	norm_str_t *norm_str;
	char *str;
	int ref_count;
	int32_t id;
};

static int next_id = 0;

str_t *s4be_st_insert (s4be_t *be, int32_t new_id, char *string)
{
	str_t *str;
	norm_str_t *norm;
	int32_t *id;
	char *norm_str = s4be_st_normalize (string);

	g_static_mutex_lock (&be->norm_str_table_lock);

	norm = g_hash_table_lookup (be->norm_str_table, norm_str);

	if (norm == NULL) {
		norm = malloc (sizeof (norm_str_t));
		norm->str = norm_str;
		norm->strings = NULL;
		g_hash_table_insert (be->norm_str_table, norm_str, norm);
	} else {
		g_free (norm_str);
	}

	str = malloc (sizeof (str_t));
	str->str = string;
	str->ref_count = 0;
	str->id = new_id;
	str->norm_str = norm;

	norm->strings = g_list_prepend (norm->strings, str);
	g_static_mutex_unlock (&be->norm_str_table_lock);

	g_hash_table_insert (be->str_table, str->str, str);

	g_static_mutex_lock (&be->id_str_table_lock);
	id = malloc (sizeof (int32_t));
	*id = str->id;
	g_hash_table_insert (be->id_str_table, id, str);
	g_static_mutex_unlock (&be->id_str_table_lock);

	midb_log_string_insert (be, new_id, string);

	return str;
}

int s4be_st_ref (s4be_t *be, const char *string)
{
	str_t *str;

	g_static_mutex_lock (&be->str_table_lock);

	str = g_hash_table_lookup (be->str_table, string);
	if (str == NULL) {
		str = s4be_st_insert (be, ++next_id, strdup (string));
	}
	g_static_mutex_unlock (&be->str_table_lock);

	str->ref_count++;

	return str->id;;
}

int s4be_st_ref_id (s4be_t *be, int32_t id)
{
	str_t *str;

	g_static_mutex_lock (&be->id_str_table_lock);
	str = g_hash_table_lookup (be->id_str_table, &id);
	if (str == NULL) {
		return -1;
		g_static_mutex_unlock (&be->id_str_table_lock);
	}
	else str->ref_count++;

	g_static_mutex_unlock (&be->id_str_table_lock);

	return str->ref_count;
}

int s4be_st_unref (s4be_t *be, const char *string)
{
	str_t *str;
	int ret;

	g_static_mutex_lock (&be->str_table_lock);

	str = g_hash_table_lookup (be->str_table, string);
	if (str == NULL) {
		ret = -1;
	} else {
		ret = --str->ref_count;
	}
	g_static_mutex_unlock (&be->str_table_lock);

	return ret;
}

int32_t s4be_st_lookup (s4be_t *be, const char *string)
{
	str_t *str;
	int32_t ret;
	g_static_mutex_lock (&be->str_table_lock);

	str = g_hash_table_lookup (be->str_table, string);
	if (str == NULL) {
		ret = 0;
	} else {
		ret = str->id;
	}
	g_static_mutex_unlock (&be->str_table_lock);

	return ret;
}

int32_t *s4be_st_lookup_all (s4be_t *be, const char *str)
{
	int32_t *ret;
	norm_str_t *norm;
	GList *list;
	char *norm_str = s4be_st_normalize (str);
	int i;

	g_static_mutex_lock (&be->norm_str_table_lock);
	norm = g_hash_table_lookup (be->norm_str_table, norm_str);
	g_static_mutex_unlock (&be->norm_str_table_lock);

	g_free (norm_str);

	if (norm == NULL)
		return NULL;

	ret = malloc (sizeof (int32_t) * (g_list_length (norm->strings) + 1));
	for (i = 0, list = norm->strings;
			list != NULL;
			list = g_list_next (list), i++) {
		ret [i] = ((str_t*)list->data)->id;
	}
	ret[i] = -1;

	return ret;
}

int s4be_st_get_refcount (s4be_t *be, int32_t node)
{
	return 0;
}
int s4be_st_set_refcount (s4be_t *be, int32_t node, int refcount)
{
	return 0;
}

int s4be_st_remove (s4be_t *be, const char* str)
{
	return -1;
}

char *s4be_st_reverse (s4be_t *be, int str_id)
{
	str_t *str;
	g_static_mutex_lock (&be->id_str_table_lock);
	str = g_hash_table_lookup (be->id_str_table, &str_id);
	g_static_mutex_unlock (&be->id_str_table_lock);

	if (str == NULL)
		return NULL;

	return strdup (str->str);
}

char *s4be_st_reverse_normalized (s4be_t *be, int str_id)
{
	str_t *str;
	g_static_mutex_lock (&be->id_str_table_lock);
	str = g_hash_table_lookup (be->id_str_table, &str_id);
	g_static_mutex_unlock (&be->id_str_table_lock);

	if (str == NULL)
		return NULL;

	return strdup (str->norm_str->str);
}

char *s4be_st_normalize (const char *key)
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

void s4be_st_foreach (s4be_t *be,
		void (*func) (int32_t node, const char *str, void *userdata),
		void *userdata)
{
	GHashTableIter iter;
	str_t *str;

	g_static_mutex_lock (&be->id_str_table_lock);
	g_hash_table_iter_init (&iter, be->str_table);

	while (g_hash_table_iter_next (&iter, NULL, (gpointer*)&str)) {
		if (str->ref_count > 0)
			func (str->id, str->str, userdata);
	}

	g_static_mutex_unlock (&be->id_str_table_lock);
}
