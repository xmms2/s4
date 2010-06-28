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

SETUP (Condition) {
	return 0;
}

CLEANUP () {
	return 0;
}

#define TEST_COND(c, s, m) ((m)?(CU_ASSERT_FALSE (test_cond ((c), (s)))):(CU_ASSERT (test_cond ((c), (s)))))

static int test_cond (s4_condition_t *cond, const char *str)
{
	s4_val_t *val = s4_val_new_string (str);
	int ret = s4_cond_get_filter_function (cond)(val, cond);
	s4_val_free (val);
	return ret;
}

static s4_condition_t *create_cond (s4_filter_type_t type, const char *str, int flags)
{
	s4_val_t *val = s4_val_new_string (str);
	s4_condition_t *cond = s4_cond_new_filter (type, str, val, NULL, flags);
	s4_val_free (val);
	return cond;
}

CASE (test_equal) {
	s4_condition_t *cond;

	cond = create_cond (S4_FILTER_EQUAL, "foobar", 0);
	TEST_COND (cond, "FOOBAR", 1);
	TEST_COND (cond, "foobar", 1);
	s4_cond_free (cond);

	cond = create_cond (S4_FILTER_EQUAL, "foobar", S4_COND_CASESENS);
	TEST_COND (cond, "foobar", 1);
	TEST_COND (cond, "FOOBAR", 0);
	s4_cond_free (cond);
}

CASE (test_greater) {
	s4_condition_t *cond;

	cond = create_cond (S4_FILTER_GREATER, "foobar", 0);
	TEST_COND (cond, "goobar", 1);
	TEST_COND (cond, "GOOBAR", 1);
	s4_cond_free (cond);

	cond = create_cond (S4_FILTER_GREATER, "foobar", S4_COND_CASESENS);
	TEST_COND (cond, "goobar", 1);
	TEST_COND (cond, "GOOBAR", 0);
	s4_cond_free (cond);
}

CASE (test_smaller) {
	s4_condition_t *cond;

	cond = create_cond (S4_FILTER_SMALLER, "hoobar", 0);
	TEST_COND (cond, "goobar", 1);
	TEST_COND (cond, "GOOBAR", 1);
	s4_cond_free (cond);

	cond = create_cond (S4_FILTER_SMALLER, "HOOBAR", S4_COND_CASESENS);
	TEST_COND (cond, "goobar", 0);
	TEST_COND (cond, "GOOBAR", 1);
	s4_cond_free (cond);
}

CASE (test_match) {
	s4_condition_t *cond;

	cond = create_cond (S4_FILTER_MATCH, "a?c*", 0);
	TEST_COND (cond, "abcd", 1);
	TEST_COND (cond, "ABCD", 1);
	TEST_COND (cond, "axc", 1);
	TEST_COND (cond, "AXC", 1);
	TEST_COND (cond, "bcd", 0);
	TEST_COND (cond, "BCD", 0);
	s4_cond_free (cond);

	cond = create_cond (S4_FILTER_MATCH, "a?c*", S4_COND_CASESENS);
	TEST_COND (cond, "abcd", 1);
	TEST_COND (cond, "ABCD", 0);
	TEST_COND (cond, "axc", 1);
	TEST_COND (cond, "AXC", 0);
	TEST_COND (cond, "bcd", 0);
	TEST_COND (cond, "BCD", 0);
	s4_cond_free (cond);
}

CASE (test_exists) {
	s4_condition_t *cond;

	cond = create_cond (S4_FILTER_EXISTS, "asdf", 0);
	TEST_COND (cond, "abc", 1);
	TEST_COND (cond, "jkl", 1);
	TEST_COND (cond, "asdf", 1);
	s4_cond_free (cond);
}

static s4_condition_t *create_combiner (s4_combine_type_t type, s4_condition_t *op_a, s4_condition_t *op_b)
{
	GList *ops = NULL;
	ops = g_list_prepend (ops, op_a);
	ops = g_list_prepend (ops, op_b);

	return s4_cond_new_combiner (type, ops);
}

#define TEST_COMBINER(c,s,m) ((m)?(CU_ASSERT_FALSE (test_combiner(c,s))):(CU_ASSERT(test_combiner(c,s))))

static int test_combiner (s4_condition_t *cond, const char *str)
{
	return s4_cond_get_combine_function (cond)(cond, (check_function_t)test_cond, (void*)str);
}

CASE (test_and) {
	s4_condition_t *cond, *a, *b;

	a = create_cond (S4_FILTER_EQUAL, "a", 0);
	b = create_cond (S4_FILTER_EQUAL, "b", 0);
	cond = create_combiner (S4_COMBINE_AND, a, b);
	TEST_COMBINER (cond, "a", 0);
	TEST_COMBINER (cond, "b", 0);
	s4_cond_free (cond);

	a = create_cond (S4_FILTER_EQUAL, "a", 0);
	b = create_cond (S4_FILTER_EQUAL, "a", 0);
	cond = create_combiner (S4_COMBINE_AND, a, b);
	TEST_COMBINER (cond, "a", 1);
	TEST_COMBINER (cond, "b", 0);
	s4_cond_free (cond);
}

CASE (test_or) {
	s4_condition_t *cond, *a, *b;

	a = create_cond (S4_FILTER_EQUAL, "a", 0);
	b = create_cond (S4_FILTER_EQUAL, "b", 0);
	cond = create_combiner (S4_COMBINE_OR, a, b);
	TEST_COMBINER (cond, "a", 1);
	TEST_COMBINER (cond, "b", 1);
	TEST_COMBINER (cond, "c", 0);
	s4_cond_free (cond);

	a = create_cond (S4_FILTER_EQUAL, "a", 0);
	b = create_cond (S4_FILTER_EQUAL, "a", 0);
	cond = create_combiner (S4_COMBINE_AND, a, b);
	TEST_COMBINER (cond, "a", 1);
	TEST_COMBINER (cond, "b", 0);
	s4_cond_free (cond);
}

CASE (test_not) {
	s4_condition_t *cond, *a, *b;

	a = create_cond (S4_FILTER_EQUAL, "a", 0);
	b = create_cond (S4_FILTER_EQUAL, "b", 0); /* Dummy, NOT only takes the first operand into account */
	cond = create_combiner (S4_COMBINE_NOT, b, a);
	TEST_COMBINER (cond, "a", 0);
	TEST_COMBINER (cond, "b", 1);
	s4_cond_free (cond);
}

static int simple_filter (s4_val_t *value, s4_condition_t *cond)
{
	return value != s4_cond_get_funcdata (cond);
}

CASE (test_custom_filter) {
	s4_condition_t *cond;

	s4_val_t *val = (void*)0x1234;

	cond = s4_cond_new_custom_filter (simple_filter, val, NULL, "asdf", NULL, 0);
	CU_ASSERT_FALSE (s4_cond_get_filter_function (cond)(val, cond));
	CU_ASSERT (s4_cond_get_filter_function (cond)((s4_val_t*)0x4312, cond));
	s4_cond_free (cond);
}

static int xor_combiner (s4_condition_t *cond, check_function_t func, void *check_data)
{
	int ret = 1;
	GList *i = s4_cond_get_operands (cond);
	for (; i != NULL; i = g_list_next (i)) {
		int tmp = func (i->data, check_data);

		ret = !!ret ^ !!tmp;
	}

	return ret;
}

CASE (test_custom_combiner) {
	s4_condition_t *cond, *a, *b;
	GList *ops;

	a = create_cond (S4_FILTER_EQUAL, "a", 0);
	b = create_cond (S4_FILTER_EQUAL, "b", 0);
	ops = g_list_prepend (NULL, a);
	ops = g_list_prepend (ops, b);
	cond = s4_cond_new_custom_combiner (xor_combiner, ops);
	TEST_COMBINER (cond, "a", 1);
	TEST_COMBINER (cond, "b", 1);
	TEST_COMBINER (cond, "c", 0);
	s4_cond_free (cond);

	a = create_cond (S4_FILTER_EQUAL, "a", 0);
	b = create_cond (S4_FILTER_EQUAL, "a", 0);
	ops = g_list_prepend (NULL, a);
	ops = g_list_prepend (ops, b);
	cond = s4_cond_new_custom_combiner (xor_combiner, ops);
	TEST_COMBINER (cond, "a", 0);
	TEST_COMBINER (cond, "b", 0);
	s4_cond_free (cond);
}
