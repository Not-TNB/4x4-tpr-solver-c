#include "../include/center3.h"
#include "../include/tpr_util.h"
#include "../include/moves.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */
uint8_t ctmv3 [CENTER3_SLICE_COORDS][CENTER3_PHASE3_MOVES];
uint8_t c3prun[CENTER3_PRUN_SIZE];

/* -------------------------------------------------------------------------
 * Slice coordinate helpers
 *
 * Each slice sub-coordinate is C(8,4)=70: which 4 of 8 positions hold a
 * given color, encoded via the combinatorial number system.
 *
 * The 8 slots per axis (after Phase 2 constraints) are the 4 solved slots
 * on the face plus the 4 equatorial slots for that axis.
 * ------------------------------------------------------------------------- */

static int choose4of8(const uint8_t slots[8], int color) {
    int rank = 0, chosen = 0;
    for (int i = 7; i >= 0 && chosen < 4; i--) {
        if (slots[i] == (uint8_t)color) {
            rank += Cnk[i][4 - chosen];
            chosen++;
        }
    }
    return rank;
}

int center3_get_slice_u(const uint8_t ct[24]) {
    /* U-face centers (0-3) and the 4 D-face positions (4-7). */
    uint8_t slots[8];
    for (int i = 0; i < 8; i++) slots[i] = ct[i]; /* positions 0-7 */
    return choose4of8(slots, 0); /* U=0 */
}

int center3_get_slice_r(const uint8_t ct[24]) {
    /* R/L equatorial positions: 16-23. */
    uint8_t slots[8];
    for (int i = 0; i < 8; i++) slots[i] = ct[16 + i];
    return choose4of8(slots, 1); /* R=1 */
}

int center3_get_slice_f(const uint8_t ct[24]) {
    /* F/B equatorial positions: 8-15. */
    uint8_t slots[8];
    for (int i = 0; i < 8; i++) slots[i] = ct[8 + i];
    return choose4of8(slots, 2); /* F=2 */
}

int center3_slice_move(int slice, int p3m) {
    return ctmv3[slice][p3m];
}

/* -------------------------------------------------------------------------
 * Pruning coordinate packing
 *
 * Packed formula from the Java Center3.java:
 *   pr = (sliceR/2)*35*12*2 + (sliceF/2)*12*2 + (sliceU/4)*2 + eparity
 * This reduces 70×70×70×2 to 35×35×12×2 = 29,400 entries by halving each
 * dimension (exploiting the symmetry that only even indices are reachable).
 * The division truncates — values must be even/divisible by the given factor.
 * ------------------------------------------------------------------------- */

int center3_prun_coord(int sliceU, int sliceR, int sliceF, int eparity) {
    return (sliceR/2)*35*12*2 + (sliceF/2)*12*2 + (sliceU/4)*2 + eparity;
}

int center3_prun_get(int coord) {
    return c3prun[coord];
}

void center3_prun_set(int coord, int dist) {
    c3prun[coord] = (uint8_t)dist;
}

/* -------------------------------------------------------------------------
 * Slice move table: same 70-entry table reused for all 3 slice types.
 * ------------------------------------------------------------------------- */
void center3_create_slice_move_table(void) {
    /* TODO: for each slice coord 0..69 and each phase-3 move 0..19,
     * compute the resulting slice coord using the combinatorial logic. */
    memset(ctmv3, 0, sizeof(ctmv3));
}

void center3_create_prun(void) {
    memset(c3prun, 0xFF, sizeof(c3prun));
    /* TODO: BFS from solved state (sliceU=0, sliceR=0, sliceF=0, eparity=0).
     * Iterate over all 4 sub-coordinates jointly using ctmv3 and parity. */
}

void center3_init(void) {
    center3_create_slice_move_table();
    center3_create_prun();
}

int center3_is_solved(int sliceU, int sliceR, int sliceF) {
    return (sliceU == 0 && sliceR == 0 && sliceF == 0);
}
