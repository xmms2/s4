/*  S4 - An XMMS2 medialib backend
 *  Copyright (C) 2009, 2010 Sivert Berg
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

#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Converts a value into a string
 * Uses a static buffer, can not be called twice without
 * possibly destroying the first value
 */
const char *value_to_string (const s4_val_t *val)
{
	static char buf[12];
	const char *ret;
	int32_t i;

	if (!s4_val_get_str (val, &ret)) {
		s4_val_get_int (val, &i);
		sprintf (buf, "%i", i);
		ret = buf;
	}

	return ret;
}

void print_list (list_t *l)
{
	list_data_t *data;
	GList *list = l->list;
	int first = 1;
	const char *sep;
	const char *print_mode = get_var ("print_mode");

	if (strcmp (print_mode, "compact") == 0) {
		sep = ", ";
	} else {
		sep = ",\n";
	}

	printf ("[");
	for (; list != NULL; list = g_list_next (list)) {
		if (first) {
			first = 0;
		} else {
			printf ("%s", sep);
		}
		data = list->data;
		printf ("%s=%s (%s)", data->key,
				value_to_string (data->val),
				data->src);
	}
	printf ("]\n");
}

/* Check if there are any non-empty columns */
static int columns_has_data (int count, const s4_result_t **cols)
{
	int i;
	for (i = 0; i < count; i++) {
		if (cols[i] != NULL)
			return 1;
	}

	return 0;
}

/* Prints a row (for the compact and pretty printer). */
static void print_row (int row, int column_width, int column_count,
		const s4_result_t **columns, const char *print_format)
{
	int i;
	const s4_result_t *res;
	char col_str[column_width];
	int compact = strcmp (print_format, "compact") == 0;

	printf ("\r|%5i ", row);

	do {
		for (i = 0; i < column_count; i++) {
			res = columns[i];
			if (res != NULL) {
				columns[i] = s4_result_next (res);
				if (compact) {
					snprintf (col_str, column_width + 1, "| %-*s", column_width,
							value_to_string (s4_result_get_val (res)));
				} else {
					snprintf (col_str, column_width + 1, "| %s (%s) %*s",
							value_to_string (s4_result_get_val (res)),
							s4_result_get_src (res),
							column_width, "");
				}
				printf ("%s", col_str);
			} else {
				printf ("| %*s", column_width - 2, "");
			}
		}
		printf ("|\n|      ");
	} while (columns_has_data (column_count, columns));
}

/* Finds the key in a resultset column. If there are more than one
 * key is returns "_"
 */
static const char *column_key (int col, const s4_resultset_t *set)
{
	int row;
	const char *ret = NULL;
	const s4_result_t *res;

	for (row = 0; row < s4_resultset_get_rowcount (set); row++) {
		res = s4_resultset_get_result (set, row, col);
		while (res != NULL) {
			const char *key = s4_result_get_key (res);
			if (ret != NULL && strcmp (ret, key)) {
				return "_";
			} else if (ret == NULL) {
				ret = key;
			}
			res = s4_result_next (res);
		}
	}

	return ret;
}

/* Uses the COLUMNS env. var to find the terminal width.
 * If it is not set it defaults to 80 chars
 */
static int terminal_width (void)
{
	const char *width = getenv ("COLUMNS");
	int ret;

	if (width == NULL || (ret = atoi (width)) == 0) {
		return 80;
	}

	return ret;
}

/* Prints a resultset. Affected by the print_format config variable */
void print_result (const s4_resultset_t *set)
{
	int col, row, total_width, col_width;
	const s4_result_t **columns, *res;
	const char *print_format = get_var ("print_mode");
	char *col_str;

	if (s4_resultset_get_rowcount (set) == 0) {
		printf ("No results\n");
		return;
	}

	columns = malloc (sizeof (s4_result_t*) * s4_resultset_get_colcount (set));
	total_width = terminal_width () - 8;
	col_width = total_width / s4_resultset_get_colcount (set);
	col_str = malloc (col_width + 1);

	if (strcmp (print_format, "pretty") == 0
			|| strcmp (print_format, "compact") == 0) {
		/* Print the header */
		printf ("| row  ");
		for (col = 0; col < s4_resultset_get_colcount (set); col++) {
			snprintf (col_str, col_width + 1, "| %-*s",
					col_width - 2, column_key (col, set));
			printf ("%s", col_str);
		}
		printf ("|\n|------|");
		for (col = 0; col < s4_resultset_get_colcount (set); col++) {
			for (row = 1; row < col_width; row++) {
				putchar ('-');
			}
			putchar ('|');
		}
		putchar ('\n');

		/* Print the rows */
		for (row = 0; row < s4_resultset_get_rowcount (set); row++) {
			for (col = 0; col < s4_resultset_get_colcount (set); col++) {
				columns[col] = s4_resultset_get_result (set, row, col);
			}
			print_row (row, col_width, s4_resultset_get_colcount (set),
					columns, print_format);
		}
	} else { /* verbose */
		/* Print the header */
		printf (" row  |  col  | data\n");

		/* Print the data */
		for (row = 0; row < s4_resultset_get_rowcount (set); row++) {
			printf ("\r%5i ", row);
			for (col = 0; col < s4_resultset_get_colcount (set); col++) {
				printf ("| %5i ", col);
				for (res = s4_resultset_get_result (set, row, col);
						res != NULL;
						res = s4_result_next (res)) {
					printf ("| %s=%s (%s)\n      |       ", s4_result_get_key (res),
							value_to_string (s4_result_get_val (res)),
							s4_result_get_src (res));
				}
				printf ("\r      ");
			}
		}
	}
	printf ("      \r"); /* Cleans up any left over | and spaces */

	free (col_str);
	free (columns);
}

void print_cond (s4_condition_t *cond)
{
	int i;
	const char *operation;
	s4_condition_t *operand;

	if (s4_cond_is_filter (cond)) {
		switch (s4_cond_get_filter_type (cond)) {
		case S4_FILTER_EQUAL: operation = "="; break;
		case S4_FILTER_NOTEQUAL: operation = "!="; break;
		case S4_FILTER_SMALLER: operation = "<"; break;
		case S4_FILTER_GREATER: operation = ">"; break;
		case S4_FILTER_SMALLEREQ: operation = "<="; break;
		case S4_FILTER_GREATEREQ: operation = ">="; break;
		case S4_FILTER_MATCH: operation = "~"; break;
		case S4_FILTER_EXISTS: operation = "+"; break;
		case S4_FILTER_TOKEN: operation = "^"; break;
		default: operation = "unknown filter"; break;
		}
		if (s4_cond_get_key (cond) != NULL) {
			printf ("%s %s", s4_cond_get_key (cond), operation);
		} else {
			printf ("%s", operation);
		}
		if (s4_cond_get_filter_type (cond) == S4_FILTER_MATCH) {
			printf (" pattern");
		} else if (s4_cond_get_filter_type (cond) == S4_FILTER_TOKEN) {
			printf (" %s", (const char *)s4_cond_get_funcdata (cond));
		} else if (s4_cond_get_filter_type (cond) != S4_FILTER_EXISTS) {
			printf (" %s", value_to_string (s4_cond_get_funcdata (cond)));
		}
	} else {
		switch (s4_cond_get_combiner_type (cond)) {
		case S4_COMBINE_AND: operation = "&"; break;
		case S4_COMBINE_NOT: operation = "!"; break;
		case S4_COMBINE_OR: operation = "|"; break;
		default: operation = "unknown combiner"; break;
		}

		if (s4_cond_get_combiner_type (cond) == S4_COMBINE_NOT) {
			printf ("!(");
		} else {
			printf ("(");
		}
		for (i = 0; (operand = s4_cond_get_operand (cond, i)) != NULL; i++) {
			if (i != 0)
				printf (") %s (", operation);
			print_cond (operand);
		}
		printf (")");
	}
}

void print_fetch (s4_fetchspec_t *fetch)
{
	int i;
	const char *sep;
	const char *print_mode = get_var ("print_mode");

	if (strcmp (print_mode, "compact") == 0) {
		sep = ", ";
	} else {
		sep = ",\n";
	}
	printf ("(");
	for (i = 0; i < s4_fetchspec_size (fetch); i++) {
		if (i != 0) {
			printf ("%s", sep);
		}
		printf ("%s", s4_fetchspec_get_key (fetch, i));
	}
	printf (")\n");
}

void print_vars ()
{
	GHashTableIter iter;
	char *str;
	void *val;

	g_hash_table_iter_init (&iter, cond_table);
	printf ("Cond table\n");
	while (g_hash_table_iter_next (&iter, (void**)&str, &val)) {
		printf ("%s: ", str);
		print_cond (val);
		printf ("\n");
	}

	g_hash_table_iter_init (&iter, fetch_table);
	printf ("Fetch table\n");
	while (g_hash_table_iter_next (&iter, (void**)&str, &val)) {
		printf ("%s: ", str);
		print_fetch (val);
	}
	g_hash_table_iter_init (&iter, res_table);
	printf ("Result table\n");
	while (g_hash_table_iter_next (&iter, (void**)&str, &val)) {
		printf ("%s: ", str);
		print_result (val);
	}
	g_hash_table_iter_init (&iter, list_table);
	printf ("List table\n");
	while (g_hash_table_iter_next (&iter, (void**)&str, &val)) {
		printf ("%s: ", str);
		print_list (val);
	}
}

void print_help (void)
{
	printf("All statements must end with a semicolon\n\n"
			"Statements with no value:\n"
			".add <list>, <list>   - For every (key, val) from the first list it adds\n"
			"                        the attributes (key, val, src) from the second list\n"
			".del <list>, <list>   - For every (key, val) from the first list it deletes\n"
			"                        the attributes (key, val, src) from the second list\n"
			".exit                 - Exit the program\n"
			".help                 - Prints this help\n"
			".set key value        - Sets the option key to val\n"
			".set key              - Shows the value of the key\n"
			".set                  - Shows the value of all keys\n"
			".vars                 - Prints all bound variables\n\n"
			"?var = <cond>         - Assigns cond to the condition variable var\n"
			"%%var = <fetch>        - Assigns fetch to the fetch variable var\n"
			"@var = <result>       - Assigns var to something returning result\n"
			"$var = <list>         - Assigns the list to the list variable var\n"
			"#var = <pref>         - Assigns the pref to the souce preference variable var\n\n"
			"Conditions (<cond>):\n"
			"?var                  - Returns the condition bound to var\n"
			"!cond                 - Matches everything cond does not match\n"
			"cond1 & cond2         - Matches if both cond1 and cond2 matches\n"
			"cond1 | cond2         - Matches if cond1 or cond2 matches\n\n"
			"Filter conditions\n"
			"key = value           - Matches all entries where key equals value\n"
			"key ~ value           - Matches all entries where key matches value\n"
			"key < value           - Matches all entries where key is smaller than value\n"
			"key > value           - Matches all entries where key is greater than value\n"
			"key ^ token           - Matches all entries where key has a token equal to token\n"
			"key != value          - Matches all entries where key does not equal value\n"
			"key <= value          - Matches all entries where key is smaller or equal to value\n"
			"key >= value          - Matches all entries where key is greater or equal to value\n"
			"= value               - Matches all entries where one or more keys equals value\n"
			"~ value               - Matches all entries where one or more keys matches value\n"
			"< value               - Matches all entries where one or more keys is smaller than value\n"
			"> value               - Matches all entries where one or more keys is greater than value\n"
			"^ token               - Matches all entries where one or more keys has token\n"
			"!= value              - Matches all entries where one or more keys does not equal value\n"
			"<= value              - Matches all entries where one or more keys is smaller or equal to value\n"
			">= value              - Matches all entries where one or more keys is greater or equal to value\n"
			"+key                  - Matches all entries that has key\n"
			"+                     - Matches everything\n"
			"<pref> may be added after all filter conditions to use a source preference to only match\n"
			"against the highest priority source in the source preference\n\n"
			"Fetch specification (<fetch>):\n"
			"%%var                  - Returns the fetch spec bound to var\n"
			"(key1, ..., keyn)     - Fetches keys 1 through n from matching entries\n"
			"(key1 <pref>,...)     - Fetches key1 using the source preference given\n"
			"key                   - Fetches key from matching entries\n"
			"key <pref>            - Fetches key using the source preference given\n"
			"_                     - Fetches everything from matching entries\n\n"
			"Results (<result>):\n"
			".query <fetch> <cond> - Queries the database, returns a result\n\n"
			"@var                  - Returns the result bound to var\n\n"
			"Lists (<list>):\n"
			"$var                  - Returns the list bound to the variable var\n"
			"<result>{<rng>,<rng>} - Creates a list of the columns given by {row,col}.\n"
			"[key val src, ...]    - Creates a list\n"
			"[key val, ...]        - Creates a list where source is set to default_source\n\n"
			"Ranges (<rng>):\n"
			"start - stop          - Creates a range from start to stop (inclusive)\n"
			"      - stop          - Creates a range from 0 to stop\n"
			"start -               - Creates a range from start with no stop\n"
			"      -               - Creates a range from 0 with no stop\n\n"
			"Source preferences (<pref>):\n"
			"#var                  - Returns the source preference bound to var\n"
			":src1:src2:...:srcn   - Creates a new source preference where src1 has the highest priority,\n"
			"                        src2 seconds highest and so on\n\n"
			"Shorthand:\n"
			".q = .query\n"
			".a = .add\n"
			".d = .del\n"
			".v = .vars\n"
			".h = .help\n"
			".? = .help\n"
			".s = .set\n"
			".e = .exit\n"
			);
}
