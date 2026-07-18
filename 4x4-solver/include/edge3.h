#ifndef EDGE3_H
#define EDGE3_H

/*
 * Phase 3 — Edge3 coordinate.
 *
 * Goal: pair all 24 wing edges into 12 matching pairs while the Center3 goal
 *       is simultaneously met.
 *
 * The coordinate is a product:
 *
 *   eperm_class — symmetry-reduced permutation of the 12 (already-paired)
 *                 edge slots, range 0..1537  (1538 sym-classes under a
 *                 16-element subgroup of cube symmetry).
 *
 *   eperm_raw   — raw permutation index 0..20159  (= 8!/2 × 7!/2 ... see
 *                 Edge3.java for the exact factoradic encoding of the
 *                 within-group permutation).
 *
 * The combined 1538 × 20160 table would be too large to store naively.
 * The Java implementation uses a 2-bit packed pruning table of size
 *   1538 × 20160 = ~31 million entries (≈ 7.75 MB at 2 bits each).
 *
 * The 2-bit encoding stores (depth mod 3) rather than the absolute depth:
 *   0 → depth ≡ 0 (mod 3) or uninitialised
 *   1 → depth ≡ 1 (mod 3)
 *   2 → depth ≡ 2 (mod 3)
 * A separate array eprun_base[1538] stores the minimum depth in the
 * corresponding sym-class, so the full depth = base + delta.
 *
 * Movement:
 *   The Phase 3 move set has 20 moves (from moves.h, move3std[]).
 *   For each Phase 3 move, the reference Java uses two move tables:
 *     mvrot [20][20]  — move conjugation table (under eperm symmetry group)
 *     mvroto[20][16]  — orientation adjustment for the 16-element sym subgroup
 */

#include <stdint.h>

#define EDGE3_SYM_CLASSES 1538
#define EDGE3_RAW_PERMS   20160
#define EDGE3_PHASE3_MOVES 20
#define EDGE3_SYM_COUNT    16     /* subgroup of cube symmetry */

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */

/* sym2raw[c] = a representative raw permutation index for sym-class c. */
extern int      e3sym2raw[EDGE3_SYM_CLASSES];

/* raw2sym[r] = packed (class * 16 + sym_element). */
extern int      e3raw2sym[EDGE3_RAW_PERMS];

/* Move tables. */
extern uint16_t e3smv  [EDGE3_SYM_CLASSES][EDGE3_PHASE3_MOVES]; /* sym-reduced */
extern uint16_t e3rawmv[EDGE3_RAW_PERMS  ][EDGE3_PHASE3_MOVES]; /* raw (for prun) */

/* 2-bit packed pruning table: eprun[class * RAW_PERMS + raw] packed 4/byte. */
extern uint8_t  eprun[EDGE3_SYM_CLASSES * EDGE3_RAW_PERMS / 4];
/* Per-class minimum depth (to recover absolute depth from 2-bit delta). */
extern uint8_t  eprun_base[EDGE3_SYM_CLASSES];

/* Symmetry conjugation of Phase 3 moves. */
extern uint8_t  mvrot [EDGE3_PHASE3_MOVES][EDGE3_PHASE3_MOVES];
extern uint8_t  mvroto[EDGE3_PHASE3_MOVES][EDGE3_SYM_COUNT];

/* -------------------------------------------------------------------------
 * Coordinate functions
 * ------------------------------------------------------------------------- */

/* Compute the raw Edge3 permutation coordinate from an EdgeCube. */
int edge3_get_raw(const uint8_t ep[24]);

/* Apply phase-3 move p3m to a raw Edge3 coordinate. */
int edge3_raw_move(int raw, int p3m);

/* Apply phase-3 move p3m to a sym-class coordinate. */
int edge3_sym_move(int sym_class, int p3m);

/* Get sym class for a raw coordinate. */
int edge3_getsym_class(int raw);

/* Get sym element within the class for a raw coordinate. */
int edge3_getsym_elem(int raw);

/* -------------------------------------------------------------------------
 * Pruning (2-bit packed with mod-3 encoding)
 * ------------------------------------------------------------------------- */

/* Low-level bit access. */
int  eprun_get(int sym_class, int raw);
void eprun_set(int sym_class, int raw, int val2bit);

/* Higher-level: return the absolute IDA* lower bound. */
int edge3_prun_get(int sym_class, int raw_within_class);

/* -------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */
void edge3_init_sym2raw(void);
void edge3_create_move_tables(void);
void edge3_create_prun(void);
void edge3_init(void);

/* -------------------------------------------------------------------------
 * Solved detection
 * ------------------------------------------------------------------------- */
int edge3_is_solved(int sym_class, int raw);

#endif /* EDGE3_H */
