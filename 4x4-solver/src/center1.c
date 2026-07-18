#include "../include/center1.h"
#include "../include/cubie.h"
#include "../include/tpr_util.h"
#include "../include/moves.h"
#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------
 * Tables (allocated in center1_init)
 * ------------------------------------------------------------------------- */
uint16_t ctsmv  [CENTER1_SYM_CLASSES][36];
int      sym2raw [CENTER1_SYM_CLASSES];
int      raw2sym [CENTER1_RAW_COORDS];
uint8_t  csprun  [CENTER1_SYM_CLASSES];

uint8_t  symmult [CENTER1_SYM_COUNT][CENTER1_SYM_COUNT];
uint8_t  symmove [CENTER1_SYM_COUNT][36];
uint8_t  syminv  [CENTER1_SYM_COUNT];
uint8_t  finish  [CENTER1_SYM_COUNT];

/* -------------------------------------------------------------------------
 * Raw coordinate: C(24,8) — which 8 of the 24 center positions hold a
 * U or D color sticker.
 *
 * Encoding: combinatorial number system (Gosper's hack), scanning positions
 * 23..0 and choosing the 8 set bits.
 * ------------------------------------------------------------------------- */

int center1_get(const uint8_t ct[24]) {
    int rank = 0, chosen = 0;
    for (int i = 23; i >= 0 && chosen < 8; i--) {
        int color = ct[i];
        if (color == 0 || color == 3) { /* U=0 or D=3 */
            rank += Cnk[i][8 - chosen];
            chosen++;
        }
    }
    return rank;
}

int center1_move_raw(int raw, int m) {
    /* TODO: apply move m to raw coord (decode → apply → re-encode). */
    (void)raw; (void)m;
    return 0;
}

int center1_move_sym(int sym_class, int m) {
    return ctsmv[sym_class][m];
}

int center1_getsym(int raw) {
    return raw2sym[raw] & 0x1F;
}

int center1_rotate(int raw, int s) {
    /* TODO: rotate a raw coord by symmetry element s. */
    (void)raw; (void)s;
    return 0;
}

int center1_prun_get(int sym_class) {
    return csprun[sym_class];
}

void center1_prun_set(int sym_class, int dist) {
    csprun[sym_class] = (uint8_t)dist;
}

/* -------------------------------------------------------------------------
 * Symmetry initialisation
 *
 * The 48-element symmetry group of the cube acts on center positions.
 * We represent each element as a permutation of the 24 center slots.
 * TODO: fill out the actual permutation arrays from the Java source.
 * ------------------------------------------------------------------------- */
void center1_init_sym(void) {
    /* TODO: build symmult, symmove, syminv, finish. */
    memset(symmult, 0, sizeof(symmult));
    memset(symmove, 0, sizeof(symmove));
    memset(syminv,  0, sizeof(syminv));
    memset(finish,  0, sizeof(finish));
}

/* -------------------------------------------------------------------------
 * BFS to build sym2raw and raw2sym
 * ------------------------------------------------------------------------- */
void center1_init_sym2raw(void) {
    /* TODO: BFS over the raw coordinate space, canonicalising each
     * state into its minimum representative under the symmetry group.
     * Populate sym2raw[] and raw2sym[]. */
    memset(sym2raw, -1, sizeof(sym2raw));
    memset(raw2sym, -1, sizeof(raw2sym));
}

/* -------------------------------------------------------------------------
 * Move table
 * ------------------------------------------------------------------------- */
void center1_create_move_table(void) {
    /* TODO: for each sym-class and each of 36 moves, compute the resulting
     * sym-class and store in ctsmv[class][move]. */
    memset(ctsmv, 0, sizeof(ctsmv));
}

/* -------------------------------------------------------------------------
 * Pruning table (BFS from solved state)
 * ------------------------------------------------------------------------- */
void center1_create_prun(void) {
    memset(csprun, 0xFF, sizeof(csprun));
    /* TODO: BFS from all solved sym-classes; set csprun[c] = depth. */
}

void center1_init(void) {
    center1_init_sym();
    center1_init_sym2raw();
    center1_create_move_table();
    center1_create_prun();
}

int center1_is_solved(int sym_class) {
    /* A state is solved when csprun == 0. */
    return csprun[sym_class] == 0;
}
