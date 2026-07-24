#ifndef SEARCH_H
#define SEARCH_H

/*
 * TPR solver pipeline:
 *   Phase 1 (search1): IDA* over Center1 sym-coord → up to SEARCH_BEAM1_MAX cubes.
 *   Phase 2 (search2): IDA* over Center2 (rl × ct) → top SEARCH_BEAM2_MAX by moves.
 *   Phase 3 (search3): IDA* over (Center3 × Edge3) → one solution.
 *   Phase 4: ckociemba finishes the reduced 3×3.
 */

#include "cubie.h"

#define SEARCH_BEAM1_MAX 3000
#define SEARCH_BEAM2_MAX 100

int search1(const FullCube *state, FullCube *out, int max_out);
int search2(const FullCube *p1,   int n1, FullCube *out, int max_out);
int search3(const FullCube *p2,   int n2, FullCube *result);

int get_move_string(const FullCube *result, char *buf, int buf_len);

typedef struct {
    int  depth;        /* current search depth (bound) */
    int  max_depth;    /* maximum allowed (for phase time-limits) */
    int  prev_move;    /* last move applied (for ckmv pruning) */
    int  axis;         /* axis of last move (for skip_axis) */
    /* Phase-specific coordinate state (union might be cleaner later). */
    int  coord1;       /* Center1 sym-class */
    int  coord_rl;     /* Center2 rl */
    int  coord_ct;     /* Center2 ct */
    int  coord_su;     /* Center3 slice_U */
    int  coord_sr;     /* Center3 slice_R */
    int  coord_sf;     /* Center3 slice_F */
    int  coord_ep;     /* Edge3 sym-class (or raw during inner loop) */
    int  coord_ep_raw; /* Edge3 raw-within-class */
    int  eparity;      /* parity bit */
} SearchState;

/* Rotate to canonical orientation (U-color on U, F-color on F) after Phase 3. */
void normalize_orientation(FullCube *fc);

/* Solve from a 96-char facelet string (6×16, "URFDLB" order).
 * Writes move string into buf; returns total move count, or -1 on failure. */
int tpr_solve(const char *facelet96, char *buf, int buf_len);

/* Build all phase tables. Must be called once before tpr_solve(). */
void tpr_init(void);

#endif /* SEARCH_H */
