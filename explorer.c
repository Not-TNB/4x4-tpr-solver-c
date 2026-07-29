#include "alg.h"
#include "cube4.h"
#include "4x4-solver/include/search.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double elapsed_ms(struct timespec t0, struct timespec t1) {
    return (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) * 1e-6;
}

/* ANSI background colour per face (U D L R F B) */
static const char *ANSI_BG[] = {
    "\033[47;30m", /* U white  */
    "\033[43;30m", /* D yellow */
    "\033[45;37m", /* L orange */
    "\033[41;37m", /* R red    */
    "\033[42;30m", /* F green  */
    "\033[44;37m", /* B blue   */
};
static const char FACE_CH[] = { 'U', 'D', 'L', 'R', 'F', 'B' };
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
 * Unfolded net -- each cell is 3 terminal columns wide:
 *
 *            U U U U
 *            U U U U
 *            U U U U
 *            U U U U
 *  L L L L   F F F F   R R R R   B B B B
 *  L L L L   F F F F   R R R R   B B B B
 *  L L L L   F F F F   R R R R   B B B B
 *  L L L L   F F F F   R R R R   B B B B
 *            D D D D
 *            D D D D
 *            D D D D
 *            D D D D
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

/* -------------------------------------------------------------------------
 * Solver integration
 *
 * CubeState4 facelet ordering: U(0-15) D(16-31) L(32-47) R(48-63) F(64-79) B(80-95)
 * facelet96 ordering expected by TPR solver: U(0-15) R(16-31) F(32-47) D(48-63) L(64-79) B(80-95)
 * CubeState4 colour: FACE_U=0 FACE_D=1 FACE_L=2 FACE_R=3 FACE_F=4 FACE_B=5
 * facelet96 chars:  'U'      'D'      'L'      'R'      'F'      'B'
 * ------------------------------------------------------------------------- */

/* facelet96 face index (0=U,1=R,2=F,3=D,4=L,5=B) -> CubeState4 slot base */
static const int cs4_base_for_f96[6] = { 0, 48, 64, 16, 32, 80 };

/* CubeState4 FACE_* colour -> facelet96 character */
static const char cs4_color_ch[6] = { 'U', 'D', 'L', 'R', 'F', 'B' };

static void cube4_to_facelet96(const CubeState4 *s, char buf[97]) {
    for (int fi = 0; fi < 6; fi++) {
        int base = cs4_base_for_f96[fi];
        for (int local = 0; local < 16; local++)
            buf[fi * 16 + local] = cs4_color_ch[sticker_color(s, base + local)];
    }
    buf[96] = '\0';
}


static void do_solve(CubeState4 *cube, bool orient) {
    static int tpr_ready;
    if (!tpr_ready) {
        tpr_set_kok_path("4x4-solver/ckociemba/cprunetables");
        printf("Initialising TPR tables (first use only)...\n");
        fflush(stdout);
        struct timespec ti0, ti1;
        clock_gettime(CLOCK_MONOTONIC, &ti0);
        tpr_init();
        clock_gettime(CLOCK_MONOTONIC, &ti1);
        printf("  tables ready in %.1f ms\n", elapsed_ms(ti0, ti1));
        tpr_ready = 1;
    }

    char facelet96[97];
    cube4_to_facelet96(cube, facelet96);

    char sol_buf[512];
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int n_tpr = tpr_solve(facelet96, sol_buf, sizeof(sol_buf), orient);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    if (n_tpr < 0) {
        printf("  no solution found\n");
        return;
    }

    double total_ms = elapsed_ms(t0, t1);
    printf("  P1: %.1f ms (%d cands)  P2: %.1f ms  P3: %.1f ms (%lld nodes, bound %d)\n",
           tpr_diag.p1_ms, tpr_diag.n1,
           tpr_diag.p2_ms,
           tpr_diag.p3_ms, tpr_diag.p3_nodes, tpr_diag.p3_bound);

    Alg a = {0};
    alg_parse(sol_buf, &a);
    char *sol_str = alg_to_string(&a);
    printf("\n  Solution (%d moves, %.1f ms): %s\n", n_tpr, total_ms,
           sol_str ? sol_str : sol_buf);
    free(sol_str);

    cube4_apply_sequence(cube, &a);
    alg_free(&a);

    printf("\n  Final:\n");
    print_cube(cube);
}

int main(void) {
    cube4_init();

    CubeState4 cube;
    cube4_identity(&cube);

    printf("4x4 Cube Explorer\n");
    printf("Moves : U D L R F B (outer)  |  Uw Dw Lw Rw Fw Bw (wide / 2-layer)\n");
    printf("        append ' for inverse, 2 for half-turn -- e.g.  R U' Fw2  (Uw R')3\n");
    printf("Commands: reset/r -- solved state | solve/s -- solve (ends white-top/green-front) | solve raw/sr -- solve without reorientation | facelet/f -- print facelet string | q -- quit\n\n");
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

        if (strcmp(p, "solve") == 0 || strcmp(p, "s") == 0) {
            printf("Solving...\n");
            do_solve(&cube, true);
            continue;
        }

        if (strcmp(p, "solve raw") == 0 || strcmp(p, "sr") == 0) {
            printf("Solving (no reorientation)...\n");
            do_solve(&cube, false);
            continue;
        }

        if (strcmp(p, "facelet") == 0 || strcmp(p, "f") == 0) {
            char facelet96[97];
            cube4_to_facelet96(&cube, facelet96);
            printf("Facelet: %s\n", facelet96);
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
