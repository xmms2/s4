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
	s4_set_t *ret, *tmp;
	int i;

	ret = tmp = NULL;

	for (i = 0; values[i] != -1; i++) {
		if (tmp == NULL) {
			ret = tmp = malloc (sizeof (s4_set_t));
		}
		else {
			tmp->next = malloc (sizeof (s4_set_t));
			tmp = tmp->next;
		}

		tmp->entry.key_i = tmp->entry.val_i = tmp->entry.src_i = values[i];
		tmp->entry.key_s = tmp->entry.val_s = tmp->entry.src_s = NULL;
	}

	if (tmp != NULL)
		tmp->next = NULL;

	return ret;
}

static void test_set (s4_set_t *set, int *values)
{
	while (set != NULL && *values != -1) {
		CU_ASSERT_EQUAL (set->entry.key_i, *values);
		CU_ASSERT_EQUAL (set->entry.val_i, *values);
		CU_ASSERT_EQUAL (set->entry.src_i, *values);
		set = s4_set_next (set);
		values++;
	}

	CU_ASSERT_EQUAL (*values, -1);
	CU_ASSERT_PTR_NULL (set);

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
