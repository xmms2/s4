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

#include "s4_be.h"
#include "midb.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MIDB_MAGIC ("midb")
#define MIDB_MAGIC_LEN (strlen (MIDB_MAGIC))

/* Sync the database if it is dirty */
void s4be_sync (s4be_t *s4)
{
}

/**
 * Read strings from a file
 *
 * @param s4 The database to add the strings to
 * @param file The file to read from
 * @return 0 on success, -1 otherwise
 */
static int _read_string (s4be_t *s4, FILE *file)
{
	size_t r;
	int32_t id, len;
	char *str;

	while ((r = fread (&id, sizeof (int32_t), 1, file)) == 1 &&
			id != -1 &&
			(r = fread (&len, sizeof (int32_t), 1, file)) == 1) {
		str = malloc (len + 1);
		r = fread (str, 1, len, file);

		if (r != len)
			return -1;

		str[len] = '\0';
		s4be_st_insert (s4, id, str);
	}

	if (r == 0)
		return -1;

	return 0;
}

/**
 * Read relations from a file
 *
 * @param s4 The database to insert them into
 * @param file The file to read
 * @return -1 on error, 0 otherwise
 */
static int _read_relations (s4be_t *s4, FILE *file)
{
	midb_data_t rec;
	s4_entry_t e,p;

	while (fread (&rec, sizeof (midb_data_t), 1, file) == 1) {
		if (rec.key_a < 0) {
			if (s4be_st_ref_id (s4, -rec.key_a) == -1)
				return -1;
		} else {
			if (s4be_st_ref_id (s4, rec.key_a) == -1 ||
					s4be_st_ref_id (s4, rec.val_a) == -1)
				return -1;
		}
		if (rec.key_b < 0) {
			if (s4be_st_ref_id (s4, -rec.key_b) == -1)
				return -1;
		} else {
			if (s4be_st_ref_id (s4, rec.key_b) == -1 ||
					s4be_st_ref_id (s4, rec.val_b) == -1)
				return -1;
		}

		if (s4be_st_ref_id (s4, rec.src) == -1)
			return -1;

		e.key_i = rec.key_a;
		e.val_i = rec.val_a;
		e.src_i = rec.src;
		p.key_i = rec.key_b;
		p.val_i = rec.val_b;
		p.src_i = rec.src;

		if (s4be_ip_add (s4, &e, &p) == -1)
			return -1;
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
static int _read_file (s4be_t *s4, const char *filename, int flags)
{
	char magic[MIDB_MAGIC_LEN];
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

	fread (magic, 1, MIDB_MAGIC_LEN, file);
	if (strncmp (MIDB_MAGIC, magic, MIDB_MAGIC_LEN)) {
		fclose (file);
		s4_set_errno (S4E_MAGIC);
		return -1;
	}

	if (_read_string (s4, file) == -1 || _read_relations (s4, file) == -1) {
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
static void _free (s4be_t *s4)
{
	g_hash_table_destroy (s4->str_table);
	g_hash_table_destroy (s4->norm_str_table);

	idt_destroy (s4->id_str_table);

	//bpt_destroy (s4->int_store);
	//bpt_destroy (s4->rev_store);

	free (s4->filename);
	free (s4);
}

/**
 * Open an S4 backend database.
 *
 * @param filename The file to open
 * @param open_flags Flags to use
 * @return A pointer to an s4be_t, or NULL on error.
 */
s4be_t *s4be_open (const char *filename, int open_flags)
{
	s4be_t* s4 = malloc (sizeof(s4be_t));

	s4->str_table = g_hash_table_new_full (g_str_hash, g_str_equal, free, free);
	s4->norm_str_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, free);
	s4->id_str_table = idt_create ();
	idt_insert (s4->id_str_table, NULL);

	g_static_mutex_init (&s4->str_table_lock);
	g_static_mutex_init (&s4->norm_str_table_lock);
	g_static_mutex_init (&s4->id_str_table_lock);

	g_static_rw_lock_init (&s4->rwlock);

//	s4->int_store = bpt_create ();
//	s4->rev_store = bpt_create ();

	s4->logfile = NULL;
	s4->filename = strdup (filename);

	if (_read_file (s4, s4->filename, open_flags)) {
		_free (s4);
		return NULL;
	}

	s4->logfile = fopen ("/tmp/s4.log", "w");

	return s4;
}

static void _write_string (int32_t str_id, const char *str, void *ud)
{
	FILE *file = ud;
	int32_t len = strlen (str);

	fwrite (&str_id, sizeof (int32_t), 1, file);
	fwrite (&len, sizeof (int32_t), 1, file);
	fwrite (str, 1, len, file);
}

static void _write_relation (s4_entry_t *e, s4_entry_t *p, void *ud)
{
	FILE *file = ud;
	midb_data_t rec;

	rec.key_a = e->key_i;
	rec.val_a = e->val_i;
	rec.key_b = p->key_i;
	rec.val_b = p->val_i;
	rec.src = p->src_i;

	fwrite (&rec, sizeof (midb_data_t), 1, file);
}

static int _write_file (s4be_t *s4, const char *filename)
{
	int32_t i = -1;
	FILE *file = fopen (filename, "w");

	fwrite (MIDB_MAGIC, 1, MIDB_MAGIC_LEN, file);

	s4be_st_foreach (s4, _write_string, file);

	fwrite (&i, sizeof (int32_t), 1, file);

	s4be_ip_foreach (s4, _write_relation, file);
	fclose (file);

	return 0;
}

/**
 * Close an open s4 database
 *
 * @param s4 The database to close
 * @return 0 on success, anything else on error
 */
int s4be_close (s4be_t* s4)
{
	_write_file (s4, s4->filename);

	_free (s4);

	return 0;
}

int s4be_recover (s4be_t *old, s4be_t *rec)
{
	return 1;
}

/**
 * Check that the database is consistent
 *
 * @param be The database to check
 * @param thorough Set to 0 to just do a quick check, 1 to do a full check.
 * @return 1 if the database is good, 0 otherwise
 */
int s4be_verify (s4be_t *be, int thorough)
{
	int ret = 1;

	if (thorough) {
		//ret = bpt_verify (be->int_store) & bpt_verify (be->rev_store);
	}

	return ret;
}

/* Locking routines
 * The database is protected by a multiple readers,
 * single writer lock. Use these functions to lock/unlock
 * the database.
 */
void be_rlock (s4be_t *s4)
{
	g_static_rw_lock_reader_lock (&s4->rwlock);
}

void be_runlock (s4be_t *s4)
{
	g_static_rw_lock_reader_unlock (&s4->rwlock);
}

void be_wlock (s4be_t *s4)
{
	g_static_rw_lock_writer_lock (&s4->rwlock);
}

void be_wunlock (s4be_t *s4)
{
	g_static_rw_lock_writer_unlock (&s4->rwlock);
}

/**
 * @}
 */
