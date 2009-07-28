#include <stdlib.h>
#include <s4.h>


/**
 *
 * @defgroup Set Set
 * @ingroup S4
 * @brief A set with intersection and union
 *
 * @{
 */

static int set_compare (s4_set_t *a, s4_set_t *b)
{
	int ret;
	if (a == NULL)
		return 1;
	if (b == NULL)
		return -1;

	ret = (a->entry.key_i < b->entry.key_i)?-1:
		(a->entry.key_i > b->entry.key_i);

	if (!ret)
		ret = (a->entry.val_i < b->entry.val_i)?-1:
			(a->entry.val_i > b->entry.val_i);

	return ret;
}

static void _set_free (s4_set_t *set)
{
	s4_entry_free_strings (&set->entry);

	free (set);
}


/**
 * Return the intersection of a and b.
 * It will free the parts of a and b not used, you can in other words
 * NOT refer to a and b after calling s4_set_intersection.
 *
 * @param a One of the two sets
 * @param b The other of the two sets
 * @return The intersection of a and b
 */
s4_set_t *s4_set_intersection (s4_set_t *a, s4_set_t *b)
{
	s4_set_t *ret, *cur, *tmp;
	int c;

	cur = ret = NULL;

	while (a != NULL && b != NULL) {
		c = set_compare (a, b);

		if (c > 0) {
			tmp = b;
			b = b->next;
			_set_free (tmp);
		} else if (c < 0) {
			tmp = a;
			a = a->next;
			_set_free (tmp);
		} else {
			if (cur == NULL) {
				ret = cur = a;
			} else {
				cur->next = a;
				cur = cur->next;
			}

			a = a->next;
			tmp = b;
			b = b->next;
			_set_free (tmp);
		}
	}

	if (cur != NULL)
		cur->next = NULL;

	return ret;
}


/**
 * Return the union of two sets.
 * Note: It will free the two sets a and b
 *
 * @param a One of the two sets
 * @param b The other of the two sets
 * @return A new set that's the union of a and b
 */
s4_set_t *s4_set_union (s4_set_t *a, s4_set_t *b)
{
	s4_set_t *ret, *cur, *tmp;
	int c;

	cur = ret = NULL;

	while (a != NULL || b != NULL) {
		c = set_compare (a, b);

		if (c > 0) {
			tmp = b;
			b = b->next;
		} else if (c < 0) {
			tmp = a;
			a = a->next;
		} else {
			tmp = b;
			b = b->next;
			_set_free (tmp);

			tmp = a;
			a = a->next;
		}

		if (ret == NULL)
			ret = cur = tmp;
		else {
			cur->next = tmp;
			cur = cur->next;
		}
	}

	if (cur != NULL)
		cur->next = NULL;

	return ret;
}


s4_set_t *s4_set_next (s4_set_t *set)
{
	s4_set_t *ret = NULL;

	if (set != NULL) {
		ret = set->next;
		_set_free (set);
	}

	return ret;
}


void s4_set_free (s4_set_t *set)
{
	s4_set_t *tmp;
	while (set != NULL) {
		tmp = set->next;
		_set_free (set);
		set = tmp;
	}
}

/**
 * @}
 */
