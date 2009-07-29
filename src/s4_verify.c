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
		switch (*(argv[i])) {
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

	log_init();

	s4 = s4_open (argv[1], 0);

	if (s4 == NULL) {
		exit (0);
	}

	if (!s4_verify (s4, flags)) {
		printf ("The database is corrupted, you should try to recover it\n");
	}
	s4_close (s4);

	return 0;
}
