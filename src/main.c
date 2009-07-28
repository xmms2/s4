#include <xmms/s4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "homegrown/bpt.h"

const char *info[] = {
	"artist",
	"title",
	"album",
	"duration",
	"tracknr",
	NULL
};

int in_info (const char *str)
{
	int i;
	for (i = 0; info[i] != NULL; i++)
		if (strcmp (str, info[i]) == 0)
			return 1;

	return 0;
}

static int is_int (const char *str, int *val)
{
	int ret = 0;
	char *end;

	if (!isspace (*str)) {
		*val = strtol (str, &end, 10);
		if (*end == '\0')
			ret = 1;
	}

	return ret;
}

int main (int argc, char *argv[])
{
	s4_t *s4;
	char buffer[2048];
	char *key, *val;
	int id, ival;
	s4_set_t *set, *props;
	s4_entry_t entry, prop;

	s4 = s4_open ("medialib");

	if (s4 == NULL) {
		printf("Could not open database\n");
		exit(0);
	}

	int key_a, val_a, key_b, val_b, src;
	char c;

	FILE *in = stdin;
	if (argc > 1)
		in = fopen (argv[1], "r");

	while (fgets (buffer, 2048, in) != NULL) {
		sscanf (buffer, "%c %i %i %i %i %i", &c, &key_a, &val_a, &key_b, &val_b, &src);
//		printf ("%c %i %i %i %i %i\n", c, key_a, val_a, key_b, val_b, src);

		bpt_record_t rec;

		rec.key_a = key_a;
		rec.key_b = key_b;
		rec.val_a = val_a;
		rec.val_b = val_b;
		rec.src = src;

		switch (c) {
			case 'i':
				bpt_insert (s4->be, 20, rec);
				break;
			case 'r':
				bpt_remove (s4->be, 20, rec);
				break;
			default:
				printf ("unknown char in %s\n", buffer);
		}
	}

	bpt_print_tree (s4->be, 20);

	/*
	if (argc > 1) {
		FILE *file = fopen (argv[1], "r");

		while (fgets (buffer, 2048, file) != NULL) {
			buffer[strlen (buffer) - 1] = 0;
			key = strtok (buffer, "|");
			if (key != NULL)
				id = atoi (key);

			key = strtok (NULL, "|");
			val = strtok (NULL, "|");

			if (key != NULL && val != NULL) {
				entry = s4_entry_get_i (s4, "song_id", id);
				if (is_int (val, &ival))
					prop = s4_entry_get_i (s4, key, ival);
				else
					prop = s4_entry_get_s (s4, key, val);

				if (s4_entry_add (s4, entry, prop))
					printf ("Error inserting %i (%s %s)\n",
							id, key, val);

				s4_entry_free (entry);
				s4_entry_free (prop);
			}
		}

		fclose (file);
	} else {
		while (fgets (buffer, 2048, stdin) != NULL) {
			buffer[strlen (buffer) - 1] = 0;

			set = s4_query (s4, buffer);
			while (set != NULL) {
				s4_entry_fillin (s4, &set->entry);
				printf ("Found (%s, %i)\n", set->entry.key_s, set->entry.val_i);

				props = s4_entry_contains (s4, &set->entry);

				while (props != NULL) {
					s4_entry_fillin (s4, &props->entry);
					if (in_info (props->entry.key_s)) {
						if (props->entry.val_s != NULL)
							printf ("  %s: %s\n", props->entry.key_s, props->entry.val_s);
						else
							printf ("i %s: %i\n", props->entry.key_s, props->entry.val_i);
					}
					props = s4_set_next (props);
				}

				set = s4_set_next (set);
			}
		}
	}
	*/

	s4_close (s4);

	return 0;
}
