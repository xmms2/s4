#include <s4.h>
#include "s4_be.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>


/**
 *
 * @defgroup S4 S4
 * @brief A database backend for XMMS2
 *
 * @{
 */

/**
 * Open an S4 database
 *
 * @param filename The name of the file containing the database
 * @param flags Define some options. S4_VERIFY will verify the file after
 * opening it, if it is corrupted and S4_RECOVER is set it will try to
 * recover it, otherwise it will return NULL. If S4_RECOVERY is set but
 * S4_VERIFY is not, it will try to recover the database without checking
 * if it is actually necessary. For the verify flags see s4_verify.
 * If S4_NEW makes sure it creates a new file, if he file already exists
 * it returns NULL.
 * @return A pointer to an s4_t, or NULL if something went wrong.
 */
s4_t *s4_open (const char *filename, int flags)
{
	s4be_t *be;
	s4_t *s4;
	int verified = 0;

	be = s4be_open (filename, flags);
	if (be == NULL) {
		return NULL;
	}

	if (flags & S4_VERIFY) {
		s4_t tmp;
		tmp.be = be;
		verified = s4_verify (&tmp, flags & S4_VERIFY_MASK);

		if (verified == 0 && !(flags & S4_RECOVER)) {
			s4be_close (be);
			return NULL;
		}
	}

	if (verified == 0 && (flags & S4_RECOVER)) {
		char buf[4096];
		s4_t tmp;
		strcpy (buf, filename);
		strcat (buf, ".rec");

		S4_INFO ("%s is corrupted, trying to recover..", filename);

		tmp.be = be;

		if (!s4_recover (&tmp, buf)) {
			s4be_close (be);
			return NULL;
		}

		s4be_close (be);

		g_unlink (filename);
		g_rename (buf, filename);

		be = s4be_open (filename, flags);

		if (be == NULL) {
			S4_ERROR ("Could not open the recovered database! This is bad..");
			return NULL;
		}
	}

	s4 = malloc (sizeof (s4_t));
	s4->be = be;

	return s4;
}


int s4_close (s4_t *s4)
{
	int ret = s4be_close (s4->be);

	free (s4);

	return ret;
}

static int treecmp (gconstpointer pa, gconstpointer pb, gpointer foo)
{
	int a, b;

	a = *(int*)pa;
	b = *(int*)pb;

	if (a < b)
		return -1;
	if (a > b)
		return 1;

	return 0;
}

static void inc_tree (GTree *tree, int32_t str)
{
	int *val = g_tree_lookup (tree, &str);
	int *key = malloc (sizeof (int));
	int *new = malloc (sizeof (int));

	*key = str;
	if (val == NULL) {
		*new = 1;
	} else {
		*new = *val + 1;
	}
	g_tree_insert (tree, key, new);
}

struct check_struct {
	s4be_t *be;
	GTree *tree;
	int errors;
};

static void check_refs (int32_t node, void *u)
{
	struct check_struct *info = u;
	int count = s4be_st_refcount (info->be, node);
	int *val = g_tree_lookup (info->tree, &node);

	if (*val != count) {
		info->errors++;
		S4_ERROR ("Wrong ref count on %s (%i) - is %i, should be %i",
				s4be_st_reverse (info->be, node),node,  count, *val);
	}
}

static void count_refs (s4_entry_t *e, s4_entry_t *p, void *u)
{
	GTree *t = u;

	if (e->type == ENTRY_INT) {
		inc_tree (t, -e->key_i);
	} else {
		inc_tree (t, e->key_i);
		inc_tree (t, e->val_i);
	}
	if (p->type == ENTRY_INT) {
		inc_tree (t, -p->key_i);
	} else {
		inc_tree (t, p->key_i);
		inc_tree (t, p->val_i);
	}
	inc_tree (t, p->src_i);
}

static int verify_refcount (s4_t *s4)
{
	GTree *tree;
	struct check_struct info;

	tree = g_tree_new_full (treecmp, NULL, free, free);

	info.tree = tree;
	info.be = s4->be;
	info.errors = 0;

	s4be_ip_foreach (s4->be, count_refs, tree);
	s4be_st_foreach (s4->be, check_refs, &info);

	if (info.errors) {
		S4_ERROR ("Found %i errors in the refcounting!", info.errors);
	}

	g_tree_destroy (tree);

	return info.errors != 0;
}

/**
 * Check the database for corruption
 *
 * @param s4 The database to check
 * @param flags Specifies what will be checked. Pass 0 to do a quick check,
 * add S4_VERIFY_THOROUGH if you want a thorough check of the backend and
 * add S4_VERFIY_REFCOUNT if you also want to check that the refcount in the
 * string-store is correct.
 *
 * @return 1 if everything is okay, 0 otherwise.
 */
int s4_verify (s4_t *s4, int flags) {
	int ret = s4be_verify (s4->be, flags & S4_VERIFY_THOROUGH);

	if (flags & S4_VERIFY_REFCOUNT) {
		ret &= verify_refcount (s4);
	}

	return ret;
}

int s4_recover (s4_t *s4, const char *name)
{
	s4be_t *rec;
	int ret = 1;

	if (g_file_test (name, G_FILE_TEST_EXISTS)) {
		S4_ERROR ("%s already exists, can't recover", name);
		return 0;
	}

	rec = s4be_open (name, S4_NEW);

	if (rec == NULL) {
		return 0;
	}

	ret = s4be_recover (s4->be, rec);
	s4be_close (rec);

	return ret;
}

/**
 * @}
 */
