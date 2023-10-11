%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"

extern FILE *yyin;

int parse_error;

void yyerror(const char *s) {
    db_error("%s", s);
    parse_error = 1;
}

extern int yylex();

struct db_record_type bs_rtype = {
    .len = 1, .key_len = 1, .col_types = { TYPE_BYTESTR }
};
%}

%token AND     "and"
%token BLOB    "blob"
%token CREATE  "create"
%token EXPLAIN "explain"
%token FROM    "from"
%token INDEX   "index"
%token INSERT  "insert"
%token INTEGER "integer"
%token INTO    "into"
%token JOIN    "join"
%token KEY     "key"
%token LIKE    "like"
%token LIMIT   "limit"
%token OFFSET  "offset"
%token ON      "on"
%token OR      "or"
%token PRIMARY "primary"
%token SCHEMA  "schema"
%token SELECT  "select"
%token TABLE   "table"
%token VALUES  "values"
%token WHERE   "where"

%token NEQ     "<>"
%token LEQ     "<="
%token GEQ     ">="

%start stmts

%union {
    long ival;
    char *str;
    db_scalar_type type;
    struct db_expr *expr;
    struct db_select *select;
    struct db_subquery *subquery;
    struct db_col_def *col_def;
    struct db_table_def *table_def;
    struct db_index_def *index_def;
    struct db_insert *insert;
    list_t list;
}

%token <ival> NUM
%token <str> STRING
%token <str> ID

%type <col_def> col_def
%type <expr> binop
%type <expr> expr
%type <expr> literal
%type <expr> where_clause
%type <index_def> create_index
%type <insert> insert
%type <ival> limit_clause
%type <ival> offset_clause
%type <list> col_defs
%type <list> exprs
%type <list> id_list
%type <list> primary_key
%type <select> select
%type <subquery> from_clause
%type <subquery> join_clause
%type <subquery> table_subquery
%type <table_def> create_table
%type <type> type

%destructor { free($$); } <str>
%destructor { db_expr_free($$); } <expr>
%destructor { db_select_free($$); } <select>
%destructor { db_subquery_free($$); } <subquery>
%destructor { db_col_def_free($$); } <col_def>
%destructor { db_table_def_free($$); } <table_def>
%destructor { db_index_def_free($$); } <index_def>
%destructor { db_insert_free($$); } <insert>
%destructor { list_free((free_fn)db_expr_free, $$); } exprs
%destructor { list_free((free_fn)db_col_def_free, $$); } col_defs
%destructor { list_free(free, $$); } id_list
%destructor { list_free(free, $$); } primary_key

%left "or"
%left "and"
%left "not"
%left '=' "<>" "like"
%left '<' '>' "<=" ">="
%left '+' '-'
%left '*' '/' '%'
%left UMINUS
%nonassoc '.'

%%

stmts : | stmt { YYACCEPT; };

stmt
  : select ';'
    { db_execute_query(db_hdl, $1); db_select_free($1); }
  | "explain" select ';'
    { $2->explain = 1; db_execute_query(db_hdl, $2); db_select_free($2); }
  | create_table ';'
    { db_table_create(db_hdl, $1); db_table_def_free($1); }
  | create_index ';'
    { db_index_create(db_hdl, $1); db_index_def_free($1); }
  | insert ';'
    { db_insert(db_hdl, $1); db_insert_free($1); }
  | "explain" "schema" ';'
    { db_schema_dump(db_hdl); }
  ;

select
  : "select" exprs from_clause where_clause limit_clause offset_clause
    { $$ = calloc(1, sizeof *$$); $$->exprs = $2; $$->from = $3;
      $$->where = $4; $$->limit = $5; $$->offset = $6; }
  ;

exprs
  : expr           { $$ = list_cons($1, LIST_NIL); }
  | expr ',' exprs { $$ = list_cons($1, $3); }
  ;

expr
  : literal
  | binop
  ;

binop
  : ID               { $$ = db_expr_make_ref($1, NULL); }
  | ID '.' ID        { $$ = db_expr_make_ref($3, $1); }
  | '(' expr ')'     { $$ = $2; }
  | expr "or" expr   { $$ = db_expr_make_binop(BINOP_OR,  $1, $3); }
  | expr "and" expr  { $$ = db_expr_make_binop(BINOP_AND, $1, $3); }
  | expr '+' expr    { $$ = db_expr_make_binop(BINOP_ADD, $1, $3); }
  | expr '-' expr    { $$ = db_expr_make_binop(BINOP_SUB, $1, $3); }
  | expr '*' expr    { $$ = db_expr_make_binop(BINOP_MUL, $1, $3); }
  | expr '/' expr    { $$ = db_expr_make_binop(BINOP_DIV, $1, $3); }
  | expr '%' expr    { $$ = db_expr_make_binop(BINOP_MOD, $1, $3); }
  | expr '=' expr    { $$ = db_expr_make_binop(BINOP_EQ,  $1, $3); }
  | expr '<' expr    { $$ = db_expr_make_binop(BINOP_LT,  $1, $3); }
  | expr '>' expr    { $$ = db_expr_make_binop(BINOP_GT,  $1, $3); }
  | expr "<>" expr   { $$ = db_expr_make_binop(BINOP_NEQ, $1, $3); }
  | expr "<=" expr   { $$ = db_expr_make_binop(BINOP_LEQ, $1, $3); }
  | expr ">=" expr   { $$ = db_expr_make_binop(BINOP_GEQ, $1, $3); }
  | expr "like" expr { $$ = db_expr_make_binop(BINOP_LIKE, $1, $3); }
  ;

literal
  : NUM                  { $$ = db_expr_make_num($1); }
  | '-' NUM %prec UMINUS { $$ = db_expr_make_num(-$2); }
  | STRING               { $$ = db_expr_make_string($1); }
  ;

from_clause
  :                    { $$ = NULL; }
  | "from" join_clause { $$ = $2; }
  ;

where_clause
  :              { $$ = NULL; }
  | "where" expr { $$ = $2; };
  ;

limit_clause
  :             { $$ = -1; }
  | "limit" NUM { $$ = $2; }
  ;

offset_clause
  :              { $$ = -1; }
  | "offset" NUM { $$ = $2; }
  ;

join_clause
  : table_subquery
  | join_clause "join" table_subquery
    { $$ = $3; $$->join = $1; $$->constraint = NULL; }
  | join_clause "join" table_subquery "on" expr
    { $$ = $3; $$->join = $1; $$->constraint = $5; }
  ;

table_subquery
  : ID
    { $$ = calloc(1, sizeof *$$); $$->tag = SUBQUERY_TABLE; $$->table = $1; }
  ;

create_table
  : "create" "table" ID '(' col_defs ',' primary_key ')'
    { $$ = calloc(1, sizeof *$$); $$->name = $3;
      $$->col_defs = list_reverse($5); $$->pkey = $7; }
  ;

primary_key
  : "primary" "key" '(' id_list ')' { $$ = $4; }
  ;

id_list
  : ID             { $$ = list_cons($1, LIST_NIL); }
  | ID ',' id_list { $$ = list_cons($1, $3); }
  ;

col_defs
  : col_def              { $$ = list_cons($1, LIST_NIL); }
  | col_defs ',' col_def { $$ = list_cons($3, $1); }
  ;

col_def
  : ID type { $$ = malloc(sizeof *$$); $$->name = $1; $$->type = $2; }
  ;

type
  : "integer" { $$ = TYPE_VARINT; }
  | "blob"    { $$ = TYPE_BYTESTR; }
  ;

create_index
  : "create" "index" ID "on" ID '(' id_list ')'
    { $$ = malloc(sizeof *$$); $$->name = $3; $$->table = $5; $$->cols = $7; }
  ;

insert
  : "insert" "into" ID "values" '(' exprs ')'
    { $$ = malloc(sizeof *$$); $$->table = $3; $$->exprs = $6; }
  ;

%%
