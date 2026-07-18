#include "alg.h"
#include "cube4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ANSI background colour per face (U D L R F B) */
static const char *ANSI_BG[] = {
    "\033[47;30m", /* U white  */
    "\033[43;30m", /* D yellow */
    "\033[45;37m", /* L orange */
    "\033[41;37m", /* R red    */
    "\033[42;30m", /* F green  */
    "\033[44;37m", /* B blue   */
};
static const char FACE_CH[] = { 'W', 'Y', 'O', 'R', 'G', 'B' };
#define RST "\033[0m"

/* Face offsets match the Face enum × 16 (U=0,D=16,L=32,R=48,F=64,B=80). */
static int sticker_color(const CubeState4 *s, int slot) {
    return (int)s->state[slot] / 16;
}

static void print_sticker(const CubeState4 *s, int slot) {
    int f = sticker_color(s, slot);
    printf("%s %c " RST, ANSI_BG[f], FACE_CH[f]);
}

/* Print one row (4 stickers) of a face given its base slot offset. */
static void print_face_row(const CubeState4 *s, int base, int row) {
    for (int c = 0; c < 4; c++)
        print_sticker(s, base + row * 4 + c);
}

/*
 * Unfolded net — each cell is 3 terminal columns wide:
 *
 *             U U U U
 *             U U U U
 *             U U U U
 *             U U U U
 *  L L L L   F F F F   R R R R   B B B B
 *  L L L L   F F F F   R R R R   B B B B
 *  L L L L   F F F F   R R R R   B B B B
 *  L L L L   F F F F   R R R R   B B B B
 *             D D D D
 *             D D D D
 *             D D D D
 *             D D D D
 *
 * L occupies 4 stickers = 12 chars, so U/D are indented by 12.
 */
static void print_cube(const CubeState4 *s) {
    for (int r = 0; r < 4; r++) {
        printf("            ");    /* 12-char indent: aligns U above F */
        print_face_row(s,  0, r); /* U: base  0 */
        putchar('\n');
    }
    for (int r = 0; r < 4; r++) {
        print_face_row(s, 32, r); /* L: base 32 */
        print_face_row(s, 64, r); /* F: base 64 */
        print_face_row(s, 48, r); /* R: base 48 */
        print_face_row(s, 80, r); /* B: base 80 */
        putchar('\n');
    }
    for (int r = 0; r < 4; r++) {
        printf("            ");
        print_face_row(s, 16, r); /* D: base 16 */
        putchar('\n');
    }
}

int main(void) {
    cube4_init();

    CubeState4 cube;
    cube4_identity(&cube);

    printf("4x4 Cube Explorer\n");
    printf("Moves : U D L R F B (outer)  |  Uw Dw Lw Rw Fw Bw (wide / 2-layer)\n");
    printf("        append ' for inverse, 2 for half-turn — e.g.  R U' Fw2  (Uw R')3\n");
    printf("Commands: reset / r — solved state    q — quit\n\n");
    print_cube(&cube);

    char line[1024];
    while (1) {
        printf("\n> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = '\0';

        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') continue;

        if ((p[0] == 'q' && p[1] == '\0') ||
            strcmp(p, "quit") == 0 || strcmp(p, "exit") == 0)
            break;

        if (strcmp(p, "reset") == 0 || strcmp(p, "r") == 0) {
            cube4_identity(&cube);
            printf("Cube reset to solved.\n");
            print_cube(&cube);
            continue;
        }

        Alg alg = {0};
        if (!alg_parse(p, &alg)) {
            fprintf(stderr, "parse error: \"%s\"\n", p);
            continue;
        }

        cube4_apply_sequence(&cube, &alg);

        char *str = alg_to_string(&alg);
        printf("Applied: %s\n", str ? str : p);
        free(str);
        alg_free(&alg);

        print_cube(&cube);
    }

    return 0;
}
