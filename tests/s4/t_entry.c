#include "xcu.h"
#include "s4.h"
#include <stdio.h>
#include <glib.h>

s4_t *s4;
char *name;

SETUP (s4_entry) {
	if (!g_thread_get_initialized ())
		g_thread_init (NULL);

	name = tmpnam (NULL);
	s4 = s4_open (name);

	return s4 == NULL;
}

CLEANUP () {
	s4_close (s4);
	unlink (name);

	return 0;
}

CASE (entry_get_s) {
	s4_entry_t *entry;

	entry = s4_entry_get_s (s4, "test", "asdf");

	CU_ASSERT_STRING_EQUAL (entry->key_s, "test");
	CU_ASSERT_STRING_EQUAL (entry->val_s, "asdf");

	s4_entry_free (entry);
}

CASE (entry_get_i) {
	s4_entry_t *entry;

	entry = s4_entry_get_i (s4, "test", 12345678);

	CU_ASSERT_STRING_EQUAL (entry->key_s, "test");
	CU_ASSERT_EQUAL (entry->val_i, 12345678);

	s4_entry_free (entry);
}

static void test_set (s4_set_t *set, int *values)
{
	while (set != NULL && *values != -1) {
		s4_entry_fillin (s4, &set->entry);

		CU_ASSERT_STRING_EQUAL (set->entry.key_s, "prop");
		CU_ASSERT_EQUAL (set->entry.val_i, *values);
		CU_ASSERT_STRING_EQUAL (set->entry.src_s, "testcase");

		set = s4_set_next (set);
		values++;
	}

	CU_ASSERT_EQUAL (*values, -1);
	CU_ASSERT_PTR_NULL (set);

	s4_set_free (set);
}

CASE (entry_contain) {
	s4_entry_t *entry, *prop;
	int properties[] = {1, 2, 3, 4, 5, 6, -1};
	int entries[] = {1, -1};
	int i;

	entry = s4_entry_get_i (s4, "prop", 1);

	for (i = 0; properties[i] != -1; i++) {
		prop = s4_entry_get_i (s4, "prop", properties[i]);
		CU_ASSERT_EQUAL (s4_entry_add (s4, entry, prop, "testcase"), 0);
		s4_entry_free (prop);
	}

	test_set (s4_entry_contains (s4, entry), properties);

	for (i = 0; properties[i] != -1; i++) {
		prop = s4_entry_get_i (s4, "prop", properties[i]);

		test_set (s4_entry_contained (s4, prop), entries);
	}

	test_set (s4_entry_get_property (s4, entry, "prop"), properties);

	for (i = 0; properties[i] != -1; i++) {
		prop = s4_entry_get_i (s4, "prop", properties[i]);
		CU_ASSERT_EQUAL (s4_entry_del (s4, entry, prop, "testcase"), 0);
		CU_ASSERT_EQUAL (s4_entry_del (s4, entry, prop, "testcase"), -1);
		s4_entry_free (prop);
	}
}

CASE (entry_smaller_greater) {
	s4_entry_t *entry, *prop;
	int properties[] = {1, 2, 3, 4, 5, 6, 7, -1};
	int smaller[] = {1, 2, 3, -1};
	int greater[] = {5, 6, 7, -1};
	int expect[] = {1, -1};
	int i;


	for (i = 0; properties[i] != -1; i++) {
		prop = s4_entry_get_i (s4, "prop", properties[i]);
		CU_ASSERT_EQUAL (s4_entry_add (s4, prop, prop, "testcase"), 0);
		s4_entry_free (prop);
	}

	entry = s4_entry_get_i (s4, "prop", 4);

	test_set (s4_entry_smaller (s4, entry), smaller);
	test_set (s4_entry_greater (s4, entry), greater);

	entry = s4_entry_get_i (s4, "prop", 1);

	for (i = 0; properties[i] != -1; i++) {
		prop = s4_entry_get_i (s4, "prop", properties[i]);
		CU_ASSERT_EQUAL (s4_entry_del (s4, prop, prop, "testcase"), 0);
		CU_ASSERT_EQUAL (s4_entry_del (s4, prop, prop, "testcase"), -1);
		s4_entry_free (prop);
	}
}

CASE (entry_fillin) {
	s4_entry_t *entry, *prop;

	entry = s4_entry_get_s (s4, "a", "b");
	prop = s4_entry_get_s (s4, "c", "d");
	CU_ASSERT_EQUAL (s4_entry_add (s4, entry, prop, "e"), 0);

	entry->src_i = entry->val_i = entry->key_i = 0;
	entry->src_s = entry->val_s = entry->key_s = NULL;

	entry->key_s = strdup ("a");
	s4_entry_fillin (s4, entry);

	entry->val_i = entry->key_i;
	s4_entry_fillin (s4, entry);

	CU_ASSERT_PTR_NOT_NULL (entry->val_s);

	if (entry->val_s != NULL)
		CU_ASSERT_STRING_EQUAL (entry->val_s, entry->key_s);

	s4_entry_free (entry);
	s4_entry_free (prop);

	entry = s4_entry_get_s (s4, "a", "b");
	prop = s4_entry_get_s (s4, "c", "d");
	CU_ASSERT_EQUAL (s4_entry_del (s4, entry, prop, "e"), 0);

	s4_entry_free (entry);
	s4_entry_free (prop);
}
