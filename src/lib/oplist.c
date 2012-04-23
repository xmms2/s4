#include "s4_priv.h"
#include <stdlib.h>

typedef enum {
	OP_ADD,
	OP_DEL,
	OP_WRITING
} op_type_t;

typedef struct {
	op_type_t type;

	const char *key_a, *key_b, *src;
	const s4_val_t *val_a, *val_b;
} op_t;

struct oplist_St {
	s4_transaction_t *trans;
	GList *ops, *cur;
};

oplist_t *_oplist_new (s4_transaction_t *trans)
{
	oplist_t *ret = malloc (sizeof (oplist_t));
	ret->ops = NULL;
	ret->cur = NULL;
	ret->trans = trans;

	return ret;
}

s4_t *_oplist_get_db (oplist_t *list)
{
	return _transaction_get_db (list->trans);
}

s4_transaction_t *_oplist_get_trans (oplist_t *list)
{
	return list->trans;
}

void _oplist_free (oplist_t *list)
{
	g_list_foreach (list->ops, (GFunc)free, NULL);
	g_list_free (list->ops);
	free (list);
}

void _oplist_insert_add (oplist_t *list,
		const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b,
		const char *src)
{
	op_t *op = malloc (sizeof (op_t));
	op->type = OP_ADD;
	op->key_a = key_a;
	op->key_b = key_b;
	op->src = src;
	op->val_a = val_a;
	op->val_b = val_b;

	list->ops = g_list_prepend (list->ops, op);
}

void _oplist_insert_del (oplist_t *list,
		const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b,
		const char *src)
{
	op_t *op = malloc (sizeof (op_t));
	op->type = OP_DEL;
	op->key_a = key_a;
	op->key_b = key_b;
	op->src = src;
	op->val_a = val_a;
	op->val_b = val_b;

	list->ops = g_list_prepend (list->ops, op);
}

void _oplist_insert_writing (oplist_t *list)
{
	op_t *op = malloc (sizeof (op_t));
	op->type = OP_WRITING;

	list->ops = g_list_prepend (list->ops, op);
}

int _oplist_next (oplist_t *list)
{
	if (list->ops == NULL) {
		return 0;
	} else if (list->cur == NULL) {
		list->cur = g_list_last (list->ops);
	} else if (list->cur != list->ops) {
		list->cur = g_list_previous (list->cur);
	} else {
		return 0;
	}

	return 1;
}

void _oplist_first (oplist_t *list)
{
	list->cur = NULL;
}

void _oplist_last (oplist_t *list)
{
	list->cur = list->ops;
}

int _oplist_get_add (oplist_t *list,
		const char **key_a, const s4_val_t **val_a,
		const char **key_b, const s4_val_t **val_b,
		const char **src)
{
	op_t *op;

	if (list->cur == NULL)
		return 0;

	op = list->cur->data;

	if (op->type != OP_ADD)
		return 0;

	*key_a = op->key_a;
	*key_b = op->key_b;
	*src = op->src;
	*val_a = op->val_a;
	*val_b = op->val_b;

	return 1;
}

int _oplist_get_del (oplist_t *list,
		const char **key_a, const s4_val_t **val_a,
		const char **key_b, const s4_val_t **val_b,
		const char **src)
{
	op_t *op;

	if (list->cur == NULL)
		return 0;

	op = list->cur->data;

	if (op->type != OP_DEL)
		return 0;

	*key_a = op->key_a;
	*key_b = op->key_b;
	*src = op->src;
	*val_a = op->val_a;
	*val_b = op->val_b;

	return 1;
}

int _oplist_get_writing (oplist_t *list)
{
	op_t *op;

	if (list->cur == NULL)
		return 0;

	op = list->cur->data;

	if (op->type != OP_WRITING)
		return 0;

	return 1;
}

int _oplist_rollback (oplist_t *list)
{
	for (; list->cur != NULL; list->cur = g_list_next (list->cur)) {
		const char *key_a, *key_b, *src;
		const s4_val_t *val_a, *val_b;

		if (_oplist_get_add (list, &key_a, &val_a, &key_b, &val_b, &src)) {
			_s4_del (list->trans, key_a, val_a, key_b, val_b, src);
		} else if (_oplist_get_del (list, &key_a, &val_a, &key_b, &val_b, &src)) {
			_s4_add (list->trans, key_a, val_a, key_b, val_b, src);
		}
	}

	return 1;
}

int _oplist_execute (oplist_t *list, int rollback_on_failure)
{
	_oplist_first (list);

	while (_oplist_next (list)) {
		const char *key_a, *key_b, *src;
		const s4_val_t *val_a, *val_b;
		int ret = 1;

		if (_oplist_get_add (list, &key_a, &val_a, &key_b, &val_b, &src)) {
			ret = _s4_add (list->trans, key_a, val_a, key_b, val_b, src);
		} else if (_oplist_get_del (list, &key_a, &val_a, &key_b, &val_b, &src)) {
			ret = _s4_del (list->trans, key_a, val_a, key_b, val_b, src);
		}

		if (!ret && rollback_on_failure) {
			_oplist_rollback (list);
			s4_set_errno (S4E_EXECUTE);
			return 0;
		}
	}

	return 1;
}
