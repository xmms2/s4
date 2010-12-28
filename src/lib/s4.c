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
#define S4_MAGIC_LEN (4)
#define S4_VERSION 1

typedef struct {
	char magic[S4_MAGIC_LEN];
	int32_t version;
	unsigned char uuid[16];
	log_number_t last_checkpoint;
} s4_header_t;

/**
 * @{
 * @internal
 */

/**
 * Reads strings from a file
 *
 * @param s4 The database to add the strings to
 * @param file The file to read from
 * @return A hashtable with the id as they key and the
 * corresponding string as they values or NULL on error
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
 * Reads relations from a file
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
		const s4_val_t *val_a, *val_b;

		key_a = g_hash_table_lookup (strings, GINT_TO_POINTER (ABS (rec.key_a)));
		key_b = g_hash_table_lookup (strings, GINT_TO_POINTER (ABS (rec.key_b)));
		src = g_hash_table_lookup (strings, GINT_TO_POINTER (ABS (rec.src)));

		if (rec.key_a > 0) {
			val_a = _string_lookup_val (s4,
					g_hash_table_lookup (strings, GINT_TO_POINTER (rec.val_a)));
		} else {
			val_a = _int_lookup_val (s4, rec.val_a);
		}
		if (rec.key_b > 0) {
			val_b = _string_lookup_val (s4,
					g_hash_table_lookup (strings, GINT_TO_POINTER (rec.val_b)));
		} else {
			val_b = _int_lookup_val (s4, rec.val_b);
		}

		_s4_add_internal (s4, key_a, val_a, key_b, val_b, src);
	}

	return 0;
}

/**
 * Reads an S4 database from filename.
 *
 * @param s4 The s4 database to read the data into
 * @param filename The name of the file to read from
 * @return 0 on success, non-zero on error
 */
static int _read_file (s4_t *s4, const char *filename, int flags)
{
	FILE *file = fopen (filename, "r");
	s4_header_t hdr;
	int i;

	if (file == NULL) {
		int ret = 0;
		switch (errno) {
			case ENOENT:
				if (flags & S4_EXISTS) {
					s4_set_errno (S4E_NOENT);
					ret = -1;
				} else {
					s4_create_uuid (s4->uuid);
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

	fread (&hdr, sizeof (s4_header_t), 1, file);
	if (strncmp (S4_MAGIC, hdr.magic, S4_MAGIC_LEN)) {
		fclose (file);
		s4_set_errno (S4E_MAGIC);
		return -1;
	}

	if (hdr.version != S4_VERSION) {
		fclose (file);
		s4_set_errno (S4E_VERSION);
		return -1;
	}

	s4->last_logpoint = s4->last_checkpoint = hdr.last_checkpoint;
	for (i = 0; i < 16; i++) {
		s4->uuid[i] = hdr.uuid[i];
	}

	GHashTable *strings = _read_string (s4, file);
	if (strings == NULL || _read_relations (s4, file, strings) == -1) {
		fclose (file);
		s4_set_errno (S4E_INCONS);
		return -1;
	}
	g_hash_table_destroy (strings);

	fclose (file);
	return 0;
}

int _reread_file (s4_t *s4)
{
	_free_relations (s4);
	return _read_file (s4, s4->filename, S4_EXISTS);
}

/**
 * Writes all the id->string relations in the hash table to file
 *
 * @param strings The hashtable holding the key-value pairs
 * @param file The file to write to
 */
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

/**
 * Writes a list of int-pairs to file
 *
 * @param pairs A GList with pairs to write
 * @param file The file to write to
 */
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

/**
 * Gets the id of a string, or gives it an unique id if it doesn't have one
 *
 * @param table A hashtable with id->string pairs
 * @param str The string to lookup
 * @param new_id A pointer to an int holding the next free id to use
 * @return The id associated with the string
 */
static int _get_string_number (save_data_t *sd, const char *str)
{
	int i = GPOINTER_TO_INT (g_hash_table_lookup (sd->strings, str));

	if (i == 0) {
		i = sd->new_id++;
		g_hash_table_insert (sd->strings, (void*)str, GINT_TO_POINTER (i));
	}

	return i;
}

/*
 * A helper function converting a resultset into a list of int-pairs.
 */
static void _result_to_pairs (s4_resultset_t *res, save_data_t *sd)
{
	const s4_resultrow_t *row;
	int row_no;

	for (row_no = 0; s4_resultset_get_row (res, row_no, &row); row_no++) {
		int32_t va, ka, i;
		const s4_val_t *val_a, *val_b;
		const char *key_a, *key_b, *src, *str;
		const s4_result_t *id_res, *val_res;

		s4_resultrow_get_col (row, 0, &id_res);
		s4_resultrow_get_col (row, 1, &val_res);

		val_a = s4_result_get_val (id_res);
		key_a = s4_result_get_key (id_res);

		ka = _get_string_number (sd, key_a);

		if (s4_val_get_int (val_a, &i)) {
			va = i;
			ka = -ka;
		} else if (s4_val_get_str (val_a, &str)) {
			va = _get_string_number (sd, str);
		}

		for (; val_res != NULL; val_res = s4_result_next (val_res)) {
			s4_intpair_t *pair = malloc (sizeof (s4_intpair_t));

			val_b = s4_result_get_val (val_res);
			key_b = s4_result_get_key (val_res);
			src = s4_result_get_src (val_res);

			pair->val_a = va;
			pair->key_a = ka;
			pair->key_b = _get_string_number (sd, key_b);
			pair->src = _get_string_number (sd, src);

			if (s4_val_get_int (val_b, &i)) {
				pair->val_b = i;
				pair->key_b = -pair->key_b;
			} else if (s4_val_get_str (val_b, &str)) {
				pair->val_b = _get_string_number (sd, str);
			}

			sd->pairs = g_list_prepend (sd->pairs, pair);
		}
	}
}

/**
 * Writes the database to disk
 *
 * @param s4 The database to write
 * @param filename The file to write to
 * @return 0 on success, non-zero on error
 */
static int _write_file (s4_t *s4)
{
	int32_t i = -1;
	int j;
	FILE *file;
	s4_header_t hdr;
	save_data_t sd;
	s4_condition_t *cond;
	s4_fetchspec_t *fs;
	s4_resultset_t *res;
	s4_transaction_t *trans;

	_log_lock_db (s4);

	file = fopen (s4->tmp_filename, "w");
	if (file == NULL) {
		_log_unlock_db (s4);
		return 0;
	}

	sd.strings = g_hash_table_new (NULL, NULL);
	sd.pairs = NULL;
	sd.new_id = 1;

	cond = s4_cond_new_filter (S4_FILTER_EXISTS, NULL, NULL, NULL, S4_CMP_BINARY, 0);

	fs = s4_fetchspec_create ();
	s4_fetchspec_add (fs, NULL, NULL, S4_FETCH_PARENT);
	s4_fetchspec_add (fs, NULL, NULL, S4_FETCH_DATA);

	trans = s4_begin (s4, 0);
	res = s4_query (s4, trans, fs, cond);
	_transaction_writing (trans);
	s4_commit (trans);

	_result_to_pairs (res, &sd);

	s4_cond_free (cond);
	s4_fetchspec_free (fs);
	s4_resultset_free (res);

	strncpy (hdr.magic, S4_MAGIC, S4_MAGIC_LEN);
	hdr.version = S4_VERSION;
	for (j = 0; j < 16; j++) {
		hdr.uuid[j] = s4->uuid[j];
	}
	hdr.last_checkpoint = s4->last_synced;

	fwrite (&hdr, sizeof (s4_header_t), 1, file);
	_write_strings (sd.strings, file);
	fwrite (&i, sizeof (int32_t), 1, file);
	_write_pairs (sd.pairs, file);

	g_hash_table_destroy (sd.strings);
	g_list_free (sd.pairs);

	fclose (file);

	g_rename (s4->tmp_filename, s4->filename);

	_log_checkpoint (s4);
	_log_unlock_db (s4);
	return 1;
}

static void *_sync_thread (s4_t *s4)
{
	g_mutex_lock (s4->sync_lock);
	while (s4->sync_thread_run) {
		if (s4->sync_thread_run)
			g_cond_wait (s4->sync_cond, s4->sync_lock);
		g_mutex_unlock (s4->sync_lock);

		s4_sync (s4);

		g_mutex_lock (s4->sync_lock);
		g_cond_broadcast (s4->sync_finished_cond);
	}
	g_mutex_unlock (s4->sync_lock);

	return NULL;
}

/* Start sync */
void _start_sync (s4_t *s4)
{
	g_mutex_lock (s4->sync_lock);
	g_cond_signal (s4->sync_cond);
	g_mutex_unlock (s4->sync_lock);
}

/* Start and wait for sync to finish */
void _sync (s4_t *s4)
{
	g_mutex_lock (s4->sync_lock);
	g_cond_signal (s4->sync_cond);
	g_cond_wait (s4->sync_finished_cond, s4->sync_lock);
	g_mutex_unlock (s4->sync_lock);
}

static s4_t *_alloc (void)
{
	s4_t* s4 = calloc (1, sizeof(s4_t));

	s4->strings = g_string_chunk_new (8192);
	s4->strings_table = g_hash_table_new_full (g_str_hash, g_str_equal,
			NULL, (GDestroyNotify)s4_val_free);
	g_static_mutex_init (&s4->strings_lock);

	s4->int_table = g_hash_table_new_full (NULL, NULL,
			NULL, (GDestroyNotify)s4_val_free);
	g_static_mutex_init (&s4->int_lock);

	s4->index_table = g_hash_table_new_full (g_str_hash, g_str_equal, free, (GDestroyNotify)_index_free);
	g_static_mutex_init (&s4->index_table_lock);

	s4->rel_table = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)_index_free);
	g_static_mutex_init (&s4->rel_lock);

	s4->coll_table = g_hash_table_new (NULL, NULL);
	g_static_mutex_init (&s4->coll_lock);
	s4->case_table = g_hash_table_new (NULL, NULL);
	g_static_mutex_init (&s4->case_lock);

	s4->log_lock = g_mutex_new ();
	s4->log_users = 0;

	s4->sync_lock = g_mutex_new ();
	s4->sync_cond = g_cond_new ();
	s4->sync_finished_cond = g_cond_new ();

	s4->lock_table = _create_lock_table ();
	s4->lock_lock = g_mutex_new ();

	return s4;
}

/**
 * Frees an S4 handle and everything it points at
 *
 * @param s4 The handle to free
 */
static void _free (s4_t *s4)
{
	_free_relations (s4);
	g_hash_table_destroy (s4->index_table);
	g_hash_table_destroy (s4->rel_table);
	g_hash_table_destroy (s4->coll_table);
	g_hash_table_destroy (s4->case_table);
	g_hash_table_destroy (s4->strings_table);
	g_hash_table_destroy (s4->int_table);
	g_hash_table_destroy (s4->lock_table);
	g_string_chunk_free (s4->strings);

	g_mutex_free (s4->log_lock);
	g_mutex_free (s4->sync_lock);
	g_mutex_free (s4->lock_lock);
	g_cond_free (s4->sync_cond);
	g_cond_free (s4->sync_finished_cond);

	g_static_mutex_free (&s4->strings_lock);
	g_static_mutex_free (&s4->rel_lock);
	g_static_mutex_free (&s4->index_table_lock);
	g_static_mutex_free (&s4->case_lock);
	g_static_mutex_free (&s4->coll_lock);
	g_static_mutex_free (&s4->int_lock);

	free (s4->filename);
	g_free (s4->tmp_filename);
	free (s4);
}

/**
 * @}
 */

/**
 * Opens an S4 database
 *
 * @b The different flags you can pass:
 * <P>
 * @b S4_NEW
 * <BR>
 * 		It will create a new file if one does not already exists.
 * 		If one exists it will fail and return NULL.
 * </P><P>
 * @b S4_EXISTS
 * <BR>
 * 		If the file does not exists it will fail and return NULL.
 * 		s4_errno may be used to get more information about what
 * 		went wrong.
 * </P><P>
 * @b S4_MEMORY
 * 		Creates a memory-only database. It will not read any files
 * 		on startup or write files on shutdown. Use this if you want
 * 		a temporary database.
 * <BR>
 *
 * @param filename The name of the file containing the database
 * @param indices An array of keys to have indices on
 * @param open_flags Zero or more of the flags bitwise-or'd.
 * @return A pointer to an s4_t, or NULL if something went wrong.
 */
s4_t *s4_open (const char *filename, const char **indices, int open_flags)
{
	int i;
	s4_t *s4;

	s4 = _alloc ();

	for (i = 0; indices != NULL && indices[i] != NULL; i++) {
		_index_add (s4, indices[i], _index_create ());
	}

	s4->open_flags = open_flags;

	if (open_flags & S4_MEMORY) {
		return s4;
	}

	s4->filename = strdup (filename);
	s4->tmp_filename = g_strconcat (filename, ".chkpnt", NULL);
	if (_read_file (s4, s4->filename, open_flags)) {
		_free (s4);
		return NULL;
	}

	if (!_log_open (s4)) {
		_free (s4);
		return NULL;
	}

	s4->sync_thread_run = 1;
	s4->sync_thread = g_thread_create ((GThreadFunc)_sync_thread, s4, TRUE, NULL);

	/* Write the file right away in case we have opened a new file
	 * or we redid something in the log that was not in the file
	 */
	g_cond_signal (s4->sync_cond);

	return s4;
}

/**
 * Closes an open S4 database
 *
 * @param s4 The database to close
 *
 */
int s4_close (s4_t* s4)
{
	if (!(s4->open_flags & S4_MEMORY)) {
		g_mutex_lock (s4->sync_lock);
		s4->sync_thread_run = 0;
		g_cond_signal (s4->sync_cond);
		g_mutex_unlock (s4->sync_lock);
		g_thread_join (s4->sync_thread);

		_log_close (s4);
	}

	_free (s4);

	return 0;
}


/**
 * Writes all changes to disk
 *
 * @param s4 The database to sync
 *
 */
void s4_sync (s4_t *s4)
{
	if (!_write_file (s4)) {
		S4_ERROR ("s4_sync: could not write file");
	}
}

/**
 * Returns the last error number set.
 * This function is thread safe, error numbers set in one thread
 * will NOT be seen in another thread.
 *
 * @return The last error number set, or 0 if none has been set
 */
s4_errno_t s4_errno()
{
	s4_errno_t *i = g_static_private_get (&_errno);
	if (i == NULL) {
		return 0;
	}
	return *i;
}

/**
 * Sets errno to the given error number
 *
 * @param err The error number to set
 */
void s4_set_errno (s4_errno_t err)
{
	s4_errno_t *i = g_static_private_get (&_errno);
	if (i == NULL) {
		i = malloc (sizeof (s4_errno_t));
		g_static_private_set (&_errno, i, NULL);
	}

	*i = err;
}

/**
 * @}
 */
