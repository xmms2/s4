%{
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <s4.h>
#include <glib.h>
#include <errno.h>
#include <string.h>

extern int yylex (void);
extern void init_lexer (char *lines[], int line_count);

static void yyerror (const char *);
static void print_value (const s4_val_t *value, int newline);
static void print_list (GList *list);
static void print_result (const s4_resultset_t *res);
static void print_cond (s4_condition_t *cond);
static void print_fetch (s4_fetchspec_t *fetch);
static void print_vars (void);
static void print_help (void);
static GList *result_to_list (GList *prev, const s4_result_t *res);
static GList *rows_to_list (const s4_resultset_t *set, int col);
static GList *cols_to_list (const s4_resultset_t *set, int row);
static GList *set_to_list (const s4_resultset_t *set);
static void add_or_del (int (*func)(), GList *a, GList *b);
static void set_var (const char *key, const char *val);
static const char *get_var (const char *key);
static void print_set_var (const char *key);

GHashTable *cond_table, *res_table, *list_table, *fetch_table;
char **lines;
s4_t *s4;

const char *user_vars[][2] = {{"default_source", "s4"}, {NULL, NULL}};

%}

%code requires {
typedef struct {
	const char *key;
	const s4_val_t *val;
	const char *src;
} list_data_t;
}

%code provides {
#define MAX_LINE_COUNT 128
}

%union {
	char *string;
	int32_t number;
	s4_condition_t *condition;
	s4_val_t *value;
	s4_filter_type_t filter_type;
	s4_resultset_t *result;
	s4_fetchspec_t *fetch;
	GList *list;
	list_data_t *list_data;
}

%token <string> STRING QUOTED_STRING COND_VAR LIST_VAR RESULT_VAR FETCH_VAR
%token <number> INT
%token INFO QUERY ADD DEL VARS SET HELP

%type <value> value
%type <condition> cond
%type <string> string
%type <filter_type> filter_type
%type <result> result
%type <fetch> fetch fetch_list
%type <list> list list_datas
%type <list_data> list_data

%error-verbose
%locations
%debug

%left '|'
%left '&'
%left '!'

%%
input: command ';'
	 | input command ';'
	 ;

command: /* Empty */
	   | cond { print_cond ($1); }
	   | list { print_list ($1); }
	   | result { print_result ($1); }
	   | fetch_list { print_fetch ($1); }
	   | add
	   | del
	   | set
	   | HELP { print_help (); }
	   | VARS { print_vars (); }
	   | COND_VAR '=' cond { g_hash_table_insert (cond_table, $1, $3); }
	   | LIST_VAR '=' list { g_hash_table_insert (list_table, $1, $3); }
	   | FETCH_VAR '=' fetch_list { g_hash_table_insert (fetch_table, $1, $3); }
	   | RESULT_VAR '=' result { g_hash_table_insert (res_table, $1, $3); }
	   ;

set: SET string string { set_var ($2, $3); }
   | SET string { print_set_var ($2); }
   | SET { print_set_var (NULL); }

add: ADD list ',' list { add_or_del (s4_add, $2, $4); }
   ;

del: DEL list ',' list { add_or_del (s4_del, $2, $4); }
   ;

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
			 $$->src = get_var ("default_source");
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
			yyerror ("Undefined list variable");
			YYERROR;
		}
	}
	| '[' list_datas ']' { $$ = $2; }
	| list_data { $$ = g_list_prepend (NULL, $1); }
	| result '[' INT ',' INT ']' { $$ = result_to_list (NULL, s4_resultset_get_result ($1, $3, $5)); }
	| result '[' '_' ',' INT ']' { $$ = rows_to_list ($1, $5); }
	| result '[' INT ',' '_' ']' { $$ = cols_to_list ($1, $3); }
	| result '[' '_' ',' '_' ']' { $$ = set_to_list ($1); }
	;


fetch: string
	 {
		 $$ = s4_fetchspec_create ();
		 s4_fetchspec_add ($$, $1, NULL, S4_FETCH_PARENT | S4_FETCH_DATA);
	 }
	 | '_'
	 {
		 $$ = s4_fetchspec_create ();
		 s4_fetchspec_add ($$, NULL, NULL, S4_FETCH_PARENT | S4_FETCH_DATA);
	 }
	 | fetch ',' '_'
	 {
		 $$ = $1;
		 s4_fetchspec_add ($$, NULL, NULL, S4_FETCH_PARENT | S4_FETCH_DATA);
	 }
	 | fetch ',' string
	 {
		 $$ = $1;
		 s4_fetchspec_add ($$, $3, NULL, S4_FETCH_PARENT | S4_FETCH_DATA);
	 }
	 ;

fetch_list: '[' fetch ']' { $$ = $2; }
		  | FETCH_VAR
		  {
			  $$ = g_hash_table_lookup (fetch_table, $1);
			  free ($1);
			  if ($$ == NULL) {
				  yyerror ("Undefined list variable");
				  YYERROR;
			  }
		  }
		  | '_'
		  {
			  $$ = s4_fetchspec_create ();
			  s4_fetchspec_add ($$, NULL, NULL, S4_FETCH_PARENT | S4_FETCH_DATA);
		  }
		  | string
		  {
			  $$ = s4_fetchspec_create ();
			  s4_fetchspec_add ($$, $1, NULL, S4_FETCH_PARENT | S4_FETCH_DATA);
		  }
		  ;

result: RESULT_VAR
  	  {
		  $$ = g_hash_table_lookup (res_table, $1);
		  free ($1);
		  if ($$ == NULL) {
			  yyerror ("Undefined result variable");
			  YYERROR;
		  }
	  }
	  | QUERY fetch_list cond
	  {
		  const int order[2] = {1, 0};
		  $$ = s4_query (s4, $2,  $3);
		  s4_resultset_sort ($$, order);
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
		   ;

cond: COND_VAR
	{
		$$ = g_hash_table_lookup (cond_table, $1);
		free ($1);
		if ($$ == NULL) {
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
		$$ = s4_cond_new_combiner (S4_COMBINE_AND,
			g_list_prepend (g_list_prepend (NULL, $3), $1));
	}
	| cond '|' cond
	{
		$$ = s4_cond_new_combiner (S4_COMBINE_OR,
			g_list_prepend (g_list_prepend (NULL, $3), $1));
	}
	| '!' cond
	{
		$$ = s4_cond_new_combiner (S4_COMBINE_NOT, g_list_prepend (NULL, $2));
	}
	| STRING filter_type value
	{
		$$ = s4_cond_new_filter ($2, $1, $3, NULL, S4_CMP_CASELESS, 0);
	}
	| filter_type value
	{
		$$ = s4_cond_new_filter ($1, NULL, $2, NULL, S4_CMP_CASELESS, 0);
	}
	| '+' STRING
	{
		$$ = s4_cond_new_filter (S4_FILTER_EXISTS, $2, NULL, NULL, S4_CMP_BINARY, 0);
	}
	| '+'
	{
		$$ = s4_cond_new_filter (S4_FILTER_EXISTS, NULL, NULL, NULL, S4_CMP_BINARY, 0);
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

void print_list (GList *list)
{
	list_data_t *data;
	int first = 1;

	printf ("list [");
	for (; list != NULL; list = g_list_next (list)) {
		if (first) {
			first = 0;
		} else {
			printf (", ");
		}
		data = list->data;
		printf ("%s ", data->key);
		print_value (data->val, 0);
		printf(" %s", data->src);
	}
	printf ("]\n");
}

void print_result (const s4_resultset_t *set)
{
	int col, row;
	const s4_result_t *res;

	for (row = 0; row < s4_resultset_get_rowcount (set); row++) {
		printf ("Row: %i\n", row);
		for (col = 0; col < s4_resultset_get_colcount (set); col++) {
			printf (" Col: %i\n", col);

			for (res = s4_resultset_get_result (set, row, col);
					res != NULL;
					res = s4_result_next (res)) {
				printf ("    %s ", s4_result_get_key (res));
				print_value (s4_result_get_val (res), 0);
				printf (" %s\n", s4_result_get_src (res));
			}
		}
	}
}

void print_cond (s4_condition_t *cond)
{
	printf ("TODO: Implement\n");
}

void print_value (const s4_val_t *val, int newline)
{
	const char *s;
	int32_t i;

	if (s4_val_get_int (val, &i)) {
		printf (newline?"%i\n":"%i", i);
	} else if (s4_val_get_str (val, &s)) {
		printf (newline?"\"%s\"\n":"\"%s\"", s);
	}
}

void print_fetch (s4_fetchspec_t *fetch)
{
	int i;
	printf ("fetchspec [");
	for (i = 0; i < s4_fetchspec_size (fetch); i++) {
		if (i != 0) {
			printf (", ");
		}
		printf ("%s", s4_fetchspec_get_key (fetch, i));
	}
	printf ("]\n");
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
			".add <list>, <list>   - Adds every key,val,src in the second list as\n"
			"                        attributes to every key,val in the first list\n"
			".del <list>, <list>   - Deletes every key,val,src in the second list as\n"
			"                        attributes to every key,val in the first list\n"
			".help                 - Prints this help\n"
			".set key value        - Sets the option key to val\n"
			".set key              - Shows the value of the key\n"
			".set                  - Shows the value of all keys\n"
			".vars                 - Prints all bound variables\n\n"
			"Conditions (<cond>):\n"
			"?var = <cond>         - Assigns cond to the condition variable var\n"
			"?var                  - Returns the condition bound to var\n"
			"key = value           - Matches all entries with key = value\n"
			"key ~ value           - Matches all entries where key matches value\n"
			"key < value           - Matches all entries with key < value\n"
			"key > value           - Matches all entries with key > value\n"
			"+key                  - Matches all entries that has key\n"
			"!cond                 - Matches everything cond does not match\n"
			"cond1 & cond2         - Matches if both cond1 and cond2 matches\n"
			"cond1 | cond2         - Matches if cond1 or cond2 matches\n\n"
			"Fetch specification (<fetch>):\n"
			"%%var = <fetch>        - Assigns fetch to the fetch variable var\n"
			"%%var                  - Returns the fetch spec bound to var\n"
			"[key1, .., keyn]      - Fetches keys 1 through n from matching entries\n"
			"key                   - Fetches key from matching entries\n"
			"_                     - Fetches everything from matching entries\n\n"
			"Results (<cond>):\n"
			".query <fetch> <cond> - Queries the database, returns a result\n\n"
			"@var = <result>       - Assigns var to something returning result\n"
			"@var                  - Returns the result bound to var\n\n"
			"Lists (<list>):\n"
			"$var <list>           - Assigns the list to the list variable var\n"
			"$var                  - Returns the list bound to the variable var\n"
			"<result>[row, col]    - Returns the list at (row,col). If either row\n"
			"                        or col is _, it will take all rows or cols\n"
			"[key val src, ...]    - Creates a list\n"
			"[key val, ...]        - Creates a list where source is set to default_source\n"
			);
}

GList *result_to_list (GList *prev, const s4_result_t *res)
{
	GList *ret = prev;
	list_data_t *data;

	for (; res != NULL; res = s4_result_next (res)) {
		data = malloc (sizeof (list_data_t));
		data->key = s4_result_get_key (res);
		data->val = s4_result_get_val (res);
		data->src = s4_result_get_src (res);
		ret = g_list_prepend (ret, data);
	}

	return ret;
}

GList *rows_to_list (const s4_resultset_t *set, int col)
{
	int row;
	GList *ret = NULL;

	for (row = 0; row < s4_resultset_get_rowcount (set); row++) {
		ret = result_to_list (ret, s4_resultset_get_result (set, row, col));
	}

	return ret;
}

GList *cols_to_list (const s4_resultset_t *set, int row)
{
	int col;
	GList *ret = NULL;

	for (col = 0; col < s4_resultset_get_colcount (set); col++) {
		ret = result_to_list (ret, s4_resultset_get_result (set, row, col));
	}

	return ret;
}

GList *set_to_list (const s4_resultset_t *set)
{
	int row, col;
	GList *ret = NULL;

	for (row = 0; row < s4_resultset_get_rowcount (set); row++) {
		for (col = 0; col < s4_resultset_get_colcount (set); col++) {
			ret = result_to_list (ret, s4_resultset_get_result (set, row, col));
		}
	}

	return ret;
}

void add_or_del (int (*func)(), GList *a, GList *b)
{
	list_data_t *da, *db;
	GList *tmp = b;

	for (; a != NULL; a = g_list_next (a)) {
		da = a->data;
		b = tmp;
		for (; b != NULL; b = g_list_next (b)) {
			db = b->data;
			if (!func (s4, da->key, da->val, db->key, db->val, db->src)) {
				printf ("failed on %s ", da->key);
				print_value (da->val, 0);
				printf (" %s ", db->key);
				print_value (db->val, 0);
				printf (" %s\n", db->src);
			}
		}
	}
}

void set_var (const char *key, const char *val)
{
	int i = 0;

	for (i = 0; user_vars[i][0] != NULL; i++) {
		if (strcmp (key, user_vars[i][0]) == 0) {
			user_vars[i][1] = val;
		}
	}
}

static const char *get_var (const char *key)
{
	int i = 0;

	for (i = 0; user_vars[i][0] != NULL; i++) {
		if (strcmp (key, user_vars[i][0]) == 0) {
			return user_vars[i][1];
		}
	}

	return NULL;
}

void print_set_var (const char *key)
{
	int i = 0;

	for (i = 0; user_vars[i][0] != NULL; i++) {
		if (key == NULL || strcmp (key, user_vars[i][0]) == 0) {
			printf ("%s = %s\n", user_vars[i][0], user_vars[i][1]);
		}
	}
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
	char *ret = "";

	while (ret != NULL && !strlen (ret)) {
		ret = readline (first?"s4> ":"..> ");
		strip (ret);
	}

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

int main(int argc, const char *argv[])
{
	int line_count;

	cond_table = g_hash_table_new_full (g_str_hash, g_str_equal,
		free, (GDestroyNotify)s4_cond_free);
	list_table = g_hash_table_new_full (g_str_hash, g_str_equal,
		free, (GDestroyNotify)g_list_free);
	res_table = g_hash_table_new_full (g_str_hash, g_str_equal,
		free, (GDestroyNotify)s4_resultset_free);
	fetch_table = g_hash_table_new_full (g_str_hash, g_str_equal,
		free, (GDestroyNotify)s4_fetchspec_free);

	g_thread_init (NULL);

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
		}
		return 1;
	}

	while ((lines = rl_get_lines (&line_count)) != NULL) {
		init_lexer (lines, line_count);
		yyparse ();
	}

	s4_close (s4);

	return 0;
}
