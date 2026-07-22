#include "../include/search.h"
#include "../include/center1.h"
#include "../include/center2.h"
#include "../include/center3.h"
#include "../include/edge3.h"
#include "../include/tpr_util.h"
#include "../include/moves.h"
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Phase 1 -- IDA* over Center1 sym-class coordinate.
 *
 * Goal: csprun[coord] == 0  (U/D axis oriented).
 * Move set: all 36 moves.
 * Beam output: up to SEARCH_BEAM1_MAX FullCubes sorted by total depth.
 * ------------------------------------------------------------------------- */

static int p1_found;
static FullCube *p1_out;
static int       p1_max;

/*
 * search1_ida -- IDA* with accumulated-symmetry tracking.
 *
 * coord: current sym-class (index into csprun/ctsmv).
 * sym:   accumulated symmetry element (index into symmult/symmove/finish).
 *
 * For each move m we conjugate by sym before the table lookup:
 *   conjugated_move = symmove[sym][m]
 *   packed          = ctsmv[coord][conjugated_move]   -> (new_class<<6)|delta_sym
 *   new_sym         = symmult[sym][delta_sym]
 *
 * This ensures coord tracks the TRUE orbit class of the actual cube state.
 * Without it, ctsmv[coord][m] would give the class for the canonical orbit
 * representative rather than the actual state, causing spurious class-0 hits.
 *
 * At class 0 we stop (don't explore further; class 0 = raw=0 = goal).
 * finish[sym]==0 is checked as a guard, but since all 4 primitive rotations
 * fix raw=0 the orbit of raw=0 is a singleton and finish[s]==0 for all s.
 */
static void search1_ida(FullCube *fc, int depth, int bound,
                         int coord, int sym, int prev_move) {
    int h = csprun[coord];
    if (h > bound - depth) return;

    if (h == 0) {
        /* Accept only finish==735470: the 8 tracked pieces are in positions 16-23.
         * This is the only case where center2_getrl() returns a valid [0,69] value.
         * finish==0 (pieces in 0-7) and finish==12869 (pieces in 8-15) leave a
         * 3-color mix in positions 16-23, which causes getrl() to underflow r. */
        if (finish[sym] == 735470 && p1_found < p1_max) {
            fullcube_copy(&p1_out[p1_found], fc);
            p1_out[p1_found].length1 = depth;
            p1_found++;
        }
        return;  /* always stop at class 0 */
    }

    if (depth == bound) return;

    for (int m = 0; m < 36; m++) {
        if (prev_move < 36 && ckmv[prev_move][m]) {
            m = skipAxis[m] - 1;
            continue;
        }
        int sm        = symmove[sym][m];         /* conjugate m by accumulated sym */
        int packed    = ctsmv[coord][sm];
        int new_coord = packed >> 6;
        int new_sym   = symmult[sym][packed & 0x3f];

        fullcube_move(fc, m);
        search1_ida(fc, depth+1, bound, new_coord, new_sym, m);
        fc->moveLength--;
        if (p1_found >= p1_max) return;
    }
}

int search1(const FullCube *state, FullCube *out, int max_out) {
    p1_found = 0;
    p1_out   = out;
    p1_max   = max_out;

    CenterCube *ct = fullcube_get_center((FullCube *)state);

    /* Compute starting (coord, sym, prun) for all three axes.
     * urf=0: UD axis (color%3==0), urf=1: RL axis, urf=2: FB axis.
     * Search order: FB(2), UD(0), RL(1). */
    static const int axis_order[3] = {2, 0, 1};
    int coords[3], syms[3], pruns[3];
    for (int urf = 0; urf < 3; urf++) {
        int packed   = raw2sym[center1_raw_urf(ct->ct, urf)];
        coords[urf]  = packed >> 6;
        syms[urf]    = packed & 0x3f;
        pruns[urf]   = csprun[coords[urf]];
    }

    int min_prun = pruns[0];
    for (int i = 1; i < 3; i++) if (pruns[i] < min_prun) min_prun = pruns[i];

    for (int bound = min_prun; bound <= 20 && p1_found < p1_max; bound++) {
        for (int ai = 0; ai < 3 && p1_found < p1_max; ai++) {
            int urf = axis_order[ai];
            if (pruns[urf] > bound) continue;
            FullCube fc;
            fullcube_copy(&fc, state);
            search1_ida(&fc, 0, bound, coords[urf], syms[urf], 36);
        }
    }
    return p1_found;
}

/* -------------------------------------------------------------------------
 * Phase 2 -- Beam search over best Phase-1 results.
 *
 * For each Phase-1 FullCube, runs IDA* over (rl × ct) Center2 coordinate.
 * Move set: 28 moves (move2std).
 * Keeps top SEARCH_BEAM2_MAX solutions by total move count.
 * ------------------------------------------------------------------------- */

static int p2_found;
static FullCube *p2_out;
static int       p2_max;

static void search2_ida(FullCube *fc, int depth, int bound,
                          int rl, int ct_coord, int prev_move) {
    int h = ctprun[ct_coord * CENTER2_RL_COORDS + rl];
    if (h > bound - depth) return;

    if (center2_is_solved(rl, ct_coord) && p2_found < p2_max) {
        /* Gate: edges must be paired before entering Phase 3.
         * Apply buffered moves to a throwaway copy to avoid corrupting edgeAvail. */
        EdgeCube tmp_edge = fc->edge;
        for (int j = fc->edgeAvail; j < fc->moveLength; j++)
            edge_cube_move(&tmp_edge, fc->moveBuffer[j]);
        if (!edge_cube_check(&tmp_edge)) return;
        fullcube_copy(&p2_out[p2_found], fc);
        p2_out[p2_found].length2 = depth;
        p2_found++;
        return;
    }

    if (depth == bound) return;

    /* Use the same 23-move set that the pruning BFS was built with.
     * The 5 excluded moves (Dwx2, Lwx1, Lwx2, Lwx3, Bwx2) would expand the tree
     * without improving pruning effectiveness. */
    for (int mi = 0; mi < 23; mi++) {
        if (prev_move < 23 && ckmv2[prev_move][mi]) {
            mi = skipAxis2[mi] - 1;
            continue;
        }
        int m   = move2std[mi];
        int nrl = rlmv[rl][mi];
        int nct = ctmv[ct_coord][mi];
        fullcube_move(fc, m);
        search2_ida(fc, depth+1, bound, nrl, nct, mi);
        fc->moveLength--;
        if (p2_found >= p2_max) return;
    }
}

/* Candidate record used to sort Phase-1 results before Phase-2 search. */
typedef struct { int score; int rl; int ct; int idx; } P2Cand;

int search2(const FullCube *p1, int n1, FullCube *out, int max_out) {
    p2_found = 0;
    p2_out   = out;
    p2_max   = max_out;

    /* Pre-score every Phase-1 result by length1 + Phase-2 lower bound.
     * Processing cheapest-first means IDA* hits short solutions early and
     * the p2_found >= p2_max guard exits before touching expensive states. */
    P2Cand cands[SEARCH_BEAM1_MAX];
    for (int i = 0; i < n1; i++) {
        FullCube fc;
        fullcube_copy(&fc, &p1[i]);
        CenterCube *cc = fullcube_get_center(&fc);
        EdgeCube   *ec = fullcube_get_edge(&fc);
        Center2State c2s;
        center2_set(&c2s, cc->ct, parity_u8(ec->ep, 24));
        /* Validate ct invariant: exactly 8 of positions 0-14 must differ from
         * position 15 for the C(15,8) coordinate to be well-defined. */
        int diff = 0;
        for (int k = 0; k < 15; k++)
            if (c2s.ct[k] != c2s.ct[15]) diff++;
        if (diff != 8) {
            cands[i].rl = 0; cands[i].ct = 1; cands[i].score = 999; cands[i].idx = i;
            continue;
        }
        cands[i].rl    = center2_getrl(&c2s);
        cands[i].ct    = center2_getct(&c2s);
        cands[i].score = p1[i].length1 + ctprun[cands[i].ct * CENTER2_RL_COORDS + cands[i].rl];
        cands[i].idx   = i;
    }

    /* Insertion sort ascending by score (n1 ≤ SEARCH_BEAM1_MAX = 500). */
    for (int i = 1; i < n1; i++) {
        P2Cand tmp = cands[i];
        int j = i - 1;
        while (j >= 0 && cands[j].score > tmp.score) { cands[j+1] = cands[j]; j--; }
        cands[j+1] = tmp;
    }

    /* Start with max 9 Phase-2 moves; retry with a larger cap if none found. */
    int max_p2 = 9;
    do {
        for (int ii = 0; ii < n1 && p2_found < p2_max; ii++) {
            int i    = cands[ii].idx;
            int rl   = cands[ii].rl;
            int ct_c = cands[ii].ct;

            FullCube fc;
            fullcube_copy(&fc, &p1[i]);
            fullcube_get_center(&fc);
            fullcube_get_edge(&fc);

            int cap = max_p2;
            if (20 - p1[i].length1 < cap) cap = 20 - p1[i].length1;
            for (int bound = 0; bound <= cap && p2_found < p2_max; bound++) {
                FullCube fc2;
                fullcube_copy(&fc2, &fc);
                search2_ida(&fc2, 0, bound, rl, ct_c, 23);
            }
        }
        max_p2++;
    } while (p2_found == 0 && max_p2 <= 15);

    return p2_found;
}

/* -------------------------------------------------------------------------
 * Phase 3 -- IDA* over (Center3 × Edge3) joint coordinate.
 *
 * Move set: 20 moves (move3std).
 * ------------------------------------------------------------------------- */

static int p3_done;
static FullCube p3_result;

static Edge3State tempe[21];

/*
 * search3_ida -- IDA* over (Center3 × Edge3) joint coordinate.
 *
 * edge_coord = symcord1 * N_RAW + cord2 (sym-reduced, used for pruning and
 * goal detection).  tempe[depth] holds the current std-normalised Edge3State,
 * populated by the caller before each invocation (root: copied from es0;
 * recursive levels: memcpy + edge3_move + edge3_std from parent).
 *
 * Fix 1: tempe is maintained incrementally — no set_from_int per node.
 * Fix 2: cord1x uses getmvrot(end=4) instead of end=10, saving 6 Lehmer iters.
 * Fix 3: both center3 and edge3 heuristics are checked at function entry.
 */
static void search3_ida(FullCube *fc, int depth, int bound,
                          int ct, int edge_coord, int prun, int prev_move) {
    int h_c = c3prun[ct];
    if (h_c > bound - depth) return;

    int h_e = edge3_getprun(edge_coord);   /* Fix 3 */
    if (h_e > bound - depth) return;

    if (ct == 0 && edge_coord == 0) {
        fullcube_copy(&p3_result, fc);
        p3_result.length3 = depth;
        p3_done = 1;
        return;
    }

    if (depth == bound) return;

    /* tempe[depth] already populated by caller — no set_from_int needed. */

    int child_prun = (prun + 1) % 3;
    int remaining  = bound - depth - 1;

    for (int mi = 0; mi < 17; mi++) {
        if (prev_move < 20 && ckmv3[prev_move][mi]) {
            mi = skipAxis3[mi] - 1;
            continue;
        }
        int m    = move3std[mi];
        int nct  = ctmove[ct][mi];

        int h_c2 = c3prun[nct];
        if (h_c2 > remaining) {
            if (h_c2 > remaining + 1 && mi < 14) mi = skipAxis3[mi] - 1;
            continue;
        }

        /* Fix 2: cord1x needs only end=4 (P(12,4) range); saves 6 Lehmer iters. */
        int cord1x         = edge3_getmvrot(tempe[depth].edge, mi * 8, 4);
        int sym_raw        = e3raw2sym[cord1x];
        int symx           = sym_raw & 7;
        int new_cls        = sym_raw >> 3;
        int cord2x         = edge3_getmvrot(tempe[depth].edge, mi * 8 | symx, 10) % EDGE3_RAW_PERMS;
        int new_edge_coord = new_cls * EDGE3_RAW_PERMS + cord2x;

        int h_e2 = edge3_getprun(new_edge_coord);
        if (h_e2 > remaining) {
            if (h_e2 > remaining + 1 && mi < 14) mi = skipAxis3[mi] - 1;
            continue;
        }

        /* Fix 1: build child edge state incrementally from parent. */
        memcpy(&tempe[depth + 1], &tempe[depth], sizeof(Edge3State));
        edge3_move(&tempe[depth + 1], mi);
        edge3_std(&tempe[depth + 1]);

        fullcube_move(fc, m);
        search3_ida(fc, depth + 1, bound, nct, new_edge_coord, child_prun, mi);
        fc->moveLength--;
        if (p3_done) return;
    }
}

int search3(const FullCube *p2, int n2, FullCube *result) {
    p3_done = 0;
    if (n2 == 0) return 0;

    /* ------------------------------------------------------------------
     * Precompute P3 coordinates and heuristics for every P2 candidate,
     * then sort by (total_base + max(h_c, h_e)) ascending.
     * Joint IDA* then tries all candidates in increasing total-bound
     * order, in increasing total-bound order.
     * ------------------------------------------------------------------ */
    typedef struct {
        int        p2_idx;
        int        ct;
        Edge3State es0;        /* std-normalised starting state (pre-sym-rotation) */
        int        edge_coord; /* sym-reduced coordinate for pruning and goal check */
        int        h_c;        /* exact center3 heuristic */
        int        h_e;        /* edge lower-bound: exact depth or 10 if unseen */
        int        total_base; /* length1 + length2 */
        int        min_total;  /* total_base + max(h_c, h_e) */
    } P3Cand;

    P3Cand cands[SEARCH_BEAM2_MAX];
    int nc = 0;

    for (int i = 0; i < n2; i++) {
        FullCube fc;
        fullcube_copy(&fc, &p2[i]);
        CenterCube *cc = fullcube_get_center(&fc);
        EdgeCube   *ec = fullcube_get_edge(&fc);

        int epar = (int)parity_u8(ec->ep, 24);
        Center3State c3s;
        center3_set(&c3s, cc->ct, epar);

        Edge3State es;
        edge3_set_from_edgecube(&es, ec->ep);
        edge3_std(&es);

        /* Sym-reduce to get edge_coord for pruning; keep es intact as es0. */
        int cord1    = edge3_get(&es, 4);
        int sym_raw  = e3raw2sym[cord1];
        int symx     = sym_raw & 7;
        int symcord1 = sym_raw >> 3;
        Edge3State es_rot = es;         /* rotate a copy; es stays as the start state */
        edge3_rotate(&es_rot, symx);
        edge3_std(&es_rot);
        int cord2_sym  = edge3_get(&es_rot, 10) % EDGE3_RAW_PERMS;
        int edge_coord = symcord1 * EDGE3_RAW_PERMS + cord2_sym;

        int ct    = center3_getct(&c3s);
        int h_c = c3prun[ct];
        int h_e = edge3_getprun(edge_coord);
        int tb    = p2[i].length1 + p2[i].length2;
        int hmax  = h_c > h_e ? h_c : h_e;

        cands[nc].p2_idx     = i;
        cands[nc].ct         = ct;
        cands[nc].es0        = es;
        cands[nc].edge_coord = edge_coord;
        cands[nc].h_c        = h_c;
        cands[nc].h_e        = h_e;
        cands[nc].total_base = tb;
        cands[nc].min_total  = tb + hmax;
        nc++;
    }

    /* Insertion-sort ascending by min_total. */
    for (int i = 1; i < nc; i++) {
        P3Cand tmp = cands[i];
        int j = i - 1;
        while (j >= 0 && cands[j].min_total > tmp.min_total) {
            cands[j+1] = cands[j]; j--;
        }
        cands[j+1] = tmp;
    }

    /* Joint IDA*: outer loop = total (P1+P2+P3) bound, inner = candidates.
     * At each total, each candidate runs IDA* at p3_bound = total-total_base.
     * Because candidates are sorted, once min_total > total we can break. */
    int total_min = cands[0].min_total;
    for (int total = total_min; total <= 100 && !p3_done; total++) {
        for (int ci = 0; ci < nc && !p3_done; ci++) {
            P3Cand *c = &cands[ci];
            if (c->min_total > total) break;   /* sorted: rest are also > total */
            int p3_bound = total - c->total_base;
            if (p3_bound > 20) continue;       /* hard cap on P3 length */
            if (c->h_c > p3_bound) continue;  /* center heuristic prunes this bound */

            FullCube fc;
            fullcube_copy(&fc, &p2[c->p2_idx]);
            tempe[0] = c->es0;   /* seed incremental edge state for depth 0 */
            search3_ida(&fc, 0, p3_bound, c->ct, c->edge_coord, p3_bound % 3, 20);
        }
    }

    if (p3_done) { fullcube_copy(result, &p3_result); return 1; }
    return 0;
}

/* -------------------------------------------------------------------------
 * Move string extraction
 * ------------------------------------------------------------------------- */

int get_move_string(const FullCube *result, char *buf, int buf_len) {
    int pos = 0, n = 0;
    for (int i = 0; i < result->moveLength && pos < buf_len - 4; i++) {
        int m = result->moveBuffer[i];
        int written = snprintf(buf + pos, (size_t)(buf_len - pos),
                               "%s ", move2str[m]);
        if (written > 0) pos += written;
        n++;
    }
    if (pos > 0 && buf[pos-1] == ' ') buf[--pos] = '\0';
    return n;
}

/* -------------------------------------------------------------------------
 * Top-level
 * ------------------------------------------------------------------------- */

void tpr_init(void) {
    build_pascal_triangle();
    moves_init();
    corner_cube_init_moves();
    center1_init();
    center2_init();
    center3_init();
    edge3_init();
}

int tpr_solve(const char *facelet96, char *buf, int buf_len) {
    FullCube state;
    fullcube_from_facelet(&state, facelet96);

    FullCube beam1[SEARCH_BEAM1_MAX];
    int n1 = search1(&state, beam1, SEARCH_BEAM1_MAX);
    if (n1 == 0) return -1;

    FullCube beam2[SEARCH_BEAM2_MAX];
    int n2 = search2(beam1, n1, beam2, SEARCH_BEAM2_MAX);
    if (n2 == 0) return -1;

    FullCube result;
    if (!search3(beam2, n2, &result)) return -1;

    /* TODO: call min2phase on result to finish the 3×3 reduction. */

    return get_move_string(&result, buf, buf_len);
}
