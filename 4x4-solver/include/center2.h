#ifndef CENTER2_H
#define CENTER2_H

/*
 * Phase 2 — Center2 coordinate.
 *
 * Goal: fully solve the R/L X-centres (and implicitly the rest, given that
 *       the U/D axis was already fixed in Phase 1).
 *
 * The coordinate is a pair (rl, ct):
 *
 *   rl  — which 4 of the 16 equatorial center slots contain R or L stickers.
 *          Range: C(16,4) = 1820 raw states.  (No sym reduction needed here —
 *          symmetry is handled by the 2×2 block structure of phase 2.)
 *
 *   ct  — permutation of the 16 equatorial center slots, given the rl mask.
 *          Range: 4! × 4! × C(8,4) = 24×24×70 = 40,320 raw states.
 *
 * Combined coord: rl_coord * 40320 + ct_coord → total 1820 × 40320 is NOT
 * used directly. The Java solver stores them as two separate look-up tables:
 *   rlmv [1820][28]   — move table for the rl sub-coord (Phase 2 move set)
 *   ctmv [6435][28]   — move table for ct sub-coord (6435 = C(16,4)*some sym)
 *   ctprun[1820][6435] — pruning table (product coordinate)
 *
 * NOTE: 6435 = C(16,4) is used directly for ctmv; the combined prun table
 * stores 1 byte per entry for a total of ~11.7 MB.
 */

#include <stdint.h>

#define CENTER2_RL_COORDS   1820
#define CENTER2_CT_COORDS   6435   /* C(16,4) */
#define CENTER2_PHASE2_MOVES 28

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */
extern uint16_t rlmv  [CENTER2_RL_COORDS][CENTER2_PHASE2_MOVES];
extern uint16_t ctmv  [CENTER2_CT_COORDS][CENTER2_PHASE2_MOVES];
extern uint8_t  ctprun[CENTER2_RL_COORDS][CENTER2_CT_COORDS];

/* -------------------------------------------------------------------------
 * Coordinate functions
 *
 * Both rl and ct operate on a uint8_t center[16] array representing only the
 * 16 equatorial center slots (R and L faces, 8 slots each).
 * Colors: 0=U, 1=R, 2=F, 3=D, 4=L, 5=B.
 * ------------------------------------------------------------------------- */

/* Which 4 of the 16 equatorial slots hold R or L stickers (combinatorial). */
int center2_get_rl (const uint8_t eq_center[16]);

/* Positional rank of the 4 R-color and 4 L-color stickers independently. */
int center2_get_ct (const uint8_t eq_center[16]);

/* Apply phase-2 move p2m (0..27) to an rl coordinate. */
int center2_rl_move(int rl, int p2m);

/* Apply phase-2 move p2m (0..27) to a ct coordinate. */
int center2_ct_move(int ct, int p2m);

/* -------------------------------------------------------------------------
 * Pruning
 * ------------------------------------------------------------------------- */
int center2_prun_get(int rl, int ct);
void center2_prun_set(int rl, int ct, int dist);

/* -------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */
void center2_create_rl_move_table(void);
void center2_create_ct_move_table(void);
void center2_create_prun(void);
void center2_init(void);

/* -------------------------------------------------------------------------
 * Solved detection
 * ------------------------------------------------------------------------- */
int center2_is_solved(int rl, int ct);

#endif /* CENTER2_H */
