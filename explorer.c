#include "alg.h"
#include "cube4.h"
#include "search.h"
#include "cubie.h"

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
 * Unfolded net -- each cell is 3 terminal columns wide:
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

/* TPR move axis (0-11) -> explorer Face enum */
static const uint8_t tpr_axis_face[12] = {
    FACE_U, FACE_R, FACE_F, FACE_D, FACE_L, FACE_B,  /* outer 0-5 */
    FACE_U, FACE_R, FACE_F, FACE_D, FACE_L, FACE_B,  /* wide  6-11 */
};

/* Build an explorer Alg from a TPR move buffer (caller must alg_free). */
static Alg tpr_buf_to_alg(const uint8_t *buf, int len) {
    Alg a = { .m = malloc((size_t)len * sizeof(Move)), .len = len, .cap = len };
    for (int i = 0; i < len; i++) {
        int axis = buf[i] / 3, power = buf[i] % 3;
        a.m[i] = (Move){
            .face  = tpr_axis_face[axis],
            .q     = (uint8_t)(power + 1),      /* TPR 0=CW,1=180,2=CCW -> q 1,2,3 */
            .depth = (uint8_t)(axis >= 6 ? 2 : 1),
        };
    }
    return a;
}

static void do_solve(CubeState4 *cube) {
    static int tpr_ready;
    if (!tpr_ready) {
        printf("Initialising TPR tables (first use only)...\n");
        fflush(stdout);
        tpr_init();
        tpr_ready = 1;
    }

    char facelet96[97];
    cube4_to_facelet96(cube, facelet96);

    FullCube state;
    fullcube_from_facelet(&state, facelet96);

    static FullCube beam1[SEARCH_BEAM1_MAX];
    static FullCube beam2[SEARCH_BEAM2_MAX];
    static FullCube p3result;

    /* --- Phase 1 --- */
    printf("  [P1] "); fflush(stdout);
    int n1 = search1(&state, beam1, SEARCH_BEAM1_MAX);
    if (n1 > 0) {
        Alg a = tpr_buf_to_alg(beam1[0].moveBuffer, beam1[0].moveLength);
        char *str = alg_to_string(&a);
        printf("%d moves: %-40s  (%d candidates)\n",
               beam1[0].moveLength, str ? str : "?", n1);
        free(str);
        alg_free(&a);
    } else {
        printf("no solution found\n");
    }

    /* --- Phase 2 --- */
    printf("  [P2] "); fflush(stdout);
    int n2 = search2(beam1, n1, beam2, SEARCH_BEAM2_MAX);
    if (n2 > 0) {
        Alg a2 = tpr_buf_to_alg(beam2[0].moveBuffer + beam2[0].length1,
                                  beam2[0].length2);
        char *str2 = alg_to_string(&a2);
        printf("%d moves: %-40s  (%d candidates)\n",
               beam2[0].length2, str2 ? str2 : "?", n2);
        free(str2);
        alg_free(&a2);
    } else {
        printf("no solution\n");
    }

    /* --- Phase 3 --- */
    printf("  [P3] "); fflush(stdout);
    int n3 = n2 > 0 ? search3(beam2, n2, &p3result) : 0;
    if (n3 > 0) {
        Alg a = tpr_buf_to_alg(p3result.moveBuffer, p3result.moveLength);
        char *str = alg_to_string(&a);
        printf("solved! %d moves total: %s\n", p3result.moveLength, str ? str : "?");
        free(str);
        alg_free(&a);
    } else {
        printf("no solution found\n");
    }

    /* Apply the deepest completed phase and display the resulting cube state */
    if (n3 > 0) {
        printf("\nApplying full solution (%d moves):\n", p3result.moveLength);
        Alg a = tpr_buf_to_alg(p3result.moveBuffer, p3result.moveLength);
        cube4_apply_sequence(cube, &a);
        alg_free(&a);
        print_cube(cube);
    } else if (n2 > 0) {
        printf("\nApplying P1+P2 solution (%d moves):\n", beam2[0].moveLength);
        Alg a = tpr_buf_to_alg(beam2[0].moveBuffer, beam2[0].moveLength);
        cube4_apply_sequence(cube, &a);
        alg_free(&a);
        print_cube(cube);
    } else if (n1 > 0) {
        printf("\nApplying P1 solution (%d moves):\n", beam1[0].moveLength);
        Alg a = tpr_buf_to_alg(beam1[0].moveBuffer, beam1[0].moveLength);
        cube4_apply_sequence(cube, &a);
        alg_free(&a);
        print_cube(cube);
    }
}

int main(void) {
    cube4_init();

    CubeState4 cube;
    cube4_identity(&cube);

    printf("4x4 Cube Explorer\n");
    printf("Moves : U D L R F B (outer)  |  Uw Dw Lw Rw Fw Bw (wide / 2-layer)\n");
    printf("        append ' for inverse, 2 for half-turn -- e.g.  R U' Fw2  (Uw R')3\n");
    printf("Commands: reset/r -- solved state  solve/s -- run TPR pipeline  q -- quit\n\n");
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
            do_solve(&cube);
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
