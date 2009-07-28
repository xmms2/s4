#include "s4_be.h"
#include "be.h"
#include <stdlib.h>
#include <db.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>


int string_open (DB **db, DB_ENV *env, const char *table)
{
	int ret;

	if (!(ret = db_create (db, env, 0)) &&
	    !(ret = (*db)->open (*db, NULL, table, NULL, DB_BTREE,
				DB_AUTO_COMMIT | DB_CREATE, 0)));

	return ret;
}


int pair_open (DB **db, DB_ENV *env, const char *table)
{
	int ret;

	if (!(ret = db_create (db, env, 0)) &&
	    !(ret = (*db)->set_dup_compare (*db, intpair_compare)) &&
	    !(ret = (*db)->set_flags (*db, DB_DUPSORT)) &&
		!(ret = (*db)->open (*db, NULL, table, NULL, DB_BTREE,
				DB_AUTO_COMMIT | DB_CREATE, 0)));

	return ret;
}

int env_dir_create(const char *dir)
{
	struct stat sb;
	if (stat(dir, &sb) == 0)
		return 0;

	/* Create the directory, read/write/access owner only. */
	if (mkdir(dir, S_IRWXU) != 0) {
		fprintf(stderr,	"mkdir: %s: %s\n", dir, strerror(errno));
		return -1;
	}
}

int env_open (DB_ENV **env, const char *env_dir)
{
	int ret;

	if (env_dir_create (env_dir))
		return -1;

	if ((ret = db_env_create (env, 0)) != 0) {
		fprintf (stderr, "db_env_create: %s\n", db_strerror(ret));
		return -1;
	}

	(*env)->set_errpfx (*env, "s4");
	(*env)->set_errfile (*env, stderr);
	(*env)->set_flags (*env, DB_TXN_NOSYNC | DB_AUTO_COMMIT, 1);

	/* Do deadlock detection internally. */
	if ((ret = (*env)->set_lk_detect(*env, DB_LOCK_DEFAULT)) != 0) {
		(*env)->err(*env, ret, "set_lk_detect: DB_LOCK_DEFAULT");
		return -1;
	}

	if ((ret = (*env)->open (*env, env_dir,	DB_CREATE | DB_INIT_LOCK |
					DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN |
					DB_RECOVER, S_IRUSR | S_IWUSR)) != 0) {
		(*env)->close (*env, 0);
		fprintf (stderr, "env->open %s : %s\n", env_dir, db_strerror (ret));
		return -1;
	}

	return 0;
}


s4be_t *s4be_open (const char *filename)
{
	s4be_t *s4 = malloc (sizeof (s4be_t));
	int ret;

	memset (s4, 0, sizeof (s4_t));

	if ((ret = env_open (&s4->env, filename)))
		goto cleanup;

	if ((ret = string_open (&s4->str_db, s4->env, "string.db")))
		goto cleanup;
	if ((ret = string_open (&s4->str_rev_db, s4->env, "string_rev.db")))
		goto cleanup;

	if ((ret = s4->str_db->associate (s4->str_db, NULL, s4->str_rev_db,
					strtab_associate, 0)))
		goto cleanup;

	if ((ret = pair_open (&s4->pair_db, s4->env, "pair.db")))
		goto cleanup;
	if ((ret = pair_open (&s4->pair_rev_db, s4->env, "pair_rev.db")))
		goto cleanup;


	return s4;

cleanup:
	printf("Something went wrong\n");
	s4be_close (s4);
	return NULL;
}


int s4be_close (s4be_t *s4)
{
	if (s4->str_db) {
		s4->str_db->close (s4->str_db, 0);
	}
	if (s4->str_rev_db) {
		s4->str_rev_db->close (s4->str_rev_db, 0);
	}
	if (s4->pair_db) {
		s4->pair_db->close (s4->pair_db, 0);
	}
	if (s4->pair_rev_db) {
		s4->pair_rev_db->close (s4->pair_rev_db, 0);
	}
	if (s4->env) {
		s4->env->close (s4->env, 0);
	}
	free (s4);
	return 0;
}
