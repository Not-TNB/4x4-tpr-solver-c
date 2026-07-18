#include "../include/center2.h"
#include "../include/tpr_util.h"
#include "../include/moves.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */
uint16_t rlmv  [CENTER2_RL_COORDS][CENTER2_PHASE2_MOVES];
uint16_t ctmv  [CENTER2_CT_COORDS][CENTER2_PHASE2_MOVES];
uint8_t  ctprun[CENTER2_RL_COORDS][CENTER2_CT_COORDS];

/* -------------------------------------------------------------------------
 * rl coordinate
 *
 * Which 4 of the 16 equatorial center slots contain R or L stickers.
 * Encoded as C(16,4) = 1820 via the combinatorial number system.
 * "Equatorial" = F(8-11) B(12-15) R(16-19) L(20-23) center slots.
 * R=1, L=4; any of the 16 slots holding those colors contributes.
 * ------------------------------------------------------------------------- */

int center2_get_rl(const uint8_t eq_center[16]) {
    int rank = 0, chosen = 0;
    for (int i = 15; i >= 0 && chosen < 4; i--) {
        int color = eq_center[i];
        if (color == 1 || color == 4) { /* R=1, L=4 */
            rank += Cnk[i][4 - chosen];
            chosen++;
        }
    }
    return rank;
}

/* -------------------------------------------------------------------------
 * ct coordinate
 *
 * Given the rl mask, independently encode the positions of the 4 R stickers
 * (within the 8 R/L slots) and the 4 L stickers similarly.
 * Range: C(8,4) × C(8,4) = 70 × 70 … but the Java solver encodes both into
 * a single 6435-range value. We follow that encoding:
 *   ct = r_rank * 70 + l_rank, where each rank is C(8,4) = 70 options.
 *   NOTE: 6435 = C(16,4) which equals 70^2/something — the actual encoding
 *   used by the Java source is the combinatorial rank of 8 chosen from 16 for
 *   the R-color positions only, with the L positions determined by exclusion.
 *   This matches CENTER2_CT_COORDS = 6435 = C(16,4).
 * ------------------------------------------------------------------------- */

int center2_get_ct(const uint8_t eq_center[16]) {
    /* Rank of R-color positions among all 16 equatorial slots. */
    int rank = 0, chosen = 0;
    for (int i = 15; i >= 0 && chosen < 8; i--) {
        if (eq_center[i] == 1) { /* R=1 */
            rank += Cnk[i][8 - chosen];
            chosen++;
        }
    }
    return rank;
}

int center2_rl_move(int rl, int p2m) {
    return rlmv[rl][p2m];
}

int center2_ct_move(int ct, int p2m) {
    return ctmv[ct][p2m];
}

int center2_prun_get(int rl, int ct) {
    return ctprun[rl][ct];
}

void center2_prun_set(int rl, int ct, int dist) {
    ctprun[rl][ct] = (uint8_t)dist;
}

void center2_create_rl_move_table(void) {
    /* TODO: for each rl coord and each phase-2 move, compute new rl. */
    memset(rlmv, 0, sizeof(rlmv));
}

void center2_create_ct_move_table(void) {
    /* TODO: for each ct coord and each phase-2 move, compute new ct. */
    memset(ctmv, 0, sizeof(ctmv));
}

void center2_create_prun(void) {
    memset(ctprun, 0xFF, sizeof(ctprun));
    /* TODO: BFS from solved state (rl=0, ct=0 in canonical form).
     * Use center2_rl_move and center2_ct_move in tandem. */
}

void center2_init(void) {
    center2_create_rl_move_table();
    center2_create_ct_move_table();
    center2_create_prun();
}

int center2_is_solved(int rl, int ct) {
    /* Solved when both sub-coords are 0 (canonical solved value). */
    return (rl == 0 && ct == 0);
}
