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
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>


void usage (const char *name)
{
	fprintf (stderr, "Usage: %s [options] infile\n", name);
	fprintf (stderr,
			"\tinfile     -- the file to recover\n\n"
			"\tOptions:\n"
			"\t-o outfile -- output to outfile instead of overwriting infile\n"
			"\t-v         -- verify the file and only recover if it is corrupted\n"
			"\t-V         -- thorough verification\n"
			"\t-W         -- very thorough verification\n");

}

int main (int argc, char *argv[])
{
	const char *infile = NULL;
	const char *outfile = NULL;
	int ver_flags = -1;
	int i, j;
	int out = 0;
	s4_t *s4;

	for (i = 1; i < argc; i++) {
		if (out) {
			outfile = argv[i];
			out = 0;
			continue;
		}
		switch (*argv[i]) {
		case '-':
			for (j = 1; argv[i][j]; j++) {
				switch (argv[i][j]) {
				case 'o':
					out = 1;
					break;
				case 'v':
					ver_flags = 0;
					break;
				case 'V':
					ver_flags = S4_VERIFY_THOROUGH;
					break;
				case 'W':
					ver_flags = S4_VERIFY_THOROUGH | S4_VERIFY_REFCOUNT;
					break;
				default:
					fprintf (stderr, "Unknown option %c\n", argv[i][j]);
					usage (argv[0]);
					exit (1);
				}
			}
			break;

		default:
			infile = argv[i];
			break;
		}
	}

	if (infile == NULL) {
		fprintf (stderr, "You must specifiy an infile\n");
		usage (argv[0]);
		exit (1);
	}
	if (out) {
		fprintf (stderr, "-o not followed by a filename\n");
		usage (argv[0]);
		exit (1);
	}

	s4 = s4_open (infile, S4_EXISTS);

	if (s4 == NULL) {
		fprintf (stderr, "Could not open %s\n", infile);
		exit (1);
	}

	if (ver_flags != -1 && s4_verify (s4, ver_flags)) {
		printf ("Verification went okay, the file does not need recovery\n");
		exit (0);
	}

	if (outfile == NULL) {
		outfile = tmpnam (NULL);
		s4_recover (s4, outfile);
		s4_close (s4);
		g_unlink (infile);
		g_rename (outfile, infile);
	} else {
		s4_recover (s4, outfile);
	}

	return 0;
}
