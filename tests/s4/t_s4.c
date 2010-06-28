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

SETUP (S4) {
	if (!g_thread_get_initialized ())
		g_thread_init (NULL);

	return 0;
}

CLEANUP () {
	return 0;
}

char *name;
s4_t *s4;

static void _open (int flags)
{
	name = strdup (tmpnam (NULL));
	s4 = s4_open (name, NULL, flags);
}

static void _close ()
{
	char *logname = g_strconcat (name, ".log", NULL);
	s4_close (s4);
	g_unlink (name);
	g_unlink (logname);
	g_free (logname);
	free (name);
}


#define ARG_SIZE 10
struct db_struct {
	const char *name;
	const char *args[ARG_SIZE];
	const char *src;
};

static void create_db (struct db_struct *db)
{
	s4_val_t *name_val, *arg_val;
	int i, j;

	for (i = 0; db[i].name != NULL; i++) {
		name_val = s4_val_new_string (db[i].name);

		for (j = 0; db[i].args[j] != NULL; j++) {
			arg_val = s4_val_new_string (db[i].args[j]);
			CU_ASSERT (s4_add (s4, "entry", name_val, "property", arg_val, db[i].src));
			s4_val_free (arg_val);
		}

		s4_val_free (name_val);
	}
}

static void check_db (struct db_struct *db)
{
	int i, j;
	s4_fetchspec_t *fs = s4_fetchspec_create ();
	s4_fetchspec_add (fs, NULL, NULL);

	for (i = 0; db[i].name != NULL; i++) {
		s4_val_t *name_val = s4_val_new_string (db[i].name);
		s4_condition_t *cond = s4_cond_new_filter (S4_FILTER_EQUAL,
				"entry", name_val, NULL, S4_COND_PARENT);
		s4_resultset_t *set = s4_query (s4, fs, cond);
		const s4_result_t *res = s4_resultset_get_result (set, 0, 0);
		int found[ARG_SIZE] = {0};

		for (; res != NULL; res = s4_result_next (res)) {
			for (j = 0; db[i].args[j] != NULL; j++) {
				const char *str;

				if (s4_val_get_str (s4_result_get_val (res), &str) && !strcmp (str, db[i].args[j]) &&
						!strcmp (s4_result_get_key (res), "property") &&
						!strcmp (s4_result_get_src (res), db[i].src)) {
					found[j] = 1;
					break;
				}
			}
		}
		for (j = 0; db[i].args[j] != NULL; j++) {
			CU_ASSERT_EQUAL (found[j], 1);
		}

		s4_resultset_free (set);
		s4_cond_free (cond);
		s4_val_free (name_val);
	}

	s4_fetchspec_free (fs);
}

CASE (test_log) {
	struct db_struct db[] = {
		{"a", {"a", NULL}, "1"},
		{"a", {"b", NULL}, "2"},
		{"b", {"a", NULL}, "2"},
		{"b", {"b", NULL}, "1"},
		{NULL, {NULL}, NULL}};
	_open (S4_NEW);

	create_db (db);
	check_db (db);

	s4 = s4_open (name, NULL, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL (s4);

	check_db (db);

	_close ();
}


CASE (test_open) {
	struct db_struct db[] = {
		{"a", {"b", "c", NULL}, "src_a"},
		{"b", {"x", "foobar", NULL}, "src_b"},
		{"c", {"basdf", "c", NULL}, "src_c"},
		{NULL, {NULL}, NULL}};
	_open (S4_EXISTS);
	CU_ASSERT_PTR_NULL (s4);
	CU_ASSERT_EQUAL (s4_errno (), S4E_NOENT);

	s4 = s4_open (name, NULL, S4_NEW);

	CU_ASSERT_PTR_NOT_NULL_FATAL (s4);

	create_db(db);
	check_db(db);

	s4_close (s4);

	s4 = s4_open (name, NULL, S4_NEW);
	CU_ASSERT_PTR_NULL (s4);
	CU_ASSERT_EQUAL (s4_errno (), S4E_EXISTS);

	s4 = s4_open (name, NULL, S4_EXISTS);

	CU_ASSERT_PTR_NOT_NULL_FATAL (s4);

	check_db(db);
	_close();
}

static void del_db (struct db_struct db[])
{
	s4_val_t *name_val, *arg_val;
	int i, j;

	for (i = 0; db[i].name != NULL; i++) {
		name_val = s4_val_new_string (db[i].name);

		for (j = 0; db[i].args[j] != NULL; j++) {
			arg_val = s4_val_new_string (db[i].args[j]);
			CU_ASSERT (s4_del (s4, "entry", name_val, "property", arg_val, db[i].src));
			s4_val_free (arg_val);
		}

		s4_val_free (name_val);
	}
}

CASE (test_add_and_del) {
	struct db_struct db[] = {
		{"a", {"b", "c", NULL}, "src_a"},
		{"b", {"x", "foobar", NULL}, "src_b"},
		{"c", {"basdf", "c", NULL}, "src_c"},
		{NULL, {NULL}, NULL}};
	struct db_struct empty[] = {
		{NULL, {NULL}}};
	_open (S4_NEW);

	CU_ASSERT_PTR_NOT_NULL_FATAL (s4);

	create_db(db);
	check_db(db);

	del_db (db);
	check_db (empty);

	_close ();
}

static void check_result (const s4_result_t *res, const char *key, const char *val, const char *src)
{
	const char *str;
	CU_ASSERT (s4_val_get_str (s4_result_get_val (res), &str));
	CU_ASSERT (!strcmp (s4_result_get_key (res), key));
	CU_ASSERT (!strcmp (str, val));
	CU_ASSERT (!strcmp (s4_result_get_src (res), src));
}

CASE (test_query) {
	struct db_struct db[] = {
		{"a", {"a", NULL}, "1"},
		{"a", {"b", NULL}, "2"},
		{"b", {"a", NULL}, "2"},
		{"b", {"b", NULL}, "1"},
		{NULL, {NULL}, NULL}};
	const char *sources[] = {"1", "2", NULL};
	_open (S4_NEW);

	create_db (db);
	check_db (db);

	s4_sourcepref_t *sp = s4_sourcepref_create (sources);
	s4_fetchspec_t *fs = s4_fetchspec_create ();
	s4_fetchspec_add (fs, "property", sp);

	s4_condition_t *cond = s4_cond_new_filter (S4_FILTER_EQUAL, "property", s4_val_new_string ("a"), sp, 0);
	s4_resultset_t *set = s4_query (s4, fs, cond);
	CU_ASSERT_PTR_NOT_NULL_FATAL (set);
	CU_ASSERT_EQUAL (s4_resultset_get_colcount (set), 1);
	CU_ASSERT_EQUAL (s4_resultset_get_rowcount (set), 1);

	const s4_result_t *res = s4_resultset_get_result (set, 0, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL (res);
	check_result (res, "property", "a", "1");
	s4_resultset_free (set);
	s4_cond_free (cond);

	cond = s4_cond_new_filter (S4_FILTER_EQUAL, "property", s4_val_new_string ("b"), sp, 0);
	set = s4_query (s4, fs, cond);
	CU_ASSERT_PTR_NOT_NULL_FATAL (set);
	CU_ASSERT_EQUAL (s4_resultset_get_colcount (set), 1);
	CU_ASSERT_EQUAL (s4_resultset_get_rowcount (set), 1);

	res = s4_resultset_get_result (set, 0, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL (res);
	check_result (res, "property", "b", "1");
	s4_resultset_free (set);
	s4_cond_free (cond);

	s4_sourcepref_free (sp);
	s4_fetchspec_free (fs);

	_close ();
}
