#ifndef SEARCH_H
#define SEARCH_H

/*
 * Top-level search orchestration for the TPR solver.
 *
 * The solve pipeline has four stages:
 *
 *   Phase 1 (search1): IDA* over Center1 sym-coord.
 *             Generates up to 500 solutions. Each is a FullCube with
 *             the U/D axis oriented and moves recorded in moveBuffer.
 *
 *   Phase 2 (search2): Beam search over best Phase-1 results.
 *             For each, runs IDA* over (Center2 rl × Center2 ct) coord.
 *             Keeps top 100 solutions by total move count.
 *
 *   Phase 3 (search3): IDA* over (Center3 × Edge3) product coordinate.
 *             For each Phase-2 result, attempts to pair edges + finish
 *             centres simultaneously. Generates one Phase-3 solution.
 *
 *   Phase 4 (solve333): The accumulated move sequence fully reduces the
 *             4×4 to an equivalent 3×3. Call min2phase to finish.
 *
 * Beam capacities are hardcoded constants.
 */

#include "cubie.h"
#include <stdint.h>

#define SEARCH_BEAM1_MAX  500
#define SEARCH_BEAM2_MAX  100

/* -------------------------------------------------------------------------
 * Phase search functions
 *
 * All three take a FullCube pointer. Phase 1 uses the input state;
 * Phases 2 and 3 are called on Phase-1/Phase-2 output FullCubes.
 *
 * Returns the number of solutions found (Phase 1/2), or 0/1 for Phase 3.
 * ------------------------------------------------------------------------- */

/*
 * Run Phase 1 IDA* from *state.
 * Writes up to SEARCH_BEAM1_MAX solutions into out[] (caller-allocated).
 * Returns count of solutions found.
 */
int search1(const FullCube *state,
            FullCube       *out,
            int             max_out);

/*
 * Run Phase 2 IDA* from each Phase-1 result in p1[0..n1).
 * Writes up to SEARCH_BEAM2_MAX solutions into out[].
 * Returns count.
 */
int search2(const FullCube *p1,
            int             n1,
            FullCube       *out,
            int             max_out);

/*
 * Run Phase 3 IDA* from each Phase-2 result in p2[0..n2).
 * Writes the first valid solution into *result.
 * Returns 1 if found, 0 otherwise.
 */
int search3(const FullCube *p2,
            int             n2,
            FullCube       *result);

/* -------------------------------------------------------------------------
 * Utility: extract the full move sequence from a FullCube after Phase 3.
 * Writes human-readable move string into buf (caller supplies buf[512]).
 * Returns the total number of moves.
 * ------------------------------------------------------------------------- */
int get_move_string(const FullCube *result, char *buf, int buf_len);

/* -------------------------------------------------------------------------
 * IDA* internals (shared across phases via callbacks)
 * ------------------------------------------------------------------------- */

typedef struct {
    int  depth;        /* current search depth (bound) */
    int  max_depth;    /* maximum allowed (for phase time-limits) */
    int  prev_move;    /* last move applied (for ckmv pruning) */
    int  axis;         /* axis of last move (for skipAxis) */
    /* Phase-specific coordinate state (union might be cleaner later). */
    int  coord1;       /* Center1 sym-class */
    int  coord_rl;     /* Center2 rl */
    int  coord_ct;     /* Center2 ct */
    int  coord_su;     /* Center3 sliceU */
    int  coord_sr;     /* Center3 sliceR */
    int  coord_sf;     /* Center3 sliceF */
    int  coord_ep;     /* Edge3 sym-class (or raw during inner loop) */
    int  coord_ep_raw; /* Edge3 raw-within-class */
    int  eparity;      /* parity bit */
} SearchState;

/* -------------------------------------------------------------------------
 * Top-level entry point
 *
 * Solve a 4×4×4 cube given as a 96-char facelet string (6×16, "URFDLB" order).
 * The solution is written as a null-terminated move string into buf.
 * Returns total move count, or -1 on failure.
 * ------------------------------------------------------------------------- */
int tpr_solve(const char *facelet96, char *buf, int buf_len);

/* -------------------------------------------------------------------------
 * Initialisation
 *
 * Call once before any tpr_solve() call. Builds all phase tables in order:
 *   1. tpr_util_init
 *   2. moves_init
 *   3. corner_cube_init_moves  (cubie.h)
 *   4. center1_init
 *   5. center2_init
 *   6. center3_init
 *   7. edge3_init
 * ------------------------------------------------------------------------- */
void tpr_init(void);

#endif /* SEARCH_H */
