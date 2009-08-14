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
#include <stdio.h>
#include <stdlib.h>
#include "log.h"

const char *name;

void usage()
{
	printf ("usage: %s [options] db_file\n", name);
	printf ("options:\n");
	printf ("\t[+/-]t - enable/disable thorough check (enabled by default)\n");
	printf ("\t[+/-]r - enable/disable refcount check (disabled by deafult)\n");
}

int get_flags (const char *str)
{
	int ret = 0;

	for (; *str; str++) {
		switch (*str) {
			case 't':
				ret |= S4_VERIFY_THOROUGH;
				break;
			case 'r':
				ret |= S4_VERIFY_REFCOUNT;
				break;
			default:
				printf ("%c - no such flag\n", *str);
				usage();
				exit (0);
		}
	}

	return ret;
}

int main(int argc, char *argv[])
{
	s4_t *s4;
	const char *filename = NULL;
	int flags = S4_VERIFY_THOROUGH;
	int i;

	name = argv[0];

	if (argc < 2) {
		usage ();
		exit (0);
	}

	for (i = 1; i < argc; i++) {
		switch (*argv[i]) {
			case '+':
				flags |= get_flags (argv[i] + 1);
				break;
			case '-':
				flags &= ~get_flags (argv[i] + 1);
				break;
			default:
				if (filename != NULL) {
					printf ("You can only specifiy one file\n");
					usage();
					exit(0);
				}
				filename = argv[i];
				break;
		}
	}

	if (filename == NULL) {
		printf ("You must give a database name\n");
		usage();
		exit (0);
	}

	log_init(G_LOG_LEVEL_MASK);

	s4 = s4_open (filename, S4_EXISTS);

	if (s4 == NULL) {
		exit (0);
	}

	if (!s4_verify (s4, flags)) {
		printf ("The database is corrupted, you should try to recover it\n");
	}
	s4_close (s4);

	return 0;
}
