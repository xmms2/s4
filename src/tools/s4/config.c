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
#include <errno.h>
#include <ctype.h>

#define CONFIG_FILENAME "s4-cli.conf"

typedef struct {
	const char *key;
	const char *default_value;
	char *value;
	const char *possible_values[8];
} config_var;

config_var user_vars[] = {
	{.key = "default_source", .default_value = "s4", .possible_values = {NULL}},
	{.key = "print_mode", .default_value = "verbose", .possible_values = {"verbose", "pretty", "compact", NULL}},
	{.key = NULL}
};

char *config_file;

static char *get_config_file (const char *filename)
{
	const char *try[] = {"XDG_CONFIG_HOME",
		"APPDATA",
		"HOME",
		NULL};
	char *conf_file, *conf_dir;
	int i = 0;

	do {
		conf_dir = getenv (try[i++]);
	} while (conf_dir == NULL && try[i] != NULL);

	if (conf_dir == NULL) {
		conf_file = g_strdup (filename);
	} else {
		conf_dir = g_build_filename (conf_dir, "s4", NULL);
		if (g_mkdir_with_parents (conf_dir, 0700) == -1) {
			conf_file = g_strdup (filename);
			fprintf (stderr, "Could not create %s - %s\n", conf_dir, strerror (errno));
		} else {
			conf_file = g_build_filename (conf_dir, filename, NULL);
		}
		g_free (conf_dir);
	}

	return conf_file;
}

#define HEX_TO_CHAR(h) ((h)<10?(h) + '0':(h) + 'a' - 9)
#define CHAR_TO_HEX(c) (((c) & 0xf) + (((c) & 0x40) >> 6) * 9)

static void encode_string (const char *val, char *buf)
{
	for (; *val; val++) {
		if (isalnum (*val)) {
			*buf++ = *val;
		} else {
			*buf++ = '%';
			*buf++ = HEX_TO_CHAR ((*val >> 4) & 0xf);
			*buf++ = HEX_TO_CHAR (*val & 0xf);
		}
	}
}

static void decode_string (char *buf)
{
	char *w, *r;
	for (w = r = buf; *r; w++, r++) {
		if (*r == '%') {
			*w = CHAR_TO_HEX (*(r+1)) << 4 | CHAR_TO_HEX (*(r+2));
			r += 2;
		} else {
			*w = *r;
		}
	}
	*w = '\0';
}

void config_init (void)
{
	FILE *file;
	char key[1024], value[4096];
	int i;

	for (i = 0; user_vars[i].key != NULL; i++) {
		user_vars[i].value = strdup (user_vars[i].default_value);
	}

	config_file = get_config_file (CONFIG_FILENAME);
	file = fopen (config_file, "r");
	if (file == NULL) {
		fprintf (stderr, "Could not open %s - %s\n", config_file, strerror (errno));
		return;
	}

	while (fscanf (file, " %s = %s ", key, value) != EOF) {
		decode_string (value);
		set_var (key, strdup (value));
	}

	fclose (file);
}

void config_cleanup (void)
{
	FILE *file;
	char value[4096];
	int i;

	file = fopen (config_file, "w");
	if (file == NULL) {
		fprintf (stderr, "Could not open %s - %s\n", config_file, strerror (errno));
		return;
	}

	for (i = 0; user_vars[i].key != NULL; i++) {
		encode_string (user_vars[i].value, value);
		fprintf (file, "%s = %s\n", user_vars[i].key, value);

	}

	fclose (file);

	g_free (config_file);
}

/* Sets the configuration variable key to val.
 */
void set_var (const char *key, char *val)
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
					free (val);
					return;
				}
			}

			free (user_vars[i].value);
			user_vars[i].value = val;
			return;
		}
	}

	fprintf (stderr, "%s is not a configuration variable\n", key);
	free (val);
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
