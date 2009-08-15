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

#include "xcu.h"
#include "s4.h"
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>

SETUP (s4) {
	if (!g_thread_get_initialized ())
		g_thread_init (NULL);

	return 0;
}

CLEANUP () {
	return 0;
}

char *name;
s4_t *s4;

static void _open ()
{
	name = strdup (tmpnam (NULL));
	s4 = s4_open (name, S4_VERIFY | S4_SYNC_THREAD);
}
static void _close ()
{
	s4_close (s4);
	g_unlink (name);
	free (name);
}

#define ARG_SIZE 10
struct db_struct {
	const char *name;
	const char *args[ARG_SIZE];
};

static void create_db (struct db_struct *db)
{
	s4_entry_t *entry, *prop;
	int i, j;

	for (i = 0; db[i].name != NULL; i++) {
		entry = s4_entry_get_s (s4, "entry", db[i].name);

		for (j = 0; db[i].args[j] != NULL; j++) {
			prop = s4_entry_get_s (s4, "property", db[i].args[j]);
			s4_entry_add (s4, entry, prop, "testsuite");
			s4_entry_free (prop);
		}
		s4_entry_free (entry);
	}
}

static void check_db (struct db_struct *db)
{
	s4_entry_t *entry, *se;
	s4_set_t *set;
	int i, j;

	for (i = 0; db[i].name != NULL; i++) {
		char *args[ARG_SIZE];
		memcpy (args, db[i].args, ARG_SIZE * sizeof (char*));

		entry = s4_entry_get_s (s4, "entry", db[i].name);
		set = s4_entry_contains (s4, entry);

		for (se = s4_set_next (set); se != NULL; se = s4_set_next (set)) {
			s4_entry_fillin (s4, se);
			for (j = 0; args[j] != NULL &&
					strcmp (args[j], se->val_s); j++);
			CU_ASSERT_PTR_NOT_NULL (args[j]);
			memmove (args + j, args + j + 1,
					(ARG_SIZE - j - 1) * sizeof (char*));
		}

		s4_set_free (set);

		CU_ASSERT_PTR_NULL (args[0]);

		s4_entry_free (entry);
	}
}

CASE (s4_verify) {
	struct db_struct db[] = {
		{"a",  {"b", "c", NULL}},
		{"c", {"d", "e", NULL}},
		{NULL, {NULL}}};
	_open ();

	create_db (db);
	check_db (db);

	s4_sync (s4);
	CU_ASSERT_TRUE (s4_verify (s4, S4_VERIFY_THOROUGH | S4_VERIFY_REFCOUNT));

	_close ();
}

CASE (s4_recover) {
	char *filename;
	s4_t *tmp;
	struct db_struct db[] = {
		{"a",  {"b", "c", NULL}},
		{"c", {"d", "e", NULL}},
		{NULL, {NULL}}};
	_open ();

	create_db (db);
	check_db (db);

	filename = tmpnam (NULL);
	s4_recover (s4, filename);

	tmp = s4;
	s4 = s4_open (filename, S4_EXISTS);
	s4_sync (s4);
	CU_ASSERT_TRUE (s4_verify (s4, S4_VERIFY_THOROUGH | S4_VERIFY_REFCOUNT));
	check_db (db);

	s4_close (s4);

	g_unlink (filename);

	s4 = tmp;

	_close();
}
