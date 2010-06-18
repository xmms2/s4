#include <s4.h>
#include "s4_priv.h"
#include <stdlib.h>

struct s4_result_St {
	s4_result_t *next;
	const char *key;
	s4_val_t *val;
	const char *src;
};

const s4_result_t *s4_result_next (const s4_result_t *res)
{
	return res->next;
}

const char *s4_result_get_key (const s4_result_t *res)
{
	return res->key;
}

const char *s4_result_get_src (const s4_result_t *res)
{
	return res->src;
}

const s4_val_t *s4_result_get_val (const s4_result_t *res)
{
	return res->val;
}

s4_result_t *s4_result_create (s4_result_t *next, const char *key, s4_val_t *val, const char *src)
{
	s4_result_t *ret = malloc (sizeof (s4_result_t));

	ret->next = next;
	ret->key = key;
	ret->val = val;
	ret->src = src;

	return ret;
}

void s4_result_free (s4_result_t *res)
{
	free (res);
}
