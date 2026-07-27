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

/* Per-solve diagnostics, reset each call. */
typedef struct {
    long long p3_nodes;   /* IDA* nodes expanded in search3_ida */
    int       p3_bound;   /* IDA* depth at solution */
    int       n_cands;    /* Phase-2 → Phase-3 candidates */
    int       h_e_best;   /* h_e of the winning P3 candidate */
    int       h_c_best;   /* h_c of the winning P3 candidate */
    double    p1_ms;
    double    p2_ms;
    double    p3_ms;
    int       n1;         /* Phase-1 beam size */
} TprDiag;
extern TprDiag tpr_diag;

/* Rotate to canonical orientation after Phase 3. */
void normalize_orientation(FullCube *fc);

/* Solve from a 96-char facelet string; writes move string into buf, returns move count or -1. */
int tpr_solve(const char *facelet96, char *buf, int buf_len);

/* Build all phase tables. Must be called once before tpr_solve(). */
void tpr_init(void);

/* Override the path to the ckociemba pruning-table cache directory.
 * Default: "../4x4-solver/ckociemba/cprunetables" (correct when CWD is test/).
 * Call before the first tpr_solve() if running from a different directory. */
void tpr_set_kok_path(const char *path);

#endif /* SEARCH_H */
