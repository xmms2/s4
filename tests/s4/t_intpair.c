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
#include "s4_be.h"
#include <stdio.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>

s4be_t *be;
char *name;

#define SAMPLES 1000

SETUP (s4_intpair) {
	if (!g_thread_get_initialized ())
		g_thread_init (NULL);
	name = tmpnam (NULL);
	be = s4be_open (name, S4_NEW);
	return be == NULL;
}

CLEANUP () {
	s4be_close (be);
	g_unlink (name);
	return 0;
}

int ip_add (int i, int j)
{
	s4_entry_t entry, prop;

	entry.key_i = i;
	entry.val_i = i;
	entry.src_i = i;

	prop.key_i = j;
	prop.val_i = j;
	prop.src_i = j;

	return s4be_ip_add (be, &entry, &prop);
}

int ip_del (int i, int j)
{
	s4_entry_t entry, prop;

	entry.key_i = i;
	entry.val_i = i;
	entry.src_i = i;

	prop.key_i = j;
	prop.val_i = j;
	prop.src_i = j;

	return s4be_ip_del (be, &entry, &prop);
}

CASE (test_ip_add_del) {
	int i;

	for (i = 1; i <= SAMPLES; i++) {
		CU_ASSERT_EQUAL (ip_add (i, i), 0);
	}
	for (i = 1; i <= SAMPLES; i++) {
		CU_ASSERT_EQUAL (ip_add (i, i), -1);
	}

	for (i = 2*SAMPLES; i > SAMPLES; i--) {
		CU_ASSERT_EQUAL (ip_add (i, i), 0);
	}
	for (i = 2*SAMPLES; i > SAMPLES; i--) {
		CU_ASSERT_EQUAL (ip_add (i, i), -1);
	}

	/*
	for (i = SAMPLES + 1; i <= 2*SAMPLES; i++) {
		CU_ASSERT_EQUAL (ip_del (i, i), 0);
	}
	for (i = SAMPLES + 1; i <= 2*SAMPLES; i++) {
		CU_ASSERT_EQUAL (ip_del (i, i), -1);
	}
	for (i = 1*SAMPLES; i > 0*SAMPLES; i--) {
		CU_ASSERT_EQUAL (ip_del (i, i), 0);
	}
	for (i = 1*SAMPLES; i > 0*SAMPLES; i--) {
		CU_ASSERT_EQUAL (ip_del (i, i), -1);
	}
	*/

	for (i = 1; i <= SAMPLES * 2; i += 5)
		ip_del (i, i);


	for (i = 1; i <= SAMPLES * 2; i++) {
		ip_del (i, i);
	}
}

static void test_set (s4_set_t *set, int *values, int src)
{
	s4_set_reset (set);
	s4_entry_t *entry = s4_set_next (set);
	while (entry != NULL && *values != -1) {
		CU_ASSERT_EQUAL (entry->key_i, *values);
		CU_ASSERT_EQUAL (entry->val_i, *values);
		CU_ASSERT_EQUAL (entry->src_i, (src==-1)?(*values):(src));
		entry = s4_set_next (set);
		values++;
	}

	CU_ASSERT_EQUAL (*values, -1);
	CU_ASSERT_PTR_NULL (entry);

	s4_set_free (set);
}

static s4_set_t *has_this (int i)
{
	s4_entry_t entry;

	entry.key_i = i;
	entry.val_i = i;
	entry.src_i = i;

	return s4be_ip_has_this (be, &entry);
}

static s4_set_t *this_has (int i)
{
	s4_entry_t entry;

	entry.key_i = i;
	entry.val_i = i;
	entry.src_i = i;

	return s4be_ip_this_has (be, &entry);
}

static s4_set_t *ip_get (int i, int j)
{
	s4_entry_t entry;

	entry.key_i = i;
	entry.val_i = i;
	entry.src_i = i;

	return s4be_ip_get (be, &entry, j);
}

CASE (test_ip_has) {
	int j;
	int has[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, -1};
	int this[] = {1, -1};

	for (j = 1; j < 10; j++) {
		CU_ASSERT_EQUAL (ip_add (1, j), 0);
	}

	test_set (this_has (1), has, -1);

	for (j = 1; j < 10; j++) {
		test_set (has_this (j), this, j);
	}

	for (j = 1; j < 10; j++) {
		this[0] = j;
		test_set (ip_get (1, j), this, -1);
	}

	for (j = 1; j < 10; j++) {
		CU_ASSERT_EQUAL (ip_del (1, j), 0);
	}
}
