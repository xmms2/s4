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

#ifndef __CLI_H__
#define __CLI_H__

#include <s4.h>
#include <glib.h>

typedef struct {
	int start;
	int end;
} range_t;
typedef struct {
	char *key;
	s4_val_t *val;
	char *src;
} list_data_t;
typedef struct {
	GList *list;
	int refs;
} list_t;

#define MAX_LINE_COUNT 128

void init_lexer (char *lines[], int line_count);
int yylex (void);

const char *value_to_string (const s4_val_t *val);

void print_list (list_t *list);
void print_result (const s4_resultset_t *res);
void print_cond (s4_condition_t *cond);
void print_fetch (s4_fetchspec_t *fetch);
void print_vars (void);
void print_help (void);

void set_var (const char *key, const char *val);
const char *get_var (const char *key);
void print_set_var (const char *key);

/* Defined in main.y */
extern GHashTable *cond_table, *res_table, *list_table, *fetch_table, *pref_table;
extern char **lines;
extern s4_t *s4;

#endif
