#include "game.h"
#include "util.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const int PIECE_COLOURS[] = {16, 14, 11, 13, 12, 208, 10, 9};
const int GHOST_COLOURS[] = {16, 6, 3, 5, 4, 130, 2, 1};

/*
  x 0 1 2 3
  y
  0
  1
  2
  3
*/

/* [angle][shape][cell] */
const pos_t SHAPES[4][8][4] = {
    {{{0}},
     {{0, 1}, {1, 1}, {2, 1}, {3, 1}},  /* I */
     {{1, 1}, {1, 2}, {2, 1}, {2, 2}},  /* O */
     {{1, 1}, {0, 1}, {1, 0}, {2, 1}},  /* T */
     {{0, 0}, {0, 1}, {1, 1}, {2, 1}},  /* J */
     {{0, 1}, {1, 1}, {2, 1}, {2, 0}},  /* L */
     {{0, 1}, {1, 1}, {1, 0}, {2, 0}},  /* S */
     {{0, 0}, {1, 0}, {1, 1}, {2, 1}}}, /* Z */

    {{{0}},
     {{2, 0}, {2, 1}, {2, 2}, {2, 3}},  /* I */
     {{1, 1}, {1, 2}, {2, 1}, {2, 2}},  /* O */
     {{1, 1}, {1, 0}, {2, 1}, {1, 2}},  /* T */
     {{1, 0}, {2, 0}, {1, 1}, {1, 2}},  /* J */
     {{1, 0}, {2, 2}, {1, 1}, {1, 2}},  /* L */
     {{1, 0}, {1, 1}, {2, 1}, {2, 2}},  /* S */
     {{2, 0}, {1, 1}, {2, 1}, {1, 2}}}, /* Z */

    {{{0}},
     {{0, 2}, {1, 2}, {2, 2}, {3, 2}},  /* I */
     {{1, 1}, {1, 2}, {2, 1}, {2, 2}},  /* O */
     {{1, 1}, {2, 1}, {1, 2}, {0, 1}},  /* T */
     {{0, 1}, {1, 1}, {2, 1}, {2, 2}},  /* J */
     {{0, 1}, {1, 1}, {2, 1}, {0, 2}},  /* L */
     {{0, 2}, {1, 2}, {1, 1}, {2, 1}},  /* S */
     {{0, 1}, {1, 1}, {1, 2}, {2, 2}}}, /* Z */

    {{{0}},
     {{1, 0}, {1, 1}, {1, 2}, {1, 3}}, /* I */
     {{1, 1}, {1, 2}, {2, 1}, {2, 2}}, /* O */
     {{1, 1}, {1, 2}, {0, 1}, {1, 0}}, /* T */
     {{1, 0}, {1, 1}, {0, 2}, {1, 2}}, /* J */
     {{1, 0}, {1, 1}, {0, 0}, {1, 2}}, /* L */
     {{0, 0}, {0, 1}, {1, 1}, {1, 2}}, /* S */
     {{1, 0}, {0, 1}, {1, 1}, {0, 2}}} /* Z */
};

#define NO_KICKS                                                               \
    {                                                                          \
        {0, 0}, {0, 0}, {0, 0}, {0, 0}, { 0, 0 }                               \
    }

/* Super rotation system (SRS) kicklist from wiki */

/* KICKS[old_angle][new_angle][attempt] */
const pos_t KICKS_I[4][4][5] = {{NO_KICKS,
                                 {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},
                                 NO_KICKS,
                                 {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}}},
                                {{{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},
                                 NO_KICKS,
                                 {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},
                                 NO_KICKS},
                                {NO_KICKS,
                                 {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},
                                 NO_KICKS,
                                 {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}}},
                                {{{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},
                                 NO_KICKS,
                                 {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},
                                 NO_KICKS}};

const pos_t KICKS_A[4][4][5] = {{NO_KICKS,
                                 {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},
                                 NO_KICKS,
                                 {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}},
                                {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},
                                 NO_KICKS,
                                 {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},
                                 NO_KICKS},
                                {NO_KICKS,
                                 {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},
                                 NO_KICKS,
                                 {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}},
                                {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}},
                                 NO_KICKS,
                                 {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}},
                                 NO_KICKS}};

int bag_fullness = 7;
char bag[7];

/* at least a little random */
shape_t rand_shape(void) {
    if (bag_fullness == 0) {
        memset(bag, 0, sizeof bag);
        bag_fullness = 7;
    }
    int r = (rand() >> 12) % bag_fullness;
    int p;
    for (p = 0; p < 7; p++) {
        if (bag[p] == 0) {
            if (r == 0) {
                bag[p] = 1;
                break;
            }
            r--;
        }
    }
    bag_fullness--;
    return p + 1;

    /* I pieces only for now */
    /* return 1; */
}

void get_cells(piece_t p, pos_t out[4]) {
    for (int i = 0; i < 4; i++) {
        pos_t cell = SHAPES[p.angle & 3][p.s][i];
        out[i].x = p.pos.x + cell.x;
        out[i].y = p.pos.y + cell.y;
    }
}

piece_t new_piece(shape_t s) {
    piece_t p;
    p.s = s != 0 ? s : rand_shape();
    p.colour = PIECE_COLOURS[p.s];
    p.gcolour = GHOST_COLOURS[p.s];
    p.pos.x = 3;
    p.pos.y = 0;
    p.angle = 0;
    p.counter = 0;
    return p;
}

game_t *new_game(game_t *g, char *gname) {
    if (!g) {
        g = calloc(sizeof(game_t), 1);
    } else {
        memset(g, 0, sizeof(game_t));
    }
    if (!gname) {
        gname = "";
    }
    /* fixme add to game struct */
    memset(bag, 0, sizeof bag);
    bag_fullness = 7;
    g->p = new_piece(0);
    for (int i = 0; i < PIECE_PREVIEWS; i++) {
        g->preview[i] = rand_shape();
    }
    g->name = gname;
    return g;
}

/* Return true if p overlaps with any placed piece in g
 * BUG: The piece can be above the board, so the returned value can be an
 * out-of-bounds read */
shape_t check_dead(game_t *g, const piece_t *const p) {
    pos_t cells[4];
    get_cells(*p, cells);
    for (int i = 0; i < 4; i++) {
        shape_t on_board = g->board[cells[i].y][cells[i].x];
        if (on_board != P_NONE) {
            /* printf("check_dead: %x\n", on_board); */
            return on_board;
        }
    }
    return P_NONE;
}

/* y-position of the ghost piece */
int ghost_pos(game_t *g) {
    piece_t ghost = g->p;
    piece_t test_ghost = ghost;
    test_ghost.pos.y++;
    while (1) {
        pos_t cells[4];
        get_cells(test_ghost, cells);
        int max_y = max4(cells[0].y, cells[1].y, cells[2].y, cells[3].y);
        if (max_y >= BOARD_H) {
            break;
        }
        if (check_dead(g, &test_ghost)) {
            break;
        }
        ghost.pos.y++;
        test_ghost.pos.y++;
    }
    return ghost.pos.y;
}

shape_t print_game(game_t *g) {
    clear_term();
    board_t outb = {0};
    /* board pieces and voids */
    for (int y = 0; y < BOARD_H; y++) {
        for (int x = 0; x < BOARD_W; x++) {
            outb[y][x] = PIECE_COLOURS[g->board[y][x]];
        }
    }

    piece_t ghost = g->p;
    ghost.pos.y = ghost_pos(g);
    pos_t ghost_cells[4];
    get_cells(ghost, ghost_cells);
    for (int i = 0; i < 4; i++) {
        outb[ghost_cells[i].y][ghost_cells[i].x] = g->p.gcolour;
    }

    /* when dead, draw ghost of curr piece only */
    int dead = check_dead(g, &g->p);
    pos_t cells[4];
    if (!dead) {
        /* curr piece */
        get_cells(g->p, cells);
        for (int i = 0; i < 4; i++) {
            outb[cells[i].y][cells[i].x] = g->p.colour;
        }
    }

    int lpanel[BOARD_H][6];
    int rpanel[BOARD_H][6];
    for (int y = 0; y < BOARD_H; y++) {
        for (int x = 0; x < 6; x++) {
            lpanel[y][x] = PIECE_COLOURS[P_NONE];
            rpanel[y][x] = PIECE_COLOURS[P_NONE];
        }
    }

    /* lpanel: hold piece */
    if (g->held != P_NONE) {
        piece_t held = new_piece(g->held);
        held.pos.x = 1;
        held.pos.y = 1;
        get_cells(held, cells);
        for (int i = 0; i < 4; i++) {
            lpanel[cells[i].y][cells[i].x] = PIECE_COLOURS[held.s];
        }
    }

    /* rpanel: next */
    for (int i = 0; i < PIECE_PREVIEWS; i++) {
        piece_t prv = new_piece(g->preview[i]);
        prv.pos.x = 1;
        prv.pos.y = i * 4 + 1;
        get_cells(prv, cells);
        for (int i = 0; i < 4; i++) {
            rpanel[cells[i].y][cells[i].x] = PIECE_COLOURS[prv.s];
        }
    }

    printf("\n");

    printf("+------------+--------------------+------------+\n");
    printf("| Hold:      | ");
    int namelen = strlen(g->name);
    for (int i = 0; i < 18; i++) {
        putchar(i < namelen && isprint(g->name[i]) ? g->name[i] : ' ');
    }
    printf(" | Next:      |\n");

    printf("+------------+--------------------+------------+\n");

    for (int y = 0; y < BOARD_H; y++) {
        printf("|");
        for (int x = 0; x < 6; x++) {
            printf("\e[48;5;%dm  ", lpanel[y][x]);
        }
        printf(TERM_ENDCOLOUR);
        printf("|");
        for (int x = 0; x < BOARD_W; x++) {
            printf("\e[48;5;%dm  ", outb[y][x]);
        }
        printf(TERM_ENDCOLOUR);
        printf("|");
        for (int x = 0; x < 6; x++) {
            printf("\e[48;5;%dm  ", rpanel[y][x]);
        }
        printf(TERM_ENDCOLOUR);
        printf("|\n");
    }
    printf("+------------+--------------------+------------+\n");
    if (g->practice) {
        printf("|            | wasd  SPC   . - =  |            |\n");
        printf("+------------+--------------------+------------+\n");
    }
    fflush(stdout);
    return 0;
}

/* hacky and bad. had to move the shape write here since otherwise can't handle
 * t-spin. g2 is the same as g; it exists just to get leaked.  there is
 * literally nothing interesting on the stack except the ra, which requires i
 * hop over the cookie and rbp, so i copy the game pointer after the bad array
 * instead. */
score_t clear_lines_write_shape(game_t *g) {
    volatile struct {
        score_t scores[5];
        game_t *g2;
    } tsp_bug = {{0, 100, 300, 500, 800}, g};

    int should_clear = 0;
    int rowcnt[BOARD_H] = {0}; /* row fullness */

    pos_t curr_cells[4];
    get_cells(tsp_bug.g2->p, curr_cells);

    for (int i = 0; i < 4; i++) {
        /* count the current piece */
        rowcnt[curr_cells[i].y]++;
    }
    for (int y = 0; y < BOARD_H; y++) {
        for (int x = 0; x < BOARD_W; x++) {
            if (tsp_bug.g2->board[y][x] != P_NONE) {
                rowcnt[y]++;
            }
        }
        if (rowcnt[y] == BOARD_W) {
            should_clear = 1;
        }
    }

    /* early return if no lines to clear */
    if (!should_clear) {
        /* writing the shape to the board */
        for (int i = 0; i < 4; i++) {
            tsp_bug.g2->board[curr_cells[i].y][curr_cells[i].x] =
                tsp_bug.g2->p.s;
        }
        return 0;
    }

    int score_i = 0;

    if (tsp_bug.g2->p.s == P_T) {
        /* puts("is T"); */
        piece_t new_p = tsp_bug.g2->p;
        new_p.pos.y = tsp_bug.g2->p.pos.y - 1;

        pos_t cells[4];
        get_cells(new_p, cells);
        int min_y = min4(cells[0].y, cells[1].y, cells[2].y, cells[3].y);
        if (min_y >= 0 && check_dead(g, &new_p)) {
            /* BUG: If we find a t-spin (last piece was a t piece AND it cannot
             * be moved up one spot AND we clear at least one line) we try to
             * boost the score by setting score_i. Since the line clear counting
             * happens after regardless, we get max points for a t-spin single,
             * and two possible leaks for 2 and 3 cleared lines respectively.
             */
            score_i = 3;
        }
    }

    /* writing the shape to the board */
    for (int i = 0; i < 4; i++) {
        tsp_bug.g2->board[curr_cells[i].y][curr_cells[i].x] = tsp_bug.g2->p.s;
    }

    int src_y = BOARD_H - 1;
    for (int dst_y = BOARD_H - 1; dst_y >= 0; dst_y--, src_y--) {
        if (src_y < 0) { /* nothing to shift; copy a blank line */
            for (int x = 0; x < BOARD_W; x++) {
                tsp_bug.g2->board[dst_y][x] = P_NONE;
            }
        } else {
            while (rowcnt[src_y] == BOARD_W && src_y >= 0) { /* clear */
                src_y--;
                score_i++;
            }
            for (int x = 0; x < BOARD_W; x++) {
                tsp_bug.g2->board[dst_y][x] = tsp_bug.g2->board[src_y][x];
            }
        }
    }

    fflush(stdout);
    return tsp_bug.scores[score_i];
}

/* Shift the queue forward, filling in the new spot, and throwing away curr.
 * Assumes curr piece has already been written to the board if necessary. */
int advance_shape(game_t *g) {
    g->p = new_piece(g->preview[0]);
    for (int i = 0; i < PIECE_PREVIEWS - 1; i++) {
        g->preview[i] = g->preview[i + 1];
    }
    g->preview[PIECE_PREVIEWS - 1] = rand_shape();
    return 0;
}

shape_t _move_lr(game_t *g, int diff) {
    g->p.counter++;
    int old_x = g->p.pos.x;
    piece_t new_p = g->p;
    new_p.pos.x = old_x + diff;

    /* check board excursion */
    pos_t cells[4];
    get_cells(new_p, cells);
    int min_x = min4(cells[0].x, cells[1].x, cells[2].x, cells[3].x);
    int max_x = max4(cells[0].x, cells[1].x, cells[2].x, cells[3].x);
    if (min_x < 0 || max_x >= BOARD_W) {
        return P_WALL;
    }

    shape_t dead = check_dead(g, &new_p);
    if (dead) {
        return dead;
    }

    g->p = new_p;
    return 0;
}

shape_t move_left(game_t *g) { return _move_lr(g, -1); }
shape_t move_right(game_t *g) { return _move_lr(g, 1); }
shape_t move_down(game_t *g) {
    g->p.counter++;
    int old_y = g->p.pos.y;
    piece_t new_p = g->p;
    new_p.pos.y = old_y + 1;

    pos_t cells[4];
    get_cells(new_p, cells);
    int max_y = max4(cells[0].y, cells[1].y, cells[2].y, cells[3].y);
    if (max_y >= BOARD_H) {
        return P_WALL;
    }

    shape_t dead = check_dead(g, &new_p);
    if (dead) {
        return dead;
    }

    g->p = new_p;
    return 0;
}

shape_t move_sdrop(game_t *g) {
    g->p.counter++;
    while (!move_down(g))
        ;
    return move_down(g);
}

/* For hard drop */
shape_t _move_commit(game_t *g) {
    int dead = check_dead(g, &g->p);
    if (dead)
        return dead;

    g->score += clear_lines_write_shape(g);
    advance_shape(g);
    return check_dead(g, &g->p);
}

shape_t move_drop(game_t *g) {
    g->p.counter++;
    move_sdrop(g);
    return _move_commit(g);
}

/* Rotate a test piece and try each of the five coordinates in the kick list
 * (where 0,0 is tried first). If all fail, don't rotate */
shape_t _move_rot(game_t *g, int old_a, int new_a) {
    g->p.counter++;
    const pos_t(*kicklist)[5];
    if (g->p.s == P_O) {
        return 0;
    } else if (g->p.s == P_I) {
        kicklist = &KICKS_I[old_a & 3][new_a & 3];
    } else {
        kicklist = &KICKS_A[old_a & 3][new_a & 3];
    }

    uint32_t lol_out = P_WALL;

    for (int i = 0; i < 5; i++) {
        piece_t new_p = g->p;
        new_p.angle = new_a;
        new_p.pos.x += (*kicklist)[i].x;
        new_p.pos.y += (*kicklist)[i].y;

        pos_t cells[4];
        get_cells(new_p, cells);
        int min_x = min4(cells[0].x, cells[1].x, cells[2].x, cells[3].x);
        int max_x = max4(cells[0].x, cells[1].x, cells[2].x, cells[3].x);
        int max_y = max4(cells[0].y, cells[1].y, cells[2].y, cells[3].y);

        if (min_x < 0 || max_x >= BOARD_W || max_y >= BOARD_H) {
            lol_out = lol_out > (uint32_t)P_WALL ? lol_out : P_WALL;
        } else if (check_dead(g, &new_p)) {
            uint32_t cd = (uint32_t)check_dead(g, &new_p);
            lol_out = lol_out > cd ? lol_out : cd;
        } else {
            g->p = new_p;
            return 0;
        }
    }
    return lol_out;
}

shape_t move_rot_r(game_t *g) {
    return _move_rot(g, g->p.angle, (g->p.angle + 1) & 3);
}

shape_t move_rot_180(game_t *g) {
    return _move_rot(g, g->p.angle, (g->p.angle + 2) & 3);
}

shape_t move_rot_l(game_t *g) {
    return _move_rot(g, g->p.angle, (g->p.angle - 1) & 3);
}

/* can fail due to no space to swap the piece */
shape_t move_hold(game_t *g) {
    if (g->held == P_NONE) {
        piece_t test_p = new_piece(g->preview[0]);
        shape_t dead = check_dead(g, &test_p);
        if (dead) {
            return dead;
        }
        g->held = g->p.s;
        advance_shape(g);
    } else {
        piece_t new_p = new_piece(g->held);
        shape_t dead = check_dead(g, &new_p);
        if (dead) {
            return 1;
        }
        g->held = g->p.s;
        g->p = new_p;
    }
    return 0;
}

/* just for testing the game */
/* handle inputs until drop, clear lines, spawn new piece, check for death */
shape_t add_piece_manual(game_t *g) {
    print_game(g);
    while (1) {
        char in = getchar();
        switch (in) {
        case '-':
            move_left(g);
            break;
        case '=':
            move_right(g);
            break;
        case '.':
            return move_drop(g);
        case 'w':
            move_hold(g);
            break;
        case 'a':
            move_rot_l(g);
            break;
        case 's':
            move_rot_180(g);
            break;
        case 'd':
            move_rot_r(g);
            break;
        case ' ':
            move_down(g);
            break;
        default:
            continue;
        }
        print_game(g);
    }
}

score_t play_manual(game_t *g) {
    /* TODO set up term fixes */
    set_up_term();
    srand(time(NULL));
    int done = 0;
    g = new_game(g, "*Practice mode*");
    g->practice = 1;
    do {
        done = add_piece_manual(g);
    } while (!done);

    score_t score = g->score;
    printf("Score: %lu\n", score);
    restore_term();
    return score;
}
