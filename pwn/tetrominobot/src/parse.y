%{
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "robot.h"
#include "game.h"

    int yyparse (game_t *g, tbot_t *t);
    int yylex (game_t *g, tbot_t *t);
    void yyerror (game_t *g, tbot_t *t, char *s);


    /* hack for conditional execution
     *
     * Whether or not the current block can have effects on the game
     * When parsing, this will be set to 0 to disable a block.
     * The block itself will keep track of whether it was zeroed (on the yacc stack)
     */
    int cond_active = 1;

    %}

/* Highest line number = highest precedence */
%define api.value.type {int64_t}
/* %define parse.trace */
%param {game_t *g}
%param {tbot_t *t}

%token GFUNC
%token NUM
%token IF "if"
%token ELSE "else"
%token CALL "call"
%token MEM "mem"
%token PIECE_COUNTER "piece_counter"
%token SCORE "score"
%token PIECE_TYPE "piece_type"
%token PIECE_X "piece_x"
%token PIECE_Y "piece_y"
%token GHOST_Y "ghost_y"
%token PIECE_ANGLE "piece_angle"
%token HOLD_PIECE_TYPE "hold_piece_type"
%token BOARD "board"
%token PRINT "print"
%token PREVIEW "preview"

%token LSHIFT "<<"
%token RSHIFT ">>"
%token LTE "<="
%token GTE ">="
%token EE "=="
%token NE "!="
%token LOR "||"
%token LAND "&&"
%right '='
%right '?' ':'
%left LOR
%left LAND
%left '|' '^' '&'
%left EE NE
%left '>' GTE '<' LTE
%left LSHIFT RSHIFT
%left '+' '-'
%left '*' '/' '%'
%precedence NEG
%precedence '!' '~'

%% /* The grammar follows. */

tbot:
'{' stmts '}' { /* printf("============================\ntbot done!\n"); */}
;

fcall:
"call" '(' GFUNC ')' {
    if (cond_active) {
        $$ = gfunc_call($3, g, t);
        /* printf("\nthe: %d\n", $$); */
    } else {
        $$ = 0xbad;
        /* printf("not called: %s\n", gfunc_table[$3].name); */
    }
}
;

stmts:
%empty
| stmt semicolon.opt stmts
;

stmt:
cond_if
| MEM '[' exp ']' '=' exp {
    if (cond_active) {
        tbot_mem_write(t, $3, $6);
        /* printf("\nthe_mem[%d]: %d\n", $3, $6); */
        $$ = $6; /* maybe? */
        /* printf("mem assigned\n"); */
    } else {
        $$ = 0xbad;
        /* printf("mem not changed\n"); */
    }
}
| fcall
| PRINT '(' exp ')' { if (cond_active) { printf("%ld\n", $3); } }
/* or macro call? */
;

/* value of the midrule action = 1 if it set cond_active to 0 and needs to reset it after */
cond_if:
IF '(' exp ')'         { if (cond_active && !$3) { cond_active = 0; $$ = 1; } else { $$ = 0; } }
'{' stmts '}'          { if ($5) { cond_active = 1; }
    if (cond_active && !!$3) { cond_active = 0; $$ = 1; } else { $$ = 0; } }
cond_else              { if ($9) { cond_active = 1; } }
;

cond_else:
%empty
| ELSE '{' stmts '}'
| ELSE cond_if
;

exp:
NUM
| MEM '[' exp ']'               { $$ = tbot_mem(t, $3); }
| BOARD '[' exp ']' '[' exp ']' { $$ = tbot_board(g, $3, $6); }
| PREVIEW '[' exp ']'           { $$ = tbot_preview(g, $3); }
| "piece_counter"               { $$ = g->p.counter; } // todo
| "score"                       { $$ = g->score; }
| "piece_type"                  { $$ = g->p.s; }
| "piece_x"                     { $$ = g->p.pos.x; }
| "piece_y"                     { $$ = g->p.pos.y; }
| "ghost_y"                     { $$ = ghost_pos(g); }
| "piece_angle"                 { $$ = g->p.angle; }
| "hold_piece_type"             { $$ = g->held;  }
| fcall                         { $$ = $1;       }
| exp "||" exp                  { $$ = $1 || $3; }
| exp "&&" exp                  { $$ = $1 && $3; }
| exp '|' exp                   { $$ = $1 |  $3; }
| exp '^' exp                   { $$ = $1 ^  $3; }
| exp '&' exp                   { $$ = $1 &  $3; }
| exp "==" exp                  { $$ = $1 == $3; }
| exp "!=" exp                  { $$ = $1 != $3; }
| exp '>' exp                   { $$ = $1 >  $3; }
| exp ">=" exp                  { $$ = $1 >= $3; }
| exp '<' exp                   { $$ = $1 <  $3; }
| exp "<=" exp                  { $$ = $1 <= $3; }
| exp "<<" exp                  { $$ = $1 << $3; }
| exp ">>" exp                  { $$ = $1 >> $3; }
| exp '+' exp                   { $$ = $1 +  $3; }
| exp '-' exp                   { $$ = $1 -  $3; }
| exp '*' exp                   { $$ = $1 *  $3; }
| exp '/' exp                   { $$ = $1 /  $3; }
| exp '%' exp                   { $$ = $1 %  $3; }
| '~' exp                       { $$ = ~$2;      }
| '-' exp  %prec NEG            { $$ = -$2;      }
| '!' exp                       { $$ = !$2;      }
| '(' exp ')'                   { $$ = $2;       }
| exp '?' { if (cond_active && !$1) { cond_active = 0; $$ = 1; } else { $$ = 0; } }
  exp ':' { if ($3) { cond_active = 1; }
            if (cond_active && !!$1) { cond_active = 0; $$ = 1; } }
  exp     { if ($6) { cond_active = 1; } $$ = ($1 ? $4 : $7); }
;

semicolon.opt:
%empty
| ';';

%%

void yyerror (game_t *g, tbot_t *t, char *s) {
    (void)g;
    char checked = strlen(t->prog) > t->ppos ? t->prog[t->ppos] : '?';
    fprintf(stderr, "at pos %ld (\"%c\"): %s\n", t->ppos, checked, s);
    exit(1);
}

uint64_t eat_ws(char *p, uint64_t ppos) {
    while (p) {
        switch (p[ppos]) {
        default:
            return ppos;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            ppos++;
        }
    }
    return ppos;
}

/* One past last char of what is probably a token */
uint64_t til_end(char *p, uint64_t ppos) {
    while (p && isgraph(p[ppos])) {
        ppos++;
    }
    return ppos;
}

typedef struct {
    char *s;
    int yyshit;
} tokdef_t;

tokdef_t punct_toks[] = {
    {"<<", LSHIFT},
    {">>", RSHIFT},
    {"<=", LTE},
    {">=", GTE},
    {"==", EE},
    {"!=", NE},
    {"||", LOR},
    {"&&", LAND},
    {0, 0}
};

/* multi-char non-gamefunc toks */
tokdef_t alpha_toks[] = {
    {"if", IF},
    {"else", ELSE},
    {"piece_counter", PIECE_COUNTER},
    {"score", SCORE},
    {"piece_type", PIECE_TYPE},
    {"piece_x", PIECE_X},
    {"piece_y", PIECE_Y},
    {"ghost_y", GHOST_Y},
    {"piece_angle", PIECE_ANGLE},
    {"hold_piece_type", HOLD_PIECE_TYPE},
    {"board", BOARD},
    {"mem", MEM},
    {"call", CALL},
    {"print", PRINT},
    {"preview", PREVIEW},
    {0, 0}
};

/* Returns position.
 * l (until whitespace) must be at least the length of the search term
 */
int toksearch(tokdef_t *tlist, char *tok, uint64_t l) {
    for (int i = 0; tlist[i].s != NULL; i++) {
        if (l >= strlen(tlist[i].s) && !strncmp(tok, tlist[i].s, l)) {
            /* printf(" %s ", tlist[i].s); */
            return i;
        }
    }
    return -1;
}

/*
 * Eats whitespace, checks the token's first char,
 * finds token's end, and increments the ppos accordingly.
 *
 * Tokens satisfy one of the following delicate checks:
 * - (ispunct) Two-character ops and parens
 * -           Single-character ops and parens
 * - (isdigit) Digits
 * - (isalpha or _) game vars or funcs
 *
 * Treat anything else as EOF.
 */
int yylex (game_t *g, tbot_t *t) {
    (void)g;
    /* yydebug = 1; */

    t->ppos = eat_ws(t->prog, t->ppos);
    uint64_t p = t->ppos;
    uint64_t max_end = til_end(t->prog, p);
    /* int out = YYUNDEF; */

    if (ispunct(t->prog[p])) {
        int t_i = toksearch(punct_toks, t->prog + p, max_end - p);
        if (t_i != -1) {
            /* long punct tok */
            t->ppos = p + strlen(punct_toks[t_i].s);
            return punct_toks[t_i].yyshit;
        } else {
            /* one char */
            /* printf("%c", t->prog[p]); */
            t->ppos = p + 1;
            return t->prog[p];
        }

    } else if (isdigit(t->prog[p])) {
        int end = p;
        /* todo: hex? */
        while (isdigit(t->prog[end])) {
            end++;
        }
        if (sscanf(t->prog + p, "%ld", &yylval) != 1) {
            abort();
        }
        /* printf("%d", yylval); */
        t->ppos = end;
        return NUM;

    } else if (isalpha(t->prog[p]) || t->prog[p] == '_') {
        int end = p;
        while (isalnum(t->prog[end]) || t->prog[end] == '_') {
            end++;
        }
        t->ppos = end;

        int at_i = toksearch(alpha_toks, t->prog + p, end - p);
        int gf_i = gfunc_i(t->prog + p, end - p);
        if (at_i != -1) {
            return alpha_toks[at_i].yyshit;
        } else if (gf_i != -1) {
            yylval = gf_i;
            return GFUNC;
        } else {
            return YYUNDEF;
        }
    } else {
        return YYEOF;
    }
}
