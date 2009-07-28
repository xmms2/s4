#include "s4_be.h"
#include "be.h"
#include <string.h>

typedef struct string_val_St {
	int id;
	int ref_count;
} string_val_t;


static void setup_dbts (DBT *key, DBT *data,
		const char *str, string_val_t *val)
{
	memset (key, 0, sizeof (DBT));
	memset (data, 0, sizeof (DBT));

	key->size = strlen (str) + 1;
	key->data = (void*)str;

	data->data = val;
	data->ulen = sizeof (string_val_t);
}

int strtab_associate (DB *db, const DBT *key, const DBT *data, DBT *result)
{
	/* We only want the id as a secondary key, not the ref count */
	memset (result, 0, sizeof (DBT));
	result->size = sizeof (int);
	result->data = data->data;
	return 0;
}

int s4be_st_ref (s4be_t *s4, const char *str)
{
	DBT key, data;
	DB_TXN *tid;
	int ret;
	static int id = 0;
	string_val_t strval;
	int tries = 10;

	setup_dbts (&key, &data, str, &strval);
	data.flags = DB_DBT_USERMEM;

	while (tries--) {
		s4->env->txn_begin (s4->env, NULL, &tid, DB_TXN_NOSYNC);

		ret = s4->str_db->get (s4->str_db, tid, &key, &data, DB_RMW);

		if (ret == DB_NOTFOUND) {
			strval.id = ++id;
			strval.ref_count = 1;
		} else if (!ret) {
			strval.ref_count++;
		} else {
			printf("Error\n");
			tid->abort (tid);
			continue;
		}

		setup_dbts (&key, &data, str, &strval);
		data.size = sizeof (string_val_t);

		ret = s4->str_db->put (s4->str_db, tid, &key, &data, 0);

		if (ret) {
			printf("Error2 %i\n", ret);
			tid->abort (tid);
			continue;
		} else {
			tid->commit (tid, DB_TXN_NOSYNC);
			break;
		}
	}

	if (!tries)
		return -1;

	return strval.id;
}

int s4be_st_unref (s4be_t *s4, const char *str)
{
	DBT key, data;
	int ret;
	string_val_t strval;


	setup_dbts (&key, &data, str, &strval);
	data.flags = DB_DBT_USERMEM;

	ret = s4->str_db->get (s4->str_db, NULL, &key, &data, 0);

	if (ret) {
		/* Error handling */
		printf("Error3\n");
		return -1;
	} else if (strval.ref_count <= 1) {
		ret = s4->str_db->del (s4->str_db, NULL, &key, 0);
		if (ret) {
			printf("Error4\n");
			return -1;
			/* Error handling */
		}
		strval.ref_count = 0;
	} else {
		strval.ref_count--;
		setup_dbts (&key, &data, str, &strval);
		data.size = sizeof (string_val_t);
		s4->str_db->put (s4->str_db, NULL, &key, &data, 0);
	}

	return strval.ref_count;
}

int s4be_st_lookup (s4be_t *s4, const char *str)
{
	DBT key, data;
	int ret;
	string_val_t strval;

	setup_dbts (&key, &data, str, &strval);
	data.flags = DB_DBT_USERMEM;
	
	ret = s4->str_db->get (s4->str_db, NULL, &key, &data, 0);

	if (ret) {
		/* Error handling */
		return -1;
	}

	return strval.id;
}

char *s4be_st_reverse (s4be_t *s4, int id)
{
	DBT key, data, pkey;
	int ret;

	memset (&key, 0, sizeof (key));
	memset (&pkey, 0, sizeof (pkey));
	memset (&data, 0, sizeof (data));

	key.data = &id;
	key.size = sizeof (int);
	pkey.flags = DB_DBT_MALLOC;

	ret = s4->str_rev_db->pget (s4->str_rev_db, NULL, &key, &pkey, &data, 0);

	if (ret) {
		printf ("Error4 %i\n", ret);
		/* Error handling */
		return NULL;
	}

	return pkey.data;
}
