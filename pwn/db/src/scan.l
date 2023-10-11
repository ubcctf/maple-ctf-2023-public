%{
#pragma GCC diagnostic ignored "-Wunused-function"
#include "db.h"
#include "parser.tab.h"

void yyerror(const char *s);

int lexer_eof = 0;

static void strapp(const char *text) {
    yylval.str = realloc(yylval.str, strlen(yylval.str) + strlen(text) + 1);
    strcat(yylval.str, text);
}

%}

%option noyywrap caseless

%x LINE_COMMENT
%x MULTI_COMMENT
%x SINGLE_QUOTE

space       [ \r\n\t]
digit       [0-9]
alpha       [a-zA-Z_]
alnum       [a-zA-Z0-9_]
ident       {alpha}{alnum}*

%%

[(),+\-*/%<=>;.]       { return yytext[0]; }
"<>"                   { return NEQ; }
"<="                   { return LEQ; }
">="                   { return GEQ; }

"and"                  { return AND; }
"blob"                 { return BLOB; }
"create"               { return CREATE; }
"explain"              { return EXPLAIN; }
"from"                 { return FROM; }
"index"                { return INDEX; }
"insert"               { return INSERT; }
"integer"              { return INTEGER; }
"into"                 { return INTO; }
"join"                 { return JOIN; }
"key"                  { return KEY; }
"like"                 { return LIKE; }
"limit"                { return LIMIT; }
"offset"               { return OFFSET; }
"on"                   { return ON; }
"or"                   { return OR; }
"primary"              { return PRIMARY; }
"schema"               { return SCHEMA; }
"select"               { return SELECT; }
"table"                { return TABLE; }
"values"               { return VALUES; }
"where"                { return WHERE; }

{digit}+               { yylval.ival = atol(yytext); return NUM; }

'                      { BEGIN(SINGLE_QUOTE); yylval.str = strdup(""); }
<SINGLE_QUOTE>[^'\0]*  { strapp(yytext); }
<SINGLE_QUOTE>''       { strapp("'"); }
<SINGLE_QUOTE>'        { BEGIN(INITIAL); return STRING; }
<SINGLE_QUOTE>.        { yyerror("lexical error"); return YYerror; }

{alpha}{alnum}*        { yylval.str = strdup(yytext); return ID; }

"--"                   { BEGIN(LINE_COMMENT); }
<LINE_COMMENT>\n       { BEGIN(INITIAL); }
<LINE_COMMENT>.        { }

"/*"                   { BEGIN(MULTI_COMMENT); }
<MULTI_COMMENT>"*/"    { BEGIN(INITIAL); }
<MULTI_COMMENT>.       { }
<MULTI_COMMENT>\n      { }

                       {space}+
.                      { yyerror("lexical error"); return YYerror; }

<<EOF>>                { lexer_eof = 1; yyterminate(); }
