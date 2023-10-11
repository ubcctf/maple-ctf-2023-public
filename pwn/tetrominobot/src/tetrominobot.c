#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "game.h"
#include "robot.h"
/* hack. should have util.c */
/* #include "util.h" */

/* considering a global timeout (seconds) for infra reasons */

/* BUG: The checker for option arguments is quite lenient.
   1. If we're in practice mode, bypass everything and play.
   2. If the bot name is not specified with -n, ask.
   3. If the program is not speficied with -b, ask.
   4. We check for the -d option in everything we've been given, incl. bot name
   5. Retry: if the -b program is not specified and the user says yes, goto 2.

   so starting your bot name with -d will trigger debug mode.

   this bug is a little silly, but i'm going with it.
 */
game_t global_game = {0};

int option_i(char **av2, int n, char c) {
    for (int i = 0; i < n; i++) {
        if (av2[i] && av2[i][0] == '-' && strchr(av2[i], c)) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    int ac2 = argc + 2; /* we need the name and the program */
    char **av2 = calloc(ac2, sizeof(int *));

    for (int i = 0; i < ac2 && i < argc; i++) {
        av2[i] = argv[i];
    }

    if (option_i(av2, ac2, 'h') != -1) {
        puts("Usage: ./tetrominobot [options] ");
        puts("       -b [program]   specify the bot and run once\n"
             "                      (remember to quote input)");
        puts("       -n [name]      specify the bot name");
        puts("       -d             debug mode");
        puts("       -h             print this help");
        puts("       -p             practice mode (overrides other flags)");
        exit(0);
    }

    if (option_i(av2, ac2, 'p') != -1) {
        play_manual(&global_game);
        exit(0);
    }

    char retry[3] = {0};
    do {
        int dnf1 = 0, dnf2 = 0;
        int npos = option_i(av2, ac2, 'n');
        if (npos != -1 && npos + 1 < argc) {
            /* name specified in argv */
            av2[ac2 - 2] = av2[npos + 1];
            dnf1 = 1;
        } else {
            /* ask */
            av2[ac2 - 2] = malloc(BOT_NAME_SIZE);
            puts("Bot name?");
            printf("> ");
            fflush(stdout);
            fgets(av2[ac2 - 2], 18, stdin);
        }

        int bpos = option_i(av2, ac2, 'b');
        if (bpos != -1 && bpos + 1 < argc) {
            av2[ac2 - 1] = av2[bpos + 1];
            dnf2 = 1;
        } else {
            av2[ac2 - 1] = malloc(PROG_SIZE);
            puts("Build your own tetrominobot!");
            puts("Enter your bot description:");
            printf("> ");
            fflush(stdout);
            fgets(av2[ac2 - 1], PROG_SIZE, stdin);
        }

        /* srand initialized in tbot. fixme */
        tbot_t *t = tbot_new(NULL, av2[ac2 - 2], av2[ac2 - 1],
                             !(option_i(av2, ac2, 'd') == -1));
        game_t *g = new_game(&global_game, av2[ac2 - 2]);
        tbot_run(t, g);

        print_game(g);
        printf("Score: %lu\n", g->score);

        if (bpos != -1) {
            break;
        } else {
            puts("Retry? (y/N)");
            printf("> ");
            fflush(stdout);
            fgets(retry, 3, stdin);
            /* free(g); // free each time? */

            if (!dnf1) {
                free(av2[ac2 - 2]);
                av2[ac2 - 2] = NULL;
            }
            if (!dnf2) {
                free(av2[ac2 - 1]);
                av2[ac2 - 1] = NULL;
            }
        }
    } while (retry[0] == 'y' || retry[0] == 'Y');

    free(av2);
    return 0;
}
