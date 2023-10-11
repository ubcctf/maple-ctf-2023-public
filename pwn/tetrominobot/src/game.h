#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define BOARD_W 10
#define BOARD_H 22

#define PIECE_PREVIEWS 5

typedef enum {
    P_NONE = 0,
    P_I = 1,
    P_O = 2,
    P_T = 3,
    P_J = 4,
    P_L = 5,
    P_S = 6,
    P_Z = 7,
    P_WALL = 8,
} shape_t;

typedef uint64_t score_t;

typedef struct {
    int x;
    int y;
} pos_t;

typedef struct {
    pos_t pos;
    shape_t s;
    int colour;
    int gcolour;
    uint8_t angle;
    uint16_t counter;
} piece_t;

typedef shape_t board_t[BOARD_H][BOARD_W];

typedef struct {
    board_t board;
    score_t score;
    piece_t p;
    /* since no gravity, no point in limiting # of holds per piece */
    shape_t held;
    shape_t preview[PIECE_PREVIEWS];
    /* whether or not player is playing by hand */
    int practice;
    char *name;
} game_t;

typedef struct {
    char *name;
    shape_t (*f)(game_t *g);
} move;

shape_t rand_shape(void);
void get_cells(piece_t p, pos_t out[4]);
game_t *new_game(game_t *g, char *name);
shape_t check_dead(game_t *g, const piece_t *const p);
int ghost_pos(game_t *g);
shape_t print_game(game_t *g);
score_t clear_lines_write_shape(game_t *g);
int advance_shape(game_t *g);
shape_t move_left(game_t *g);
shape_t move_right(game_t *g);
shape_t move_down(game_t *g);
shape_t move_sdrop(game_t *g);
shape_t _move_commit(game_t *g);
shape_t move_drop(game_t *g);
shape_t move_rot_r(game_t *g);
shape_t move_rot_180(game_t *g);
shape_t move_rot_l(game_t *g);
shape_t move_hold(game_t *g);
shape_t add_piece_manual(game_t *g);
score_t play_manual(game_t *g);

#endif
