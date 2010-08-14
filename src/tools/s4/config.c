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
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
	const char *key;
	const char *value;
	const char *possible_values[8];
} config_var;

config_var user_vars[] = {
	{.key = "default_source", .value = "s4", .possible_values = {NULL}},
	{.key = "print_mode", .value = "verbose", .possible_values = {"verbose", "pretty", "compact", NULL}},
	{.key = NULL}
};

/* Sets the configuration variable key to val.
 */
void set_var (const char *key, const char *val)
{
	int i, j;

	for (i = 0; user_vars[i].key != NULL; i++) {
		if (strcmp (key, user_vars[i].key) == 0) {
			if (user_vars[i].possible_values[0] != NULL) {
				for (j = 0; user_vars[i].possible_values[j] != NULL; j++) {
					if (strcmp (user_vars[i].possible_values[j], val) == 0) {
						break;
					}
				}
				if (user_vars[i].possible_values[j] == NULL) {
					fprintf (stderr, "%s is not a valid value for %s\n", val, key);
					return;
				}
			}

			user_vars[i].value = val;
			return;
		}
	}

	fprintf (stderr, "%s is not a configuration variable\n", key);
}

/* Returns the value of key
 * If key is not found, NULL is returned
 */
const char *get_var (const char *key)
{
	int i = 0;

	for (i = 0; user_vars[i].key != NULL; i++) {
		if (strcmp (key, user_vars[i].key) == 0) {
			return user_vars[i].value;
		}
	}

	return NULL;
}

/* Prints the config var key if it exists.
 * If NULL is passed it prints everything
 */
void print_set_var (const char *key)
{
	int i, j;

	for (i = 0; user_vars[i].key != NULL; i++) {
		if (key == NULL || strcmp (key, user_vars[i].key) == 0) {
			printf ("%s = %s\n", user_vars[i].key, user_vars[i].value);

			if (user_vars[i].possible_values[0] != NULL) {
				printf ("Possible values:\n");
				for (j = 0; user_vars[i].possible_values[j] != NULL; j++) {
					printf ("- %s\n", user_vars[i].possible_values[j]);
				}
			}
			printf ("\n");
		}
	}
}
