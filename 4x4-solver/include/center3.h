#ifndef CENTER3_H
#define CENTER3_H

/*
 * Phase 3 — Center3 coordinate.
 *
 * Goal: fully solve all 24 X-centres (all centers solved) while simultaneously
 *       pairing all 24 wing edges (Edge3 sub-goal).
 *
 * The Center3 state space is a product of three independent sub-coordinates:
 *
 *   sliceU  — arrangement of the 4 U-center stickers within the 8 slots on
 *              the U and D faces that were already constrained by Phase 2.
 *              Range: C(8,4) = 70.
 *
 *   sliceR  — same for the 4 R-center stickers within the 8 R/L equatorial
 *              slots.  Range: C(8,4) = 70.
 *
 *   sliceF  — same for the 4 F-center stickers within the 8 F/B equatorial
 *              slots.  Range: C(8,4) = 70.
 *
 *   eperm   — corner/edge parity parity bit (1 bit) for even permutation
 *              constraint imposed by Phase 2 reductions.
 *
 * Combined: 70 × 70 × 70 × 2 = 686,000 states (from which parity halves it
 * to 343,000 reachable). But the Java TPR stores the full 70×70×70×2 product
 * without further reduction.
 *
 * Tables:
 *   ctmv3[70][20]     — move table for each slice coordinate (same for all 3,
 *                        since all slices use the same C(8,4) combinatorics).
 *   c3prun[29400]     — pruning table; 29400 = 35×35×12×2 (packed sym classes
 *                        of the product; see Java Center3.java for packing).
 *
 * NOTE: The 20-move Phase 3 move set is defined in moves.h (move3std[]).
 */

#include <stdint.h>

#define CENTER3_SLICE_COORDS  70    /* C(8,4)                        */
#define CENTER3_EPERM_COORDS  2     /* parity bit                    */
#define CENTER3_PRUN_SIZE  29400    /* = 35 × 35 × 12 × 2           */
#define CENTER3_PHASE3_MOVES 20

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */
extern uint8_t  ctmv3 [CENTER3_SLICE_COORDS][CENTER3_PHASE3_MOVES];
extern uint8_t  c3prun[CENTER3_PRUN_SIZE];

/* -------------------------------------------------------------------------
 * Coordinate functions
 *
 * Each "slice" sub-coordinate is an index in 0..69 encoding which 4 of the
 * 8 relevant center slots contain a given face color.
 * The 8 relevant slots per axis are fixed after Phase 2.
 * ------------------------------------------------------------------------- */

/* Get sliceU coord from the current CenterCube ct[24]. */
int center3_get_slice_u(const uint8_t ct[24]);
int center3_get_slice_r(const uint8_t ct[24]);
int center3_get_slice_f(const uint8_t ct[24]);

/* Apply phase-3 move p3m (0..19) to a slice coordinate. */
int center3_slice_move(int slice, int p3m);

/* -------------------------------------------------------------------------
 * Pruning
 *
 * The prun coord is computed from sliceU, sliceR, sliceF, and eparity.
 * The packing formula matches the Java implementation:
 *   packed = (sliceR/2)*35*12*2 + (sliceF/2)*12*2 + (sliceU/4)*2 + eparity
 * (integer divisions, so 70/2=35, 70/4 needs rounding — see center3.c).
 * ------------------------------------------------------------------------- */
int center3_prun_coord(int sliceU, int sliceR, int sliceF, int eparity);
int center3_prun_get(int coord);
void center3_prun_set(int coord, int dist);

/* -------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */
void center3_create_slice_move_table(void);
void center3_create_prun(void);
void center3_init(void);

/* -------------------------------------------------------------------------
 * Solved detection
 * ------------------------------------------------------------------------- */
int center3_is_solved(int sliceU, int sliceR, int sliceF);

#endif /* CENTER3_H */
