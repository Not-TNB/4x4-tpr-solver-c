#include "../include/search.h"
#include "../include/center1.h"
#include "../include/center2.h"
#include "../include/center3.h"
#include "../include/edge3.h"
#include "../include/tpr_util.h"
#include "../include/moves.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * Phase 1 — IDA* over Center1 sym-class coordinate.
 *
 * Goal: csprun[coord] == 0  (U/D axis oriented).
 * Move set: all 36 moves.
 * Beam output: up to SEARCH_BEAM1_MAX FullCubes sorted by total depth.
 * ------------------------------------------------------------------------- */

static int p1_found;
static FullCube *p1_out;
static int       p1_max;

static void search1_ida(FullCube *fc, int depth, int bound,
                         int coord, int prev_move) {
    int h = center1_prun_get(coord);
    if (h > bound - depth) return;

    if (h == 0 && p1_found < p1_max) {
        /* Found a Phase-1 solution. */
        fullcube_copy(&p1_out[p1_found], fc);
        p1_out[p1_found].length1 = depth;
        p1_found++;
        if (p1_found >= p1_max) return;
        /* Continue search — we want multiple solutions. */
        if (h == 0 && bound - depth == 0) return;
    }

    if (depth == bound) return;

    for (int m = 0; m < 36; m++) {
        if (prev_move < 36 && ckmv[prev_move][m]) {
            m = skipAxis[m] - 1;
            continue;
        }
        int new_coord = center1_move_sym(coord, m);
        fullcube_move(fc, m);
        search1_ida(fc, depth+1, bound, new_coord, m);
        fc->moveLength--;
        if (p1_found >= p1_max) return;
    }
}

int search1(const FullCube *state, FullCube *out, int max_out) {
    p1_found = 0;
    p1_out   = out;
    p1_max   = max_out;

    CenterCube *ct = fullcube_get_center((FullCube *)state);
    int raw_coord  = center1_get(ct->ct);

    /* TODO: convert raw_coord to sym-class using raw2sym[] from center1.h.
     * For now, use the raw coord directly (correctness TBD). */
    int coord = raw_coord;

    for (int bound = 0; bound <= 20 && p1_found < p1_max; bound++) {
        FullCube fc;
        fullcube_copy(&fc, state);
        search1_ida(&fc, 0, bound, coord, 36);
    }
    return p1_found;
}

/* -------------------------------------------------------------------------
 * Phase 2 — Beam search over best Phase-1 results.
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
    int h = center2_prun_get(rl, ct_coord);
    if (h > bound - depth) return;

    if (center2_is_solved(rl, ct_coord) && p2_found < p2_max) {
        fullcube_copy(&p2_out[p2_found], fc);
        p2_out[p2_found].length2 = depth;
        p2_found++;
        return;
    }

    if (depth == bound) return;

    for (int mi = 0; mi < 28; mi++) {
        if (prev_move < 28 && ckmv2[prev_move][mi]) {
            mi = skipAxis2[mi] - 1;
            continue;
        }
        int m   = move2std[mi];
        int nrl = center2_rl_move(rl, mi);
        int nct = center2_ct_move(ct_coord, mi);
        fullcube_move(fc, m);
        search2_ida(fc, depth+1, bound, nrl, nct, mi);
        fc->moveLength--;
        if (p2_found >= p2_max) return;
    }
}

int search2(const FullCube *p1, int n1, FullCube *out, int max_out) {
    p2_found = 0;
    p2_out   = out;
    p2_max   = max_out;

    for (int i = 0; i < n1 && p2_found < p2_max; i++) {
        FullCube fc;
        fullcube_copy(&fc, &p1[i]);
        CenterCube *ct = fullcube_get_center(&fc);
        /* TODO: extract equatorial subset of ct for center2 coords. */
        int rl = 0; /* placeholder */
        int ct_coord = 0;
        (void)ct;

        for (int bound = 0; bound <= 20 && p2_found < p2_max; bound++) {
            FullCube fc2;
            fullcube_copy(&fc2, &fc);
            search2_ida(&fc2, 0, bound, rl, ct_coord, 28);
        }
    }
    return p2_found;
}

/* -------------------------------------------------------------------------
 * Phase 3 — IDA* over (Center3 × Edge3) joint coordinate.
 *
 * Move set: 20 moves (move3std).
 * ------------------------------------------------------------------------- */

static int p3_done;
static FullCube p3_result;

static void search3_ida(FullCube *fc, int depth, int bound,
                          int su, int sr, int sf, int ep, int ep_raw,
                          int eparity, int prev_move) {
    int h_c = center3_prun_get(center3_prun_coord(su, sr, sf, eparity));
    int h_e = edge3_prun_get(ep, ep_raw);
    int h   = h_c > h_e ? h_c : h_e;
    if (h > bound - depth) return;

    if (center3_is_solved(su, sr, sf) && edge3_is_solved(ep, ep_raw)) {
        fullcube_copy(&p3_result, fc);
        p3_result.length3 = depth;
        p3_done = 1;
        return;
    }

    if (depth == bound) return;

    for (int mi = 0; mi < 20; mi++) {
        if (prev_move < 20 && ckmv3[prev_move][mi]) {
            mi = skipAxis3[mi] - 1;
            continue;
        }
        int m    = move3std[mi];
        int nsu  = center3_slice_move(su, mi);
        int nsr  = center3_slice_move(sr, mi);
        int nsf  = center3_slice_move(sf, mi);
        int nep  = edge3_sym_move(ep, mi);
        int nraw = edge3_raw_move(ep_raw, mi);
        /* TODO: track parity correctly */
        fullcube_move(fc, m);
        search3_ida(fc, depth+1, bound, nsu, nsr, nsf, nep, nraw, eparity, mi);
        fc->moveLength--;
        if (p3_done) return;
    }
}

int search3(const FullCube *p2, int n2, FullCube *result) {
    p3_done = 0;

    for (int i = 0; i < n2 && !p3_done; i++) {
        FullCube fc;
        fullcube_copy(&fc, &p2[i]);

        CenterCube *ct = fullcube_get_center(&fc);
        EdgeCube   *ep = fullcube_get_edge(&fc);

        int su   = center3_get_slice_u(ct->ct);
        int sr   = center3_get_slice_r(ct->ct);
        int sf   = center3_get_slice_f(ct->ct);
        int ep_c = edge3_getsym_class(edge3_get_raw(ep->ep));
        int ep_r = edge3_getsym_elem (edge3_get_raw(ep->ep));
        int epar = edge_cube_parity(ep);

        for (int bound = 0; bound <= 20 && !p3_done; bound++) {
            FullCube fc2;
            fullcube_copy(&fc2, &fc);
            search3_ida(&fc2, 0, bound, su, sr, sf, ep_c, ep_r, epar, 20);
        }
    }

    if (p3_done) {
        fullcube_copy(result, &p3_result);
        return 1;
    }
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
    tpr_util_init();
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
