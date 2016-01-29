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

%{
#include "cli.h"
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <s4.h>
#include <glib.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

static void yyerror (const char *);
static void ref_list (list_t *list);
static void unref_list (list_t *list);
static list_t *create_list (GList *data);
static GList *result_to_list (GList *prev, const s4_result_t *res);
static GList *set_to_list (const s4_resultset_t *set,
	int row_start, int row_end,
	int col_start, int col_end);
static void add_or_del (int (*func)(s4_transaction_t *, const char *,
									const s4_val_t*, const char *,
									const s4_val_t*, const char *),
						list_t *list_a, list_t *list_b);
static void cleanup (void);

GHashTable *cond_table, *res_table, *list_table, *fetch_table, *pref_table;
char **lines;
s4_t *s4;

#define SET_YYLLOC(l) (yylloc = (l))

%}

%union {
	char *string;
	int32_t number;
	s4_condition_t *condition;
	s4_val_t *value;
	s4_filter_type_t filter_type;
	s4_resultset_t *result;
	s4_fetchspec_t *fetch;
	s4_sourcepref_t *sourcepref;
	list_t *list;
	list_data_t *list_data;
	GList *list_datas, *sourcepref_list;
	range_t range;
}

%token <string> STRING QUOTED_STRING COND_VAR LIST_VAR RESULT_VAR FETCH_VAR PREF_VAR
%token <number> INT
%token INFO QUERY ADD DEL VARS SET HELP EXIT GR_EQ LE_EQ NOT_EQ

%type <value> value
%type <condition> cond
%type <string> string fetch_key
%type <filter_type> filter_type
%type <result> result
%type <fetch> fetch fetch_list fetch_item
%type <list> list
%type <list_datas> list_datas
%type <list_data> list_data
%type <sourcepref_list> pref_data
%type <sourcepref> pref pref_or_not
%type <range> range

%destructor { free ($$); } <string>
%destructor { s4_val_free ($$); } <value>
%destructor { s4_cond_unref ($$); } <condition>
%destructor { s4_resultset_unref ($$); } <result>
%destructor { s4_fetchspec_unref ($$); } <fetch>
%destructor { free ($$); } <list_data>
%destructor { unref_list (create_list ($$)); } <list_datas>
%destructor { unref_list ($$); } <list>
%destructor { s4_sourcepref_unref ($$); } <sourcepref>
%destructor { for (; $$ != NULL; $$ = g_list_delete_link ($$, $$)) free ($$->data); } <sourcepref_list>

%error-verbose
%debug

%left '|'
%left '&'
%left '!'

%%
input: command ';'
	 | input command ';'
	 ;

command: /* Empty */
	   | cond { print_cond ($1); printf ("\n"); s4_cond_unref ($1); }
	   | list { print_list ($1); unref_list ($1); }
	   | result { print_result ($1); s4_resultset_unref ($1); }
	   | fetch_list { print_fetch ($1); s4_fetchspec_unref ($1); }
	   | add
	   | del
	   | set
	   | HELP { print_help (); }
	   | VARS { print_vars (); }
	   | EXIT { cleanup (); exit (0); }
	   | COND_VAR '=' cond { g_hash_table_insert (cond_table, $1, $3); }
	   | LIST_VAR '=' list { g_hash_table_insert (list_table, $1, $3); }
	   | FETCH_VAR '=' fetch_list { g_hash_table_insert (fetch_table, $1, $3); }
	   | PREF_VAR '=' pref { g_hash_table_insert (pref_table, $1, $3); }
	   | RESULT_VAR '=' result { g_hash_table_insert (res_table, $1, $3); }
	   ;

set: SET string string { set_var ($2, $3); free ($2); }
   | SET string { print_set_var ($2); free ($2); }
   | SET { print_set_var (NULL); }

add: ADD list ',' list { add_or_del (s4_add, $2, $4); unref_list ($2); unref_list ($4); }
   ;

del: DEL list ',' list { add_or_del (s4_del, $2, $4); unref_list ($2); unref_list ($4); }
   ;

range: INT '-' INT { $$.start = $1; $$.end = $3; }
	 | INT '-'     { $$.start = $1; $$.end = INT_MAX; }
	 |     '-' INT { $$.start =  0; $$.end = $2; }
	 |     '-'     { $$.start =  0; $$.end = INT_MAX; }
	 | INT         { $$.start = $1; $$.end = $1; }
	 ;

pref_data: string { $$ = g_list_prepend (NULL, $1); }
		 | pref_data ':' string { $$ = g_list_prepend ($1, $3); }

pref: PREF_VAR
	{
		$$ = g_hash_table_lookup (pref_table, $1);
		free ($1);
		if ($$ == NULL) {
			SET_YYLLOC (@1);
			yyerror ("Undefined source preference variable");
			YYERROR;
		}
		s4_sourcepref_ref ($$);
	}
	| ':' pref_data
	{
		int i, len = g_list_length ($2);
		char **sources = malloc (sizeof (char*) * (len + 1));

		$2 = g_list_reverse ($2);

		for (i = 0; $2 != NULL; $2 = g_list_delete_link ($2, $2), i++) {
			sources[i] = $2->data;
		}
		sources[len] = NULL;

		$$ = s4_sourcepref_create ((const char **)sources);
		for (i = 0; i < len; i++) {
			free (sources[i]);
		}
		free (sources);
	}

list_data: string value string
		 {
			 $$ = malloc (sizeof (list_data_t));
			 $$->key = $1;
			 $$->val = $2;
			 $$->src = $3;
		 }
		 | string value
		 {
			 $$ = malloc (sizeof (list_data_t));
			 $$->key = $1;
			 $$->val = $2;
			 $$->src = strdup (get_var ("default_source"));
		 }
		 ;

list_datas: list_data { $$ = g_list_prepend (NULL, $1); }
		  | list_datas ',' list_data { $$ = g_list_prepend ($1, $3); }
		  ;

list: LIST_VAR
	{
		$$ = g_hash_table_lookup (list_table, $1);
		free ($1);
		if ($$ == NULL) {
			SET_YYLLOC (@1);
			yyerror ("Undefined list variable");
			YYERROR;
		}
		ref_list ($$);
	}
	| '[' list_datas ']' { $$ = create_list ($2); }
	| list_data { $$ = create_list (g_list_prepend (NULL, $1)); }
	| result '{' range ',' range '}'
	{
		$$ = create_list (set_to_list ($1, $3.start, $3.end, $5.start, $5.end));
		s4_resultset_unref ($1);
	}
	| result '{' range ',' string '}'
	{
		int col = find_column ($5, $1);
		if (col != -1) {
			$$ = create_list (set_to_list ($1, $3.start, $3.end, col, col));
		} else {
			SET_YYLLOC (@5);
			yyerror ("No column with that key");
			YYERROR;
		}
		s4_resultset_unref ($1);
	}
	;

fetch_key: string
		 | '_' { $$ = NULL; }

fetch_item: fetch_key pref_or_not
		  {
			  $$ = s4_fetchspec_create ();
			  s4_fetchspec_add ($$, $1, $2, S4_FETCH_PARENT | S4_FETCH_DATA);
			  if ($2 != NULL)
				  s4_sourcepref_unref ($2);
			  if ($1 != NULL)
				  free ($1);
		  }
		  ;

fetch: fetch_item
	 | fetch ',' fetch_key pref_or_not
	 {
		 $$ = $1;
		 s4_fetchspec_add ($$, $3, $4, S4_FETCH_PARENT | S4_FETCH_DATA);
		 if ($4 != NULL)
			 s4_sourcepref_unref ($4);
		 if ($3 != NULL)
			 free ($3);
	 }
	 ;

fetch_list: '(' fetch ')' { $$ = $2; }
		  | FETCH_VAR
		  {
			  $$ = s4_fetchspec_ref (g_hash_table_lookup (fetch_table, $1));
			  free ($1);
			  if ($$ == NULL) {
				  SET_YYLLOC (@1);
				  yyerror ("Undefined fetch variable");
				  YYERROR;
			  }
		  }
		  | fetch_item
		  ;

result: RESULT_VAR
  	  {
		  $$ = s4_resultset_ref (g_hash_table_lookup (res_table, $1));
		  free ($1);
		  if ($$ == NULL) {
			  SET_YYLLOC (@1);
			  yyerror ("Undefined result variable");
			  YYERROR;
		  }
	  }
	  | QUERY fetch_list cond
	  {
		  s4_order_t *order;
		  s4_order_entry_t *entry;
		  s4_transaction_t *trans;

		  order = s4_order_create ();
		  entry = s4_order_add_column (order,
		                               S4_CMP_COLLATE,
		                               S4_ORDER_ASCENDING);
		  s4_order_entry_add_choice (entry, 0);

		  trans = s4_begin (s4, 0);
		  $$ = s4_query (trans, $2,  $3);
		  s4_commit (trans);
		  s4_resultset_sort ($$, order);
		  s4_cond_unref ($3);
		  s4_fetchspec_unref ($2);
	  }
	  ;

string: STRING
	  | QUOTED_STRING

value: INT { $$ = s4_val_new_int ($1); }
	 | string { $$ = s4_val_new_string ($1); free ($1); }
	 ;

filter_type: '=' { $$ =  S4_FILTER_EQUAL; }
		   | '~' { $$ =  S4_FILTER_MATCH; }
		   | '<' { $$ =  S4_FILTER_SMALLER; }
		   | '>' { $$ =  S4_FILTER_GREATER; }
		   | '^' { $$ =  S4_FILTER_TOKEN; }
		   | LE_EQ { $$ =  S4_FILTER_SMALLEREQ; }
		   | GR_EQ { $$ =  S4_FILTER_GREATEREQ; }
		   | NOT_EQ { $$ =  S4_FILTER_NOTEQUAL; }
		   ;

pref_or_not: pref
		   | /* Nothing */ { $$ = NULL; }
		   ;

cond: COND_VAR
	{
		$$ = s4_cond_ref (g_hash_table_lookup (cond_table, $1));
		free ($1);
		if ($$ == NULL) {
			SET_YYLLOC (@1);
			yyerror ("Undefined condition variable");
			YYERROR;
		}
	}
	| '(' cond ')'
	{
		$$ = $2;
	}
	| cond '&' cond
	{
		$$ = s4_cond_new_combiner (S4_COMBINE_AND);
		s4_cond_add_operand ($$, $1);
		s4_cond_add_operand ($$, $3);
		s4_cond_unref ($1);
		s4_cond_unref ($3);
	}
	| cond '|' cond
	{
		$$ = s4_cond_new_combiner (S4_COMBINE_OR);
		s4_cond_add_operand ($$, $1);
		s4_cond_add_operand ($$, $3);
		s4_cond_unref ($1);
		s4_cond_unref ($3);
	}
	| '!' cond
	{
		$$ = s4_cond_new_combiner (S4_COMBINE_NOT);
		s4_cond_add_operand ($$, $2);
		s4_cond_unref ($2);
	}
	| string filter_type value pref_or_not
	{
		$$ = s4_cond_new_filter ($2, $1, $3, $4, S4_CMP_CASELESS, 0);
		s4_val_free ($3);
		if ($4 != NULL)
			s4_sourcepref_unref ($4);
	}
	| filter_type value pref_or_not
	{
		$$ = s4_cond_new_filter ($1, NULL, $2, $3, S4_CMP_CASELESS, 0);
		s4_val_free ($2);
		if ($3 != NULL)
			s4_sourcepref_unref ($3);
	}
	| '+' string pref_or_not
	{
		$$ = s4_cond_new_filter (S4_FILTER_EXISTS, $2, NULL, $3, S4_CMP_BINARY, 0);
		if ($3 != NULL)
			s4_sourcepref_unref ($3);
	}
	| '+' pref_or_not
	{
		$$ = s4_cond_new_filter (S4_FILTER_EXISTS, NULL, NULL, $2, S4_CMP_BINARY, 0);
		if ($2 != NULL)
			s4_sourcepref_unref ($2);
	}
	;

%%

void yyerror (const char *str)
{
	int i, line, col = yylloc.first_column;

	fprintf (stderr, "%s\n", str);

	for (line = yylloc.first_line; line <= yylloc.last_line; line++) {
		fprintf (stderr, "%s\n", lines[line]);
		for (i = 0; i < col; i++)
			fputc (' ', stderr);
		for (; (line < yylloc.last_line && col < strlen (lines[line])) || col < yylloc.last_column; col++)
			fputc ('^', stderr);

		col = 0;
		fputc ('\n', stderr);
	}
}

void ref_list (list_t *list)
{
	list->refs++;
}

void unref_list (list_t *list)
{
	list->refs--;
	if (list->refs <= 0) {
		GList *l = list->list;
		for (; l != NULL; l = g_list_next (l)) {
			list_data_t *data = l->data;
			free (data->key);
			if (data->src != NULL)
				free (data->src);
			s4_val_free (data->val);
			free (l->data);
		}

		g_list_free (list->list);
		free (list);
	}
}

list_t *create_list (GList *data)
{
	list_t *ret = malloc (sizeof (list_t));
	ret->refs = 1;
	ret->list = data;

	return ret;
}

GList *result_to_list (GList *prev, const s4_result_t *res)
{
	GList *ret = prev;
	list_data_t *data;

	for (; res != NULL; res = s4_result_next (res)) {
		data = malloc (sizeof (list_data_t));
		data->key = strdup (s4_result_get_key (res));
		data->val = s4_val_copy (s4_result_get_val (res));
		data->src = (char*)s4_result_get_src (res);
		if (data->src != NULL)
			data->src = strdup (data->src);
		ret = g_list_prepend (ret, data);
	}

	return ret;
}

GList *set_to_list (const s4_resultset_t *set,
	int row_start, int row_end,
	int col_start, int col_end)
{
	int row, col;
	GList *ret = NULL;

	for (row = row_start; row <= row_end && row < s4_resultset_get_rowcount (set); row++) {
		for (col = col_start; col <= col_end &&  col < s4_resultset_get_colcount (set); col++) {
			ret = result_to_list (ret, s4_resultset_get_result (set, row, col));
		}
	}

	return g_list_reverse (ret);
}

static void add_or_del (int (*func)(s4_transaction_t *t, const char *,
									const s4_val_t*, const char *,
									const s4_val_t*, const char *),
						list_t *list_a, list_t *list_b)
{
	list_data_t *da, *db;
	GList *a, *b;
	s4_transaction_t *trans = s4_begin (s4, 0);

	for (a = list_a->list; a != NULL; a = g_list_next (a)) {
		da = a->data;
		for (b = list_b->list; b != NULL; b = g_list_next (b)) {
			db = b->data;
			if (!func (trans, da->key, da->val, db->key, db->val, db->src)) {
				printf ("failed on %s %s, %s %s %s",
					da->key, value_to_string (da->val),
					db->key, value_to_string (db->val),
					db->src);
			}
		}
	}

	s4_commit (trans);
}

static int no_semicolon (const char *line)
{
	return line[strlen (line) - 1] != ';';
}

static void strip (char *line)
{
	int last = 0;
	int i;

	if (line == NULL)
		return;

	for (i = 0; line[i]; i++) {
		if (!isspace (line[i]))
			last = i;
	}

	line[last + 1] = '\0';
}

static char *rl_get_line (int first)
{
	char *ret = NULL;

	do {
		if (ret != NULL)
			free (ret);
		ret = readline (first?"s4> ":"..> ");
		strip (ret);
	} while (ret != NULL && !strlen (ret));

	add_history (ret);

	return ret;
}

static char **rl_get_lines (int *line_co)
{
	static char *lines[MAX_LINE_COUNT];
	static int line_count = 0;
	int i;

	/* Free old lines */
	while (--line_count >= 0) {
		free (lines[line_count]);
	}

	for (i = 0; (lines[i] = rl_get_line (i == 0)) != NULL && no_semicolon (lines[i]); i++);
	if (lines[i] == NULL) {
		return NULL;
	}
	line_count = *line_co = i + 1;

	return lines;
}

void cleanup ()
{
	s4_close (s4);

	g_hash_table_destroy (cond_table);
	g_hash_table_destroy (fetch_table);
	g_hash_table_destroy (list_table);
	g_hash_table_destroy (res_table);
	g_hash_table_destroy (pref_table);
}

int main(int argc, const char *argv[])
{
	int line_count;

	if (argc < 2) {
		printf("Not enough arguments\nUsage: %s <s4-file>\n", argv[0]);
		return 1;
	}

	printf ("S4 CLI tool\nEnter \".help;\" for instructions\n");

	cond_table = g_hash_table_new_full (g_str_hash, g_str_equal,
		free, (GDestroyNotify)s4_cond_unref);
	list_table = g_hash_table_new_full (g_str_hash, g_str_equal,
		free, (GDestroyNotify)unref_list);
	res_table = g_hash_table_new_full (g_str_hash, g_str_equal,
		free, (GDestroyNotify)s4_resultset_free);
	fetch_table = g_hash_table_new_full (g_str_hash, g_str_equal,
		free, (GDestroyNotify)s4_fetchspec_unref);
	pref_table = g_hash_table_new_full (g_str_hash, g_str_equal,
		free, (GDestroyNotify)s4_sourcepref_unref);

	config_init ();

	s4 = s4_open (argv[1], NULL, S4_EXISTS);

	if (s4 == NULL) {
		printf ("Could not open %s - ", argv[1]);
		switch (s4_errno ()) {
		case S4E_MAGIC:
			printf ("Wrong magic number\n");
			break;
		case S4E_VERSION:
			printf ("Wrong version number\n");
			break;
		case S4E_NOENT:
			printf ("File does not exist\n");
			break;
		case S4E_INCONS:
			printf ("File is inconsistent (corrupted?)\n");
			break;
		case S4E_LOGOPEN:
			printf ("Could not open logfile: %s\n", strerror (errno));
			break;
		case S4E_LOGREDO:
			printf ("Could not redo log\n");
			break;
		case S4E_OPEN:
			printf ("%s\n", strerror (errno));
			break;
		case S4E_EXISTS:
			printf ("File exists?? If you see this, file a bug report!\n");
			break;
		default:
			break;
		}
		return 1;
	}

	while ((lines = rl_get_lines (&line_count)) != NULL) {
		init_lexer (lines, line_count);
		yyparse ();
	}

	config_cleanup ();
	cleanup ();

	printf (".exit;\n");

	return 0;
}
