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

#include "s4.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include <glib.h>

static int tree_cmp (gconstpointer a, gconstpointer b)
{
	int pa, pb;
	pa = *(int*)a;
	pb = *(int*)b;

	if (pa < pb)
		return -1;
	if (pa > pb)
		return 1;
	return 0;
}

static int source_callback (void *u, int argc, char *argv[], char *col[])
{
	GTree *sources = u;
	int i;
	char *src;
	int *id = malloc (sizeof (int));

	for (i = 0; i < argc; i++) {
		if (!strcmp ("source", col[i]))
			src = strdup (argv[i]);
		else if (!strcmp ("id", col[i]))
			*id = atoi (argv[i]);
	}

	g_tree_insert (sources, id, src);

	return 0;
}

s4_t *s4;
static int media_callback (void *u, int argc, char *argv[], char *col[])
{
	GTree *sources = u;
	int id, src_id, i;
	char *key, *val, *src;
	s4_entry_t *entry, *prop;

	for (i = 0; i < argc; i++) {
		if (!strcmp ("id", col[i])) {
			id = atoi (argv[i]);
		} else if (!strcmp ("key", col[i])) {
			key = argv[i];
		} else if (!strcmp ("value", col[i])) {
			val = argv[i];
		} else if (!strcmp ("source", col[i])) {
			src_id = atoi (argv[i]);
		}
	}

	src = g_tree_lookup (sources, &src_id);
	entry = s4_entry_get_i (s4, "song_id", id);
	prop = s4_entry_get_s (s4, key, val);

	s4_entry_add (s4, entry, prop, src);

	return 0;
}

int main (int argc, char *argv[])
{
	sqlite3 *db;
	char *errmsg = NULL;
	int ret;
	GTree *sources = g_tree_new (tree_cmp);

	if (argc != 3) {
		fprintf (stderr, "Usage: %s infile outfile\n"
				"\tinfile  - the sql file to import\n"
				"\toutfile - the s4 file to write to\n",
				argv[0]);
		exit (1);
	}

	ret = sqlite3_open (argv[1], &db);
	if (ret) {
		fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close (db);
		exit (1);
	}

	s4 = s4_open (argv[2], S4_NEW);
	if (s4 == NULL) {
		fprintf (stderr, "Can't open s4 file\n");
		exit (1);
	}

	ret = sqlite3_exec (db, "select id,source from Sources;",
			source_callback, sources, &errmsg);

	ret = sqlite3_exec (db, "select id,key,value,source from Media;",
			media_callback, sources, &errmsg);


	sqlite3_close (db);
	s4_close (s4);

	return 0;
}
