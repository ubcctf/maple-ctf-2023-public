#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

void read_flag() {
    FILE *fptr;
    char c;

    fptr = fopen("flag.txt", "r");
    if (fptr == NULL) {
        printf("Flag file not found. If this is a local environment, include a flag.txt file in the same folder as your program\n");
        printf("Otherwise, contact admin.\n");
        exit(-1);
    }

    c = fgetc(fptr);
    while (c != EOF)
    {
        printf ("%c", c);
        c = fgetc(fptr);
    }
  
    fclose(fptr);
    return;
}

void play_baby_shark(int* meter) {
    printf("You started playing baby shark...");
    printf("The baby is hypnotized for a few seconds by the colors and starts crying again.\n");
    *meter = *meter + (1 << 2) + 1; 
}


void give_cat_toy(int* meter) {
    printf("You hand the baby a cute squish toy in the shape of a cat...");
    printf("The baby flings it accross the room and starts crying again. \n");

    // A long winded way to add 4 to the meter

    *meter += 100;

    for (int i = 0; i < 4; i++) {
        *meter = *meter - 4;
    }

    int j = 6;
    while(--j) {
        *meter -= 9;
    }

    *meter = *meter - (5 * 3);
    
    int k = 2;
    do {
        *meter -= 7;
        k--;
    } while(k > 0);

    *meter -= 1;
    *meter -= 1;
    *meter -= 1;
    *meter -= 1;
    *meter -= 1;
    *meter -= 1;
}   

void feed_sugar() {
    printf("ARE YOU CRAZY? I can't feed it sugar!\n");
}

void use_baby_food(int* meter) {
    printf("You open a can of pea-flavoured baby food and feed the baby. It eats a bite and continues to cry with tears falling down into its mouth\n");
    *meter += 13;
}

void prepare_milk(int* meter) {
    printf("The baby holds on to the bottle and begins to suck milk...");
    *meter += (495494 % 7);
    printf(" It sighs and continues to cry :(\n");
}



int swaddle_baby(int * sleep_bar) {
    printf("You wrap the baby in a tight and warm blanket...\n");
    *sleep_bar = 3;
    return 1;
}

int read_bedtime_story(int * sleep_bar) {
    if (*sleep_bar != 3) return 0;
    printf("You recite a story about a boy who said he will become king of the pirates... The baby gets tired from the long story\n");
    *sleep_bar *= 7;
    return 1;
}

int rock_the_baby(int * sleep_bar) {
    if (*sleep_bar != 21) return 0;

    printf("You pick the baby up, and start rocking it in your arm. The baby is starting to look relaxed... \n");
    *sleep_bar *= 5;
    return 1;
}


int sing_a_lullaby(int * sleep_bar) {
    if (*sleep_bar != 105) return 0;

    printf("You start singing a fun lullaby that your mother sang to you. The baby is drowsy, but so are you...\n");
    *sleep_bar *= 11;
    return 1;
}


int bigfoot(int * sleep_bar) {
    if (*sleep_bar != 1155) return 0;

    printf("You tell the baby that bigfoot will take it away if it does not stop crying, the baby whimpers and tries to sleep... \n");
    *sleep_bar *= 2;
    return 1;
}

int gauntlet_of_happiness() {
    char actions [30];
    int digit;
    int happiness;

    if (!fgets(actions, sizeof(actions), stdin))
        return -1;
    
    happiness = 0;
    actions[strcspn(actions, "\n")] = '\0';
    for (int i = 0; actions[i] != '\0'; i++) {
        if (!isdigit(actions[i])) {
            printf("The baby did not like that.\n");
            return -1;
        }
        digit = actions[i] - '0';
        switch(digit) {
            case 5:
                play_baby_shark(&happiness);
                break;
            case 9: 
                give_cat_toy(&happiness);
                break;
            default:
                printf("The baby is now confused and is crying even louder...\n");
                return 0;
        }
    }

    if (happiness == 127) {
        return 1;
    }

    return 0;
}


int gauntlet_of_hunger() {
    char actions [6];
    int digit;
    int hunger;
    if (!fgets(actions, sizeof(actions), stdin))
        return -1;
    
    hunger = 0;
    actions[strcspn(actions, "\n")] = '\0';
    for (int i = 0; actions[i] != '\0'; i++) {
        if (!isdigit(actions[i])) {
            printf("The baby did not like that.\n");
            return -1;
        }
        digit = actions[i] - '0';
        switch(digit) {
            case 1:
                feed_sugar();
                return 0;
            case 2:
                use_baby_food(&hunger);
                break;
            case 4:
                prepare_milk(&hunger);
                break;
            default:
                printf("The baby is now confused and is crying even louder...\n");
                return 0;
        }
    }

    // hunger must be congruent to 31 mod 60.
    if ((hunger % 3 == 1) && (hunger % 4 == 3) && (hunger % 5 == 1)) {
        return 1;
    }

    return 0;
}


int gauntlet_of_sleep() {
    char actions [6];
    int digit;
    int sleep;

    if (!fgets(actions, sizeof(actions), stdin))
        return -1;
    
    sleep = 1;

    actions[strcspn(actions, "\n")] = '\0';
    for (int i = 0; actions[i] != '\0'; i++) {
        if (!isdigit(actions[i])) {
            printf("The baby did not like that.\n");
            return -1;
        }
        digit = actions[i] - '0';
        switch(digit) {
            case 0:
                if (!read_bedtime_story(&sleep)) return 0;
                break;
            case 3:
                if (!bigfoot(&sleep)) return 0;
                break;
            case 7:
                if (!sing_a_lullaby(&sleep)) return 0; 
                break;
            case 6:
                if (!rock_the_baby(&sleep)) return 0;
                break;
            case 8:
                if (!swaddle_baby(&sleep)) return 0;
                break;
            default:
                printf("The baby is now confused and is crying even louder...\n");
                return 0;
        }
    }

    if (sleep == 2310) { 
        return 1;
    }

    return 0;
}


void init(void) {
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
}


int main() {
    init();
    printf("This baby does NOT stop crying. Please help I don't know what it wants!\n");
    printf("Maybe we should try to make it happy\n");
    printf("> ");
    if (gauntlet_of_happiness() != 1) return -1;

    printf("I don't think that the baby wants to play...\n");
    printf("Try something else, maybe it's hungry? \n");
    printf("> ");
    if (gauntlet_of_hunger() != 1) return -1;

    printf("Okay, the baby is full. I can probably put it to sleep.\n");
    printf("> ");
    if (gauntlet_of_sleep() != 1) return -1;

    printf("oops you forgot to burp the baby, the baby begins to burp: ");
    read_flag();
    printf(". What a strange choice of a baby's first word you wonder...\n");
    return 0;
}
