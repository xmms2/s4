/*  S4 - An XMMS2 medialib backend
 *  Copyright (C) 2009 Sivert Berg
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

#include <s4.h>
#include "s4_priv.h"
#include "logging.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <errno.h>

static GStaticPrivate _errno = G_STATIC_PRIVATE_INIT;

/**
 *
 * @defgroup S4 S4
 * @brief A database backend for XMMS2
 *
 * @{
 */

#define S4_MAGIC ("s4db")
#define S4_MAGIC_LEN (strlen (S4_MAGIC))

/**
 * Read strings from a file
 *
 * @param s4 The database to add the strings to
 * @param file The file to read from
 * @return 0 on success, -1 otherwise
 */
static GHashTable *_read_string (s4_t *s4, FILE *file)
{
	size_t r;
	int32_t id, len;
	char *str;
	GHashTable *ret = g_hash_table_new (NULL, NULL);

	while ((r = fread (&id, sizeof (int32_t), 1, file)) == 1 &&
			id != -1 &&
			(r = fread (&len, sizeof (int32_t), 1, file)) == 1) {
		str = malloc (len + 1);
		r = fread (str, 1, len, file);

		if (r != len) {
			g_hash_table_destroy (ret);
			return NULL;
		}

		str[len] = '\0';
		g_hash_table_insert (ret, GINT_TO_POINTER (id), (void*)_string_lookup (s4, str));
		free (str);
	}

	if (r == 0) {
		g_hash_table_destroy (ret);
		return NULL;
	}

	return ret;
}

/**
 * Read relations from a file
 *
 * @param s4 The database to insert them into
 * @param file The file to read
 * @return -1 on error, 0 otherwise
 */
static int _read_relations (s4_t *s4, FILE *file, GHashTable *strings)
{
	s4_intpair_t rec;

	while (fread (&rec, sizeof (s4_intpair_t), 1, file) == 1) {
		const char *key_a, *key_b, *src;
		s4_val_t *val_a, *val_b;

		key_a = g_hash_table_lookup (strings, GINT_TO_POINTER (ABS (rec.key_a)));
		key_b = g_hash_table_lookup (strings, GINT_TO_POINTER (ABS (rec.key_b)));
		src = g_hash_table_lookup (strings, GINT_TO_POINTER (ABS (rec.src)));

		if (rec.key_a > 0) {
			val_a = s4_val_new_string (g_hash_table_lookup (strings, GINT_TO_POINTER (rec.val_a)));
		} else {
			val_a = s4_val_new_int (rec.val_a);
		}
		if (rec.key_b > 0) {
			val_b = s4_val_new_string (g_hash_table_lookup (strings, GINT_TO_POINTER (rec.val_b)));
		} else {
			val_b = s4_val_new_int (rec.val_b);
		}

		s4_add (s4, key_a, val_a, key_b, val_b, src);

		s4_val_free (val_a);
		s4_val_free (val_b);
	}

	return 0;
}

/**
 * Read an S4 database from filename.
 *
 * @param s4 The s4 database to read the data into
 * @param filename The name of the file to read from
 * @return 0 on success, non-zero on error
 */
static int _read_file (s4_t *s4, const char *filename, int flags)
{
	char magic[S4_MAGIC_LEN];
	FILE *file = fopen (filename, "r");

	if (file == NULL) {
		int ret = 0;
		switch (errno) {
			case ENOENT:
				if (flags & S4_EXISTS) {
					s4_set_errno (S4E_NOENT);
					ret = -1;
				}
				break;
			default:
				s4_set_errno (S4E_OPEN);
				ret = -1;
				break;
		}
		return ret;
	} else if (flags & S4_NEW) {
		fclose (file);
		s4_set_errno (S4E_EXISTS);
		return -1;
	}

	fread (magic, 1, S4_MAGIC_LEN, file);
	if (strncmp (S4_MAGIC, magic, S4_MAGIC_LEN)) {
		fclose (file);
		s4_set_errno (S4E_MAGIC);
		return -1;
	}

	GHashTable *strings = _read_string (s4, file);
	if (strings == NULL || _read_relations (s4, file, strings) == -1) {
		fclose (file);
		s4_set_errno (S4E_INCONS);
		return -1;
	}

	fclose (file);
	return 0;
}

/**
 * Free a midb handle and everything it points at
 *
 * @param s4 Fhe handle to free
 */
static void _free (s4_t *s4)
{
	s4_free_relations (s4);
	g_hash_table_destroy (s4->index_table);
	g_hash_table_destroy (s4->rel_table);
	g_string_chunk_free (s4->strings);

	free (s4->filename);
	free (s4);
}

static void _write_strings (GHashTable *strings, FILE *file)
{
	GHashTableIter iter;
	void *i;
	const char *str;

	g_hash_table_iter_init (&iter, strings);
	while (g_hash_table_iter_next (&iter, (void**)&str, &i)) {
		int32_t len = strlen (str);
		int32_t str_id = GPOINTER_TO_INT (i);
		fwrite (&str_id, sizeof (int32_t), 1, file);
		fwrite (&len, sizeof (int32_t), 1, file);
		fwrite (str, 1, len, file);
	}
}

static void _write_pairs (GList *pairs, FILE *file)
{
	for (; pairs != NULL; pairs = g_list_next (pairs)) {
		s4_intpair_t *pair = pairs->data;
		fwrite (pair, sizeof (s4_intpair_t), 1, file);
		free (pair);
	}
}

typedef struct {
	GHashTable *strings;
	GList *pairs;
	int new_id;
} save_data_t;

static int _get_string_number (GHashTable *table, const char *str, int *new_id)
{
	int i = GPOINTER_TO_INT (g_hash_table_lookup (table, str));

	if (i == 0) {
		i = (*new_id)++;
		g_hash_table_insert (table, (void*)str, GINT_TO_POINTER (i));
	}

	return i;
}

static void _entry_to_pair (s4_t *s4, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src, void *data)
{
	save_data_t *sd = data;
	const char *str;
	int32_t i;
	s4_intpair_t *pair = malloc (sizeof (s4_intpair_t));

	pair->key_a = _get_string_number (sd->strings, key_a, &sd->new_id);
	pair->key_b = _get_string_number (sd->strings, key_b, &sd->new_id);
	pair->src = _get_string_number (sd->strings, src, &sd->new_id);

	if (s4_val_get_int (val_a, &i)) {
		pair->val_a = i;
		pair->key_a = -pair->key_a;
	} else if (s4_val_get_str (val_a, &str)) {
		pair->val_a = _get_string_number (sd->strings, str, &sd->new_id);
	}
	if (s4_val_get_int (val_b, &i)) {
		pair->val_b = i;
		pair->key_b = -pair->key_b;
	} else if (s4_val_get_str (val_b, &str)) {
		pair->val_b = _get_string_number (sd->strings, str, &sd->new_id);
	}

	sd->pairs = g_list_prepend (sd->pairs, pair);
}

static int _write_file (s4_t *s4, const char *filename)
{
	int32_t i = -1;
	FILE *file = fopen (filename, "w");
	save_data_t sd;

	sd.strings = g_hash_table_new (NULL, NULL);
	sd.pairs = NULL;
	sd.new_id = 1;

	s4_foreach (s4, _entry_to_pair, &sd);

	fwrite (S4_MAGIC, 1, S4_MAGIC_LEN, file);
	_write_strings (sd.strings, file);
	fwrite (&i, sizeof (int32_t), 1, file);
	_write_pairs (sd.pairs, file);

	g_hash_table_destroy (sd.strings);
	g_list_free (sd.pairs);

	fclose (file);

	return 0;
}


/**
 * Open an S4 database
 *
 * @b The different flags you can pass:
 * <P>
 * @b S4_VERIFY
 * <BR>
 * 		S4 will check the database. If it is inconsistent and
 *		if S4_RECOVERYis set it will try to recover it.
 * </P><P>
 * @b S4_RECOVERY
 * <BR>
 * 		If S4_VERIFY is set the database will be recovered if it
 * 		is inconsistent. If S4_VERIFY is not set it will try to
 * 		recover the database anyway.
 * </P><P>
 * @b S4_NEW
 * <BR>
 * 		It will create a new file if one does not already exists.
 * 		If one exists it will fail and return NULL.
 * </P><P>
 * @b S4_EXISTS
 * <BR>
 * 		If the file does not exists it will fail and return NULL.
 * </P><P>
 * @b S4_SYNC_THREAD
 * <BR>
 * 		It will start a synchronisation thread that will flush the
 * 		database to disk in regular intervals.
 * </P>
 *
 * @param filename The name of the file containing the database
 * @param flags Zero or more of the flags bitwise-or'd.
 * @return A pointer to an s4_t, or NULL if something went wrong.
 */
s4_t *s4_open (const char *filename, const char **indices, int open_flags)
{
	int i;
	s4_t* s4 = malloc (sizeof(s4_t));

	s4->strings = g_string_chunk_new (8192);
	g_static_mutex_init (&s4->strings_lock);

	s4->index_table = g_hash_table_new_full (g_str_hash, g_str_equal, free, (GDestroyNotify)_index_free);
	g_static_mutex_init (&s4->index_table_lock);

	s4->rel_table = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)_index_free);
	g_static_rw_lock_init (&s4->rel_lock);

	s4->norm_table = g_hash_table_new (NULL, NULL);
	g_static_mutex_init (&s4->norm_lock);

	for (i = 0; indices != NULL && indices[i] != NULL; i++) {
		_index_add (s4, indices[i], _index_create ());
	}

	s4->logfile = NULL;
	s4->filename = strdup (filename);

	if (_read_file (s4, s4->filename, open_flags)) {
		_free (s4);
		return NULL;
	}

	s4->logfile = fopen ("/tmp/s4.log", "w");

	return s4;
}

/**
 * Close an open S4 database
 *
 * @param s4 The database to close
 *
 */
int s4_close (s4_t* s4)
{
	_write_file (s4, s4->filename);

	_free (s4);

	return 0;
}

/**
 * Write all changes to disk
 *
 * @param s4 The database to sync
 *
 */
void s4_sync (s4_t *s4)
{
}

/*
int s4_recover (s4_t *s4, const char *name)
{
	s4be_t *rec;
	int ret = 1;

	rec = s4be_open (name, S4_NEW);

	if (rec == NULL) {
		return 0;
	}

	ret = s4be_recover (s4->be, rec);

	fix_refcount (rec);

	s4be_close (rec);

	return ret;
}
*/


/**
 * Return the last error number set.
 * This function is thread safe, error numbers set in one thread
 * will NOT be seen in another thread.
 *
 * @return The last error number set, or 0 if none has been set
 */
int s4_errno()
{
	int *i = g_static_private_get (&_errno);
	if (i == NULL) {
		return 0;
	}
	return *i;
}

/**
 * Set errno to the given error number
 *
 * @param err The error number to set
 */
void s4_set_errno (int err)
{
	int *i = g_static_private_get (&_errno);
	if (i == NULL) {
		i = malloc (sizeof (int));
		g_static_private_set (&_errno, i, NULL);
	}

	*i = err;
}

/**
 * @}
 */
