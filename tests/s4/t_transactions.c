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

s4_t *s4;
s4_val_t *val;

SETUP (S4) {
	if (!g_thread_get_initialized ())
		g_thread_init (NULL);

	val = s4_val_new_int (1);
	return 0;
}

static void _mem_open ()
{
	s4 = s4_open (NULL, NULL, S4_MEMORY);
}

static void _mem_close ()
{
	s4_close (s4);
}

CLEANUP () {
	s4_val_free (val);
	return 0;
}

static void _dead_thread_one ()
{
	s4_transaction_t *trans = s4_begin (s4, 0);

	CU_ASSERT_PTR_NOT_NULL (trans);
	CU_ASSERT_TRUE (s4_add (NULL, trans, "a", val, "b", val, "src"));

	g_usleep (G_USEC_PER_SEC);

	CU_ASSERT_TRUE (s4_add (NULL, trans, "b", val, "a", val, "src"));

	CU_ASSERT_TRUE (s4_commit (trans));
}

static void _dead_thread_two ()
{
	s4_transaction_t *trans = s4_begin (s4, 0);

	CU_ASSERT_PTR_NOT_NULL (trans);
	g_usleep (G_USEC_PER_SEC / 2);
	CU_ASSERT_TRUE (s4_add (NULL, trans, "b", val, "a", val, "src"));
	g_usleep (G_USEC_PER_SEC);
	CU_ASSERT_FALSE (s4_add (NULL, trans, "a", val, "b", val, "src"));

	CU_ASSERT_FALSE (s4_commit (trans));
	CU_ASSERT_EQUAL (s4_errno (), S4E_DEADLOCK);
}

CASE (test_deadlock) {
	GThread *t1, *t2;
	_mem_open ();

	t1 = g_thread_create (_dead_thread_one, NULL, TRUE, NULL);
	t2 = g_thread_create (_dead_thread_two, NULL, TRUE, NULL);

	g_thread_join (t1);
	g_thread_join (t2);

	_mem_close ();
}

CASE (test_failed) {
	s4_transaction_t *trans;
	_mem_open ();

	trans = s4_begin (s4, 0);
	CU_ASSERT_PTR_NOT_NULL (trans);
	CU_ASSERT_FALSE (s4_del (NULL, trans, "a", val, "b", val, "src"));
	CU_ASSERT_FALSE (s4_commit (trans));
	CU_ASSERT_EQUAL (s4_errno (), S4E_EXECUTE);

	_mem_close ();
}

CASE (test_abort) {
	s4_transaction_t *trans;
	_mem_open ();

	trans = s4_begin (s4, 0);
	CU_ASSERT_PTR_NOT_NULL (trans);
	CU_ASSERT_TRUE (s4_add (NULL, trans, "a", val, "b", val, "src"));
	s4_abort (trans);

	trans = s4_begin (s4, 0);
	CU_ASSERT_PTR_NOT_NULL (trans);
	CU_ASSERT_TRUE (s4_add (NULL, trans, "a", val, "b", val, "src"));
	CU_ASSERT_TRUE (s4_commit (trans));

	trans = s4_begin (s4, 0);
	CU_ASSERT_PTR_NOT_NULL (trans);
	CU_ASSERT_FALSE (s4_add (NULL, trans, "a", val, "b", val, "src"));
	CU_ASSERT_FALSE (s4_commit (trans));
	CU_ASSERT_EQUAL (s4_errno (), S4E_EXECUTE);

	_mem_close ();
}
