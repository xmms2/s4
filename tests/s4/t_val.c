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
#include <glib.h>

SETUP (Value) {
	return 0;
}

CLEANUP () {
	return 0;
}

CASE (test_string) {
	s4_val_t *a, *b;
	const char *sa, *sb;
	int32_t i;

	a = s4_val_new_string ("ASDF");
	CU_ASSERT_PTR_NOT_NULL_FATAL (a);
	b = s4_val_new_string ("asdf");
	CU_ASSERT_PTR_NOT_NULL_FATAL (b);

	CU_ASSERT (s4_val_get_str (a, &sa));
	CU_ASSERT (s4_val_get_str (b, &sb));

	CU_ASSERT_FALSE (s4_val_get_int (a, &i));

	CU_ASSERT_EQUAL (strcmp (sa, "ASDF"), 0);
	CU_ASSERT_EQUAL (strcmp (sb, "asdf"), 0);

	CU_ASSERT (s4_val_get_casefolded_str (a, &sa));
	CU_ASSERT (s4_val_get_casefolded_str (b, &sb));

	CU_ASSERT_EQUAL (strcmp (sa, sb), 0);

	s4_val_free (a);
	s4_val_free (b);
}

CASE (test_integer) {
	s4_val_t *a, *b;
	const char *str;
	int32_t i;

	a = s4_val_new_int (1);
	b = s4_val_new_int (2);

	CU_ASSERT (s4_val_get_int (a, &i));
	CU_ASSERT_EQUAL (i, 1);
	CU_ASSERT (s4_val_get_int (b, &i));
	CU_ASSERT_EQUAL (i, 2);

	CU_ASSERT_FALSE (s4_val_get_str (a, &str));

	s4_val_free (a);
	s4_val_free (b);
}

CASE (test_copy) {
	s4_val_t *a, *b;

	a = s4_val_new_string ("asdf");
	b = s4_val_copy (a);

	CU_ASSERT_EQUAL (s4_val_cmp (a, b, 0), 0);
	CU_ASSERT_EQUAL (s4_val_cmp (a, b, 1), 0);

	s4_val_free (a);
	s4_val_free (b);

	a = s4_val_new_int (10);
	b = s4_val_copy (a);

	CU_ASSERT_EQUAL (s4_val_cmp (a, b, 0), 0);
	CU_ASSERT_EQUAL (s4_val_cmp (a, b, 1), 0);

	s4_val_free (a);
	s4_val_free (b);
}

CASE (test_cmp) {
	s4_val_t *ia, *ib;
	s4_val_t *sa, *sb;
	s4_val_t *is;

	ia = s4_val_new_int (1);
	ib = s4_val_new_int (3);
	sa = s4_val_new_string ("a");
	sb = s4_val_new_string ("B");
	is = s4_val_new_string ("2");

	CU_ASSERT (s4_val_cmp (ia, ib, S4_CMP_CASELESS) < 0);
	CU_ASSERT (s4_val_cmp (ib, ia, S4_CMP_CASELESS) > 0);
	CU_ASSERT (s4_val_cmp (ia, ia, S4_CMP_CASELESS) == 0);
	CU_ASSERT (s4_val_cmp (ia, ib, S4_CMP_BINARY) < 0);
	CU_ASSERT (s4_val_cmp (ib, ia, S4_CMP_BINARY) > 0);
	CU_ASSERT (s4_val_cmp (ia, ia, S4_CMP_BINARY) == 0);
	CU_ASSERT (s4_val_cmp (ia, ib, S4_CMP_COLLATE) < 0);
	CU_ASSERT (s4_val_cmp (ib, ia, S4_CMP_COLLATE) > 0);
	CU_ASSERT (s4_val_cmp (ia, ia, S4_CMP_COLLATE) == 0);

	CU_ASSERT (s4_val_cmp (sa, sb, S4_CMP_BINARY) > 0);
	CU_ASSERT (s4_val_cmp (sb, sa, S4_CMP_BINARY) < 0);
	CU_ASSERT (s4_val_cmp (sa, sa, S4_CMP_BINARY) == 0);
	CU_ASSERT (s4_val_cmp (sa, sb, S4_CMP_CASELESS) < 0);
	CU_ASSERT (s4_val_cmp (sb, sa, S4_CMP_CASELESS) > 0);
	CU_ASSERT (s4_val_cmp (sa, sa, S4_CMP_CASELESS) == 0);

	CU_ASSERT (s4_val_cmp (ia, is, S4_CMP_BINARY) > 0);
	CU_ASSERT (s4_val_cmp (ib, is, S4_CMP_BINARY) > 0);
	CU_ASSERT (s4_val_cmp (ia, is, S4_CMP_CASELESS) > 0);
	CU_ASSERT (s4_val_cmp (ib, is, S4_CMP_CASELESS) > 0);
	CU_ASSERT (s4_val_cmp (is, ia, S4_CMP_BINARY) < 0);
	CU_ASSERT (s4_val_cmp (is, ib, S4_CMP_BINARY) < 0);
	CU_ASSERT (s4_val_cmp (is, ia, S4_CMP_CASELESS) < 0);
	CU_ASSERT (s4_val_cmp (is, ib, S4_CMP_CASELESS) < 0);

	CU_ASSERT (s4_val_cmp (is, ia, S4_CMP_COLLATE) > 0);
	CU_ASSERT (s4_val_cmp (is, ib, S4_CMP_COLLATE) < 0);
	CU_ASSERT (s4_val_cmp (ia, is, S4_CMP_COLLATE) < 0);
	CU_ASSERT (s4_val_cmp (ib, is, S4_CMP_COLLATE) > 0);

	s4_val_free (ia);
	s4_val_free (ib);
	s4_val_free (sa);
	s4_val_free (sb);
	s4_val_free (is);
}
