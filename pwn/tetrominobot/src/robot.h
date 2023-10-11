#ifndef ROBOT_H
#define ROBOT_H

#include "game.h"
#include <stdint.h>

#define MEM_COUNT 0x100
#define BOT_NAME_SIZE 0x20
#define BOT_ITERATIONS 30000
#define PROG_SIZE 0x8000
#define PIECE_TIMEOUT 0x100

typedef int64_t mem_t;

typedef struct {
    mem_t mem[MEM_COUNT];
    char *name;
    uint64_t ppos;
    char *prog;
    int debug;
    uint64_t mvcount;
} tbot_t;

tbot_t *tbot_new(tbot_t *t, char *name, char *prog, int debug);
int tbot_run(tbot_t *t, game_t *g);

typedef shape_t(gfunc_t)(game_t *g);

typedef struct {
    char *name;
    gfunc_t *f;
} gfte_t;

/* struct purely to make the overwite between two global structs easier without
 * a linker script */
typedef struct {
    gfte_t gfunctable[11];
    tbot_t global_tbot;
} buggy_t;

extern gfte_t (*const gfunc_table);

int gfunc_i(char *name, uint64_t n);
int gfunc_call(int i, game_t *g, tbot_t *t);
int gfunct_extend(gfte_t fte);

/* checked array accesses */
mem_t tbot_mem(tbot_t *t, int i);
void tbot_mem_write(tbot_t *t, int i, mem_t val);
int tbot_board(game_t *g, int y, int x);
int tbot_preview(game_t *g, int i);

#endif
