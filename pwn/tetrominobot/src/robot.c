#include "robot.h"
#include "game.h"
#include "out/parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

buggy_t buggy = {{{"left", move_left},
                  {"right", move_right},
                  {"down", move_down},
                  {"drop", move_drop},
                  {"rot_l", move_rot_l},
                  {"rot_r", move_rot_r},
                  {"rot_180", move_rot_180},
                  {"hold", move_hold},
                  {"sdrop", move_sdrop},
                  {0},
                  {0}},
                 {0}};

/* ideally not revealing the struct (tbot = &tbot + 0, not &buggy + sizeof
 * functable) */
gfte_t(*const gfunc_table) = (gfte_t *const)&buggy.gfunctable;
tbot_t *const global_tbot = &buggy.global_tbot;

tbot_t *tbot_new(tbot_t *t, char *name, char *prog, int debug) {
    if (!t) {
        t = global_tbot;
    }
    memset(t, 0, sizeof(tbot_t));
    *t = (tbot_t){.name = name, .prog = prog, .debug = debug};
    if (t->debug) {
        gfunct_extend((gfte_t){.name = "commit", .f = &_move_commit});
        gfunct_extend(
            (gfte_t){.name = "dump", .f = (shape_t(*)(game_t *)) & print_game});
    }

    /* piece generator starting config is easily choosable by putting an EOF in
     * the program and whatever characters after.
     *
     * player probably doesn't even care to predict; having the same set of
     * pieces every time should be sufficient.
     *
     * patching the binary to srand(particular number) in practice mode is
     * probably what i would do to hit the ceiling bug
     */
    uint16_t not_random = 0;
    int plen = strlen(t->prog);
    for (int i = 0; i < plen; i++) {
        not_random += t->prog[i];
    }
    /* printf("SRAND: %d", not_random); */
    srand(not_random);

    if (t->debug) {
        printf("program: %s\n", t->prog);
    }

    return t;
}

int tbot_run(tbot_t *t, game_t *g) {
    /* this is horrible */
    for (int i = 0; i < BOT_ITERATIONS; i++) {
        t->ppos = 0;
        yyparse(g, t);
        if (check_dead(g, &g->p)) {
            break;
        }
        /* if (t->debug) { */
        /*     print_game(g); */
        /* } */
    }
    return 1;
}

int gfunc_i(char *name, uint64_t n) {
    for (int i = 0; gfunc_table[i].name != NULL; i++) {
        if (!strncmp(gfunc_table[i].name, name, n)) {
            return i;
        }
    }
    return -1;
}

/* BUG: using null as the end */
int gfunc_call(int i, game_t *g, tbot_t *t) {
    t->mvcount++;
    /* just for pwn */
    shape_t(*f)(game_t *, int what, tbot_t *) = (shape_t (*)(game_t *, int what, tbot_t *))gfunc_table[i].f;
    int ok = f(g, 0, t);
    return ok;
}

/* BUG: no */
int gfunct_extend(gfte_t fte) {
    int i = 0;
    for (; gfunc_table[i].name != NULL; i++) {
        if (!strcmp(gfunc_table[i].name, fte.name)) {
            return -1;
        }
    }
    gfunc_table[i] = fte;
    return i;
}

mem_t tbot_mem(tbot_t *t, int i) {
    if (i < 0 || i >= MEM_COUNT) {
        puts("Out of bounds");
        exit(1);
    }
    return t->mem[i];
}

void tbot_mem_write(tbot_t *t, int i, mem_t val) {
    if (i < 0 || i >= MEM_COUNT) {
        puts("Out of bounds");
        exit(1);
    }
    t->mem[i] = val;
}

int tbot_board(game_t *g, int y, int x) {
    if (x < 0 || y < 0 || x >= BOARD_W || y >= BOARD_H) {
        puts("Out of bounds");
        exit(1);
    }
    return g->board[y][x];
}

int tbot_preview(game_t *g, int i) {
    if (i < 0 || i >= PIECE_PREVIEWS) {
        puts("Out of bounds");
        exit(1);
    }
    return g->preview[i];
}
