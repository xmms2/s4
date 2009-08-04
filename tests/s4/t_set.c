#include "xcu.h"
#include "s4.h"
#include <stdlib.h>



SETUP (s4_set) {
	return 0;
}

CLEANUP () {
	return 0;
}

s4_set_t *create_set (int *values)
{
	s4_set_t *ret = s4_set_new (0);
	s4_entry_t entry;
	int i;

	for (i = 0; values[i] != -1; i++) {
		entry.key_i = entry.val_i = entry.src_i = values[i];
		entry.key_s = entry.val_s = entry.src_s = NULL;

		s4_set_insert (ret, &entry);
	}

	return ret;
}

static void test_set (s4_set_t *set, int *values)
{
	s4_entry_t *entry;

	s4_set_reset (set);

	entry = s4_set_next (set);

	while (entry != NULL && *values != -1) {
		CU_ASSERT_EQUAL (entry->key_i, *values);
		CU_ASSERT_EQUAL (entry->val_i, *values);
		CU_ASSERT_EQUAL (entry->src_i, *values);
		entry = s4_set_next (set);
		values++;
	}

	CU_ASSERT_EQUAL (*values, -1);
	CU_ASSERT_PTR_NULL (entry);

	s4_set_free (set);
}

CASE (test_set_intersection) {
	int s1_a[] = {1, 3, 5, 7, 9, 11, -1};
	int s1_b[] = {2, 4, 6, 8, 10, 12, -1};
	int s1_r[] = {-1};

	int s2_a[] = {1, 2, 3, 4, -1};
	int s2_b[] = {1, 3, 5, -1};
	int s2_r[] = {1, 3, -1};

	test_set (s4_set_intersection (create_set (s1_a), create_set (s1_b)), s1_r);
	test_set (s4_set_intersection (create_set (s1_b), create_set (s1_a)), s1_r);

	test_set (s4_set_intersection (create_set (s2_a), create_set (s2_b)), s2_r);
	test_set (s4_set_intersection (create_set (s2_b), create_set (s2_a)), s2_r);
}

CASE (test_set_union) {
	int s1_a[] = {1, 3, 5, 7, 9, 11, -1};
	int s1_b[] = {2, 4, 6, 8, 10, 12, -1};
	int s1_r[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, -1};

	int s2_a[] = {1, 2, 3, -1};

	test_set (s4_set_union (create_set (s1_a), create_set (s1_b)), s1_r);
	test_set (s4_set_union (create_set (s1_b), create_set (s1_a)), s1_r);

	test_set (s4_set_union (create_set (s2_a), NULL), s2_a);
	test_set (s4_set_union (NULL, create_set (s2_a)), s2_a);

	test_set (s4_set_union (create_set (s1_a), create_set (s1_a)), s1_a);
}

CASE (test_set_next) {
	int s1[] = {1, 2, 3, 4, 5, 6, 7, 8, -1};
	s4_set_t *s = create_set (s1);

	test_set (s, s1);
}

CASE (test_set_get) {
	int s[] = {1, 2, 3, 4, 5, 6, 7, 8, -1};
	s4_set_t *set = create_set (s);
	int i, size;

	for (i = 0, size = s4_set_size (set); i < size; i++) {
		s4_entry_t *entry = s4_set_get (set, i);

		CU_ASSERT_EQUAL (entry->key_i, s[i]);
		CU_ASSERT_EQUAL (entry->val_i, s[i]);
		CU_ASSERT_EQUAL (entry->src_i, s[i]);
	}

	CU_ASSERT_EQUAL (-1, s[i]);
}

/* This test relies on create_set using s4_set_insert! */
CASE (test_set_insert) {
	int s1[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, -1};
	int s1_r[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, -1};
	int s2[] = {1, 1, 1, 5, 5, 4, 3, 4, 4, -1};
	int s2_r[] = {1, 2, 3, 4, 5, -1};
	s4_set_t *set = create_set (s2);
	s4_entry_t entry;

	entry.key_i = entry.val_i = entry.src_i = 2;
	entry.key_s = entry.val_s = entry.src_s = NULL;
	CU_ASSERT_EQUAL (1, s4_set_insert (set, &entry));
	CU_ASSERT_EQUAL (0, s4_set_insert (set, &entry));
	CU_ASSERT_EQUAL (0, s4_set_insert (NULL, &entry));

	test_set (create_set (s1), s1_r);
	test_set (create_set (s2), s2_r);
}
