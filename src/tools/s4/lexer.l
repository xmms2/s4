%{
#include <s4.h>
#include <stdio.h>
#include <readline/readline.h>
#include "main.tab.h"

static char *process_string (const char *);

YY_BUFFER_STATE buffer_lines[MAX_LINE_COUNT];
int cur_line, cur_col, buffer_line_count;

#define YY_USER_ACTION { \
	yylloc.first_line = yylloc.last_line = cur_line; \
	yylloc.first_column = cur_col; \
	cur_col += yyleng; \
	yylloc.last_column = cur_col;}
%}

STRING_CHAR [[:alnum:]]
STRING [[:alnum:]][[:alnum:]_]*
QUOTED_STRING \"([^\\\"]|\\\"|\\[^x]|\\x[a-fA-F0-9]{2})*\"
INT [0-9]+

%%

\.add|\.a return ADD; 
\.del|\.d return DEL;
\.info|\.i return INFO;
\.query|\.q return QUERY;
\.vars|\.v return VARS;
\.set return SET;
\.help|\.h|\.\? return HELP;
{INT} yylval.number = atoi (yytext); return INT;
{STRING} yylval.string = strdup (yytext); return STRING;
{QUOTED_STRING} yylval.string = process_string (yytext + 1); return QUOTED_STRING;
\?{STRING} yylval.string = strdup (yytext + 1); return COND_VAR;
\@{STRING} yylval.string = strdup (yytext + 1); return RESULT_VAR;
\${STRING} yylval.string = strdup (yytext + 1); return LIST_VAR;
\%{STRING} yylval.string = strdup (yytext + 1); return FETCH_VAR;
[[:space:]]+
. return yytext[0];

%%

#define HEX_TO_INT(c) (((c) & 0xf) + (((c) & 0x40) >> 6) * 9)

char *process_string (const char *str)
{
	char *s, *ret = strdup (str);
	int len = strlen (str);

	ret[--len] = '\0';

	for (s = ret; len; s++, len--) {
		if (*s == '\\') {
			int skip = 1;
			switch (s[1]) {
			case 'n':
				*s = '\n';
				break;
			case 't':
				*s = '\t';
				break;
			case 'r':
				*s = '\r';
				break;
			case 'x':
				skip = 3;
				*s = HEX_TO_INT (s[2]) * 16 + HEX_TO_INT (s[3]);
				break;
			default:
				*s = s[1];
				break;
			}

			len -= skip;
			memmove (s + 1, s + 1 + skip, len - 1);
		}
	}
	*s = '\0';
	return ret;
}

void init_lexer (char *lines[], int line_count)
{
	int i;

	for (i = 0; i < line_count; i++) {
		buffer_lines[i] = yy_scan_string (lines[i]);
	}

	cur_line = 0;
	cur_col = 0;
	buffer_line_count = line_count;
	yy_switch_to_buffer (buffer_lines [0]);
}

int yywrap ()
{
	yy_delete_buffer (buffer_lines [cur_line]);

	if (++cur_line >= buffer_line_count)
		return 1;

	cur_col = 0;
	yy_switch_to_buffer (buffer_lines [cur_line]);
	return 0;
}
