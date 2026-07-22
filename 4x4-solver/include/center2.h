#ifndef CENTER2_H
#define CENTER2_H

/*
 * Phase 2 — Center2 coordinate.
 *
 * Goal: fully solve the R/L centers while preserving the U/D axis from Phase 1.
 *
 * State is a pair (rl, ct) derived from CenterCube + edge parity:
 *
 *   rl  — C(7,4) × 2 = 70 values.
 *          Tracks which 4 of the 8 R/L center slots (positions 16–23) hold a
 *          different color from slot 23.  The ×2 factor encodes edge parity.
 *          Encoding: C(7,4) rank of slots 16–22 that differ from slot 23,
 *          times 2, plus parity bit.
 *
 *   ct  — C(15,8) = 6435 values.
 *          Tracks the axis-class distribution across the 16 non-R/L center
 *          positions (slots 0–15).  Each slot's color is reduced mod 3
 *          (0=UD-axis, 1=RL-axis, 2=FB-axis), then C(15,8) ranks those among
 *          0–14 that differ from slot 15.
 *
 * Tables (from Java Center2.java):
 *   rlmv [70][28]       — rl coordinate after each phase-2 move
 *   ctmv [6435][28]     — ct coordinate after each phase-2 move
 *   ctprun[6435 * 70]   — flat pruning table, indexed ct * 70 + rl
 *
 * Memory: 70*28 + 6435*28*2 + 450450 ≈ 812 KB.
 */

#include <stdint.h>

#define CENTER2_RL_COORDS    70       /* C(7,4) * 2                  */
#define CENTER2_CT_COORDS    6435     /* C(15,8)                     */
#define CENTER2_PHASE2_MOVES 28
#define CENTER2_PRUN_SIZE    (CENTER2_CT_COORDS * CENTER2_RL_COORDS)  /* 450450 */

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */
extern uint8_t  rlmv  [CENTER2_RL_COORDS][CENTER2_PHASE2_MOVES];
extern uint16_t ctmv  [CENTER2_CT_COORDS][CENTER2_PHASE2_MOVES];
extern uint8_t  ctprun[CENTER2_PRUN_SIZE];   /* indexed: ct * 70 + rl */

/* -------------------------------------------------------------------------
 * Center2 simulation state
 *
 * rl[8]   : actual color values from CenterCube.ct[16..23] (R/L face slots).
 *            In setrl-decoded states, values are binary (0 or 1).
 * ct[16]  : CenterCube.ct[0..15] reduced mod 3 (axis-class: 0=UD, 1=RL, 2=FB).
 *            In setct-decoded states, values are binary (0 or 1).
 * parity  : edge permutation parity bit, toggled by Rw1, Rw', Lw1, Lw'.
 * ------------------------------------------------------------------------- */
typedef struct {
    int rl[8];
    int ct[16];
    int parity;
} Center2State;

/* Populate from a CenterCube's ct[24] array and the current edge parity. */
void center2_set(Center2State *s, const uint8_t cc_ct[24], int edge_parity);

/* rl coordinate: C(7,4) rank of rl[0..6] differing from rl[7], *2 + parity. */
int  center2_getrl(const Center2State *s);
void center2_setrl(Center2State *s, int idx);

/* ct coordinate: C(15,8) rank of ct[0..14] differing from ct[15]. */
int  center2_getct(const Center2State *s);
void center2_setct(Center2State *s, int idx);

/* Apply standard move m (0..35) to a Center2State (simulation kernel). */
void center2_move(Center2State *s, int m);

/* -------------------------------------------------------------------------
 * Move table accessors
 * ------------------------------------------------------------------------- */
int center2_rl_move(int rl, int p2m);
int center2_ct_move(int ct, int p2m);

/* -------------------------------------------------------------------------
 * Pruning  (indexed ct * CENTER2_RL_COORDS + rl)
 * ------------------------------------------------------------------------- */
int  center2_prun_get(int rl, int ct);
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
