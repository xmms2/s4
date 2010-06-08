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

#include <s4.h>
#include "s4_be.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>

static GStaticPrivate errno = G_STATIC_PRIVATE_INIT;

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
 * @b The different flags you can pass:
 * <P>
 * @b S4_VERIFY
 * <BR>
 * 		S4 will check the database. If it is inconsistent and
 *		if S4_RECOVERYis set it will try to recover it.
 * </P><P>
 * @b S4_RECOVERY
 * <BR>
 * 		If S4_VERIFY is set the database will be recovered if it
 * 		is inconsistent. If S4_VERIFY is not set it will try to
 * 		recover the database anyway.
 * </P><P>
 * @b S4_NEW
 * <BR>
 * 		It will create a new file if one does not already exists.
 * 		If one exists it will fail and return NULL.
 * </P><P>
 * @b S4_EXISTS
 * <BR>
 * 		If the file does not exists it will fail and return NULL.
 * </P><P>
 * @b S4_SYNC_THREAD
 * <BR>
 * 		It will start a synchronisation thread that will flush the
 * 		database to disk in regular intervals.
 * </P>
 *
 * @param filename The name of the file containing the database
 * @param flags Zero or more of the flags bitwise-or'd.
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

			s4_set_errno (S4E_INCONS);

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

	s4 = calloc (1, sizeof (s4_t));
	s4->be = be;

	if (flags & S4_SYNC_THREAD) {
		if (!s4_start_sync_thread (s4)) {
			s4_close (s4);
			return NULL;
		}
	}

	return s4;
}

/**
 * Close an open S4 database
 *
 * @param s4 The database to close
 *
 */
int s4_close (s4_t *s4)
{
	int ret;

	s4_stop_sync_thread (s4);
	ret = s4be_close (s4->be);

	free (s4);

	return ret;
}

/**
 * Write all changes to disk
 *
 * @param s4 The database to sync
 *
 */
void s4_sync (s4_t *s4)
{
	s4be_sync (s4->be);
}

/* The loop for the sync thread */
static gpointer sync_thread (gpointer ptr)
{
	s4_t *s4 = ptr;
	GTimeVal tv;

	g_get_current_time (&tv);
	g_time_val_add (&tv, 60*1000);

	g_mutex_lock (s4->cond_mutex);

	while (!g_cond_timed_wait (s4->cond, s4->cond_mutex, &tv)) {
		s4_sync (s4);
		g_get_current_time (&tv);
		g_time_val_add (&tv, 60*1000);
	}

	g_mutex_unlock (s4->cond_mutex);

	return NULL;
}

/**
 * Start the synchronisation thread
 *
 * @param s4 The database to run the sync thread on
 * @return 1 if everything went okay, 0 otherwise.
 */
int s4_start_sync_thread (s4_t *s4)
{
	if (s4->cond != NULL || s4->cond_mutex != NULL || s4->s_thread != NULL)
		return 0;

	s4->cond = g_cond_new ();
	s4->cond_mutex = g_mutex_new ();
	s4->s_thread = g_thread_create (sync_thread, s4, TRUE, NULL);

	if (s4->s_thread == NULL) {
		g_mutex_free (s4->cond_mutex);
		g_cond_free (s4->cond);

		s4_set_errno (S4E_STHREAD);

		return 0;
	}

	return 1;
}

/**
 * Stop the synchronisation thread
 *
 * @param s4 The database to stop the sync thread in
 * @return 1 if everything went okay, 0 otherwise.
 *
 */
int s4_stop_sync_thread (s4_t *s4)
{
	if (s4->cond == NULL || s4->cond_mutex == NULL || s4->s_thread == NULL)
		return 0;

	g_mutex_lock (s4->cond_mutex);
	g_cond_signal (s4->cond);
	g_mutex_unlock (s4->cond_mutex);

	g_thread_join (s4->s_thread);

	g_mutex_free (s4->cond_mutex);
	g_cond_free (s4->cond);

	return 1;
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
	int count = s4be_st_get_refcount (info->be, node);
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

	return info.errors == 0;
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

static void set_refs (int32_t node, void *u)
{
	struct check_struct *info = u;
	int *val = g_tree_lookup (info->tree, &node);

	if (val == NULL) {
		char *str = s4be_st_reverse (info->be, node);
		S4_DBG ("Found no references to %s, removing it", str);
		s4be_st_remove (info->be, str);
		free (str);
	} else {
		s4be_st_set_refcount (info->be, node, *val);
	}
}

void fix_refcount (s4be_t *be)
{
	GTree *tree;
	struct check_struct info;

	tree = g_tree_new_full (treecmp, NULL, free, free);

	info.tree = tree;
	info.be = be;

	s4be_ip_foreach (be, count_refs, tree);
	s4be_st_foreach (be, set_refs, &info);
}

int s4_recover (s4_t *s4, const char *name)
{
	s4be_t *rec;
	int ret = 1;

	rec = s4be_open (name, S4_NEW);

	if (rec == NULL) {
		return 0;
	}

	ret = s4be_recover (s4->be, rec);

	fix_refcount (rec);

	s4be_close (rec);

	return ret;
}


/**
 * Return the last error number set.
 * This function is thread safe, error numbers set in one thread
 * will NOT be seen in another thread.
 *
 * @return The last error number set, or 0 if none has been set
 */
int s4_errno()
{
	int *i = g_static_private_get (&errno);
	if (i == NULL) {
		return 0;
	}
	return *i;
}

/**
 * Set errno to the given error number
 *
 * @param err The error number to set
 */
void s4_set_errno (int err)
{
	int *i = g_static_private_get (&errno);
	if (i == NULL) {
		i = malloc (sizeof (int));
		g_static_private_set (&errno, i, NULL);
	}

	*i = err;
}

GList *s4be_ip_fetch (s4be_t *be, s4_set_t *set, int32_t fetch[], int size);
GList *s4_fetch (s4_t *s4, s4_set_t *set, const char **fetch)
{
	int32_t *ifetch;
	int i, j, size;
	GList *ret;

	s4be_st_ref (s4->be, "id");

	for (size = 0; fetch[size] != NULL; size++);

	ifetch = malloc (sizeof (int32_t) * size);

	for (i = 0; i < size; i++) {
		int32_t sn = s4be_st_lookup (s4->be, fetch[i]);
		for (j = i - 1; j >= 0 && ifetch[j] > sn; j--) {
			ifetch[j + 1] = ifetch[j];
		}
		ifetch[j + 1] = sn;
	}

	for (i = 0; i < size; i++) {
		fetch[i] = s4be_st_reverse (s4->be, ifetch[i]);
	}

	s4be_st_unref (s4->be, "id");

	ret = s4be_ip_fetch (s4->be, set, ifetch, size);

	free (ifetch);

	return ret;
}

void s4_val_free (s4_val_t *val)
{
	if (val->type == S4_VAL_STR)
		free (val->val.s);

	free (val);
}

/**
 * @}
 */
