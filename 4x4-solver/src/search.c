#include "../ckociemba/include/search.h"
#include "../include/search.h"
#include "../include/cubie.h"
#include <time.h>
#include "../include/center1.h"
#include "../include/center2.h"
#include "../include/center3.h"
#include "../include/edge3.h"
#include "../include/tpr_util.h"
#include "../include/moves.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static const char *kok_path = NULL;

void tpr_set_kok_path(const char *path) { kok_path = path; }

/* -------------------------------------------------------------------------
 * Phase 1 -- IDA* over Center1 sym-class coordinate.
 * Goal: U/D axis oriented.  Move set: 27 moves.
 * ------------------------------------------------------------------------- */

static int p1_found;
static FullCube *p1_out;
static int       p1_max;

/* Accumulates sym so each move is conjugated before the table lookup,
 * tracking the true orbit class rather than just the canonical representative. */
static void search1_ida(FullCube *fc, int depth, int bound,
                         int coord, int sym, int prev_move) {
    int h = csprun[coord];
    if (h > bound - depth) return;

    if (h == 0) {
        /* finish[sym]==735470: no correction needed.
         * finish[sym]==0:     apply z rotation (Fw+Bw').
         * finish[sym]==12869: apply y rotation (Uw+Dw'). */
        if (p1_found < p1_max) {
            if (finish[sym] == 735470) {
                fullcube_copy(&p1_out[p1_found], fc);
                p1_out[p1_found].length1 = depth;
                p1_found++;
            } else if (finish[sym] == 0) {
                fullcube_copy(&p1_out[p1_found], fc);
                fullcube_move(&p1_out[p1_found], 24);  /* Fw */
                fullcube_move(&p1_out[p1_found], 35);  /* Bw' */
                p1_out[p1_found].length1 = depth + 2;
                p1_found++;
            } else if (finish[sym] == 12869) {
                fullcube_copy(&p1_out[p1_found], fc);
                fullcube_move(&p1_out[p1_found], 18);  /* Uw */
                fullcube_move(&p1_out[p1_found], 29);  /* Dw' */
                p1_out[p1_found].length1 = depth + 2;
                p1_found++;
            }
        }
        return;
    }

    if (depth == bound) return;

    for (int m = 0; m < 27; m++) {
        if (prev_move < 27 && ckmv[prev_move][m]) {
            m = skip_axis[m] - 1;
            continue;
        }
        int sm        = symmove[sym][m];         /* conjugate m by accumulated sym */
        int packed    = ctsmv[coord][sm];
        int new_coord = packed >> 6;
        int new_sym   = symmult[sym][packed & 0x3f];

        fullcube_move(fc, m);
        search1_ida(fc, depth+1, bound, new_coord, new_sym, m);
        fc->move_length--;
        if (p1_found >= p1_max) return;
    }
}

int search1(const FullCube *state, FullCube *out, int max_out) {
    p1_found = 0;
    p1_out   = out;
    p1_max   = max_out;

    CenterCube *ct = fullcube_get_center((FullCube *)state);

    /* Try all 3 axes at increasing IDA* bounds; FB(2) first. */
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
            search1_ida(&fc, 0, bound, coords[urf], syms[urf], 27);
        }
    }
    return p1_found;
}

/* -------------------------------------------------------------------------
 * Phase 2 -- IDA* over Center2 (rl × ct) coordinate.
 * Move set: 28 moves.
 * ------------------------------------------------------------------------- */

static int p2_found;
static FullCube *p2_out;
static int       p2_max;

static void search2_ida(FullCube *fc, int depth, int bound,
                          int rl, int ct_coord, int prev_move) {
    int h = ctprun[ct_coord * CENTER2_RL_COORDS + rl];
    if (h > bound - depth) return;

    if (center2_is_solved(rl, ct_coord) && p2_found < p2_max) {
        /* Check edge pairing on a throwaway copy (avoids corrupting edge_avail). */
        EdgeCube tmp_edge = fc->edge;
        for (int j = fc->edge_avail; j < fc->move_length; j++)
            edge_cube_move(&tmp_edge, fc->move_buffer[j]);
        if (!edge_cube_check(&tmp_edge)) return;
        fullcube_copy(&p2_out[p2_found], fc);
        p2_out[p2_found].length2 = depth;
        p2_found++;
        return;
    }

    if (depth == bound) return;

    /* 23-move set matching the pruning BFS (excludes Dwx2, Lwx1–3, Bwx2). */
    for (int mi = 0; mi < 23; mi++) {
        if (prev_move < 23 && ckmv2[prev_move][mi]) {
            mi = skip_axis2[mi] - 1;
            continue;
        }
        int m   = move2std[mi];
        int nrl = rlmv[rl][mi];
        int nct = ctmv[ct_coord][mi];
        fullcube_move(fc, m);
        search2_ida(fc, depth+1, bound, nrl, nct, mi);
        fc->move_length--;
        if (p2_found >= p2_max) return;
    }
}

typedef struct { int score; int rl; int ct; int idx; } P2Cand;

int search2(const FullCube *p1, int n1, FullCube *out, int max_out) {
    p2_found = 0;
    p2_out   = out;
    p2_max   = max_out;

    /* Sort cheapest-first so IDA* hits short solutions before the beam cap. */
    P2Cand cands[SEARCH_BEAM1_MAX];
    for (int i = 0; i < n1; i++) {
        FullCube fc;
        fullcube_copy(&fc, &p1[i]);
        CenterCube *cc = fullcube_get_center(&fc);
        EdgeCube   *ec = fullcube_get_edge(&fc);
        Center2State c2s;
        center2_set(&c2s, cc->ct, parity_u8(ec->ep, 24));
        /* Validate C(15,8) invariant: exactly 8 of positions 0-14 differ from 15. */
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
 * Phase 3 -- IDA* over (Center3 × Edge3).  Move set: 20 moves.
 * ------------------------------------------------------------------------- */

static int p3_done;
static FullCube p3_result;

static uint8_t tempe[21][12];

TprDiag tpr_diag;

/* tempe[depth] holds the std-normalised edge permutation, seeded by caller. */
static void search3_ida(FullCube *fc, int depth, int bound,
                          int ct, int edge_coord, int edge_prun, int prev_move) {
    tpr_diag.p3_nodes++;
    if (ct == 0 && edge_coord == 0) {
        FullCube tmp;
        fullcube_copy(&tmp, fc);
        fullcube_copy(&p3_result, &tmp);
        p3_result.length3 = depth;
        p3_done = 1;
        return;
    }

    if (depth == bound) return;

    int remaining = bound - depth - 1;

    for (int mi = 0; mi < 17; mi++) {
        if (prev_move < 20 && ckmv3[prev_move][mi]) {
            mi = skip_axis3[mi] - 1;
            continue;
        }
        int m    = move3std[mi];
        int nct  = ctmove[ct][mi];

        int h_c2 = c3prun[nct];
        if (h_c2 > remaining) {
            if (h_c2 > remaining + 1 && mi < 14) mi = skip_axis3[mi] - 1;
            continue;
        }

        int cord1x         = edge3_getmvrot4(tempe[depth], mi * 8);
        int sym_raw        = e3raw2sym[cord1x];
        int symx           = sym_raw & 7;
        int new_cls        = sym_raw >> 3;
        if (e3sym_min_prun[new_cls] > remaining) continue;
        int cord2x         = edge3_getmvrot10(tempe[depth], mi * 8 | symx) % EDGE3_RAW_PERMS;
        int new_edge_coord = new_cls * EDGE3_RAW_PERMS + cord2x;

        int new_h_e = edge3_getprun(new_edge_coord, edge_prun);
        if (new_h_e > remaining) {
            if (new_h_e > remaining + 1 && mi < 14) mi = skip_axis3[mi] - 1;
            continue;
        }

        for (int k = 0; k < 12; k++)
            tempe[depth+1][k] = e3cval[mi][tempe[depth][e3cpos[mi][k]]];

        fullcube_move(fc, m);
        search3_ida(fc, depth + 1, bound, nct, new_edge_coord, new_h_e, mi);
        fc->move_length--;
        if (p3_done) return;
    }
}

int search3(const FullCube *p2, int n2, FullCube *result) {
    p3_done = 0;
    tpr_diag.p3_nodes = 0;
    tpr_diag.p3_bound = 0;
    tpr_diag.n_cands  = n2;
    tpr_diag.h_e_best = -1;
    tpr_diag.h_c_best = -1;
    /* p1_ms/p2_ms/n1 set by tpr_solve; p3_ms set below */
    if (n2 == 0) return 0;

    /* Precompute coordinates and heuristics; sort by min_total. */
    typedef struct {
        int        p2_idx;
        int        ct;
        uint8_t    es0[12];    /* std-normalised edge[], pre-sym rot */
        int        edge_coord; /* sym-reduced */
        int        h_c;
        int        h_e;        /* exact depth from BFS */
        int        total_base;
        int        min_total;
    } P3Cand;

    P3Cand cands[SEARCH_BEAM2_MAX];
    int nc = 0;

    for (int i = 0; i < n2; i++) {
        FullCube fc;
        fullcube_copy(&fc, &p2[i]);
        CenterCube *cc = fullcube_get_center(&fc);
        EdgeCube   *ec = fullcube_get_edge(&fc);
        CornerCube  *cor = fullcube_get_corner(&fc);
        Edge3State   es;
        int epar = edge3_set_from_edgecube(&es, ec->ep) ^ corner_cube_parity(cor);
        Center3State c3s;
        center3_set(&c3s, cc->ct, epar);
        edge3_std(&es);

        /* Sym-reduce to get edge_coord for pruning; keep es intact as es0. */
        int cord1    = edge3_get(&es, 4);
        int sym_raw  = e3raw2sym[cord1];
        int symx     = sym_raw & 7;
        int symcord1 = sym_raw >> 3;

        Edge3State es_rot = es;
        edge3_rotate(&es_rot, symx);
        edge3_std(&es_rot);
        int cord2_sym  = edge3_get(&es_rot, 10) % EDGE3_RAW_PERMS;
        int edge_coord = symcord1 * EDGE3_RAW_PERMS + cord2_sym;

        int ct    = center3_getct(&c3s);
        int h_c = c3prun[ct];
        int h_e = edge3_getprun_init(edge_coord);
        int tb    = p2[i].length1 + p2[i].length2;
        int hmax  = h_c > h_e ? h_c : h_e;

        cands[nc].p2_idx     = i;
        cands[nc].ct         = ct;
        memcpy(cands[nc].es0, es.edge, 12);
        cands[nc].edge_coord = edge_coord;
        cands[nc].h_c        = h_c;
        cands[nc].h_e        = h_e;
        cands[nc].total_base = tb;
        cands[nc].min_total  = tb + hmax;
        nc++;
    }

    for (int i = 1; i < nc; i++) {
        P3Cand tmp = cands[i];
        int j = i - 1;
        while (j >= 0 && cands[j].min_total > tmp.min_total) {
            cands[j+1] = cands[j]; j--;
        }
        cands[j+1] = tmp;
    }

    /* Joint IDA*: p3_bound = total - total_base; sorted order enables early break. */
    int total_min = cands[0].min_total;
    for (int total = total_min; total <= 100 && !p3_done; total++) {
        for (int ci = 0; ci < nc && !p3_done; ci++) {
            P3Cand *c = &cands[ci];
            if (c->min_total > total) break;   /* sorted: rest are also > total */
            int p3_bound = total - c->total_base;
            if (p3_bound > 20) continue;       /* hard cap on P3 length */
            if (c->h_c > p3_bound) continue;
            if (c->h_e > p3_bound) continue;

            FullCube fc;
            fullcube_copy(&fc, &p2[c->p2_idx]);
            memcpy(tempe[0], c->es0, 12);
            if (tpr_diag.h_e_best < 0) {
                tpr_diag.h_e_best = c->h_e;
                tpr_diag.h_c_best = c->h_c;
                tpr_diag.p3_bound = p3_bound;
            }
            search3_ida(&fc, 0, p3_bound, c->ct, c->edge_coord, c->h_e, 20);
            if (p3_done) tpr_diag.p3_bound = p3_bound;
        }
    }

    if (p3_done) { fullcube_copy(result, &p3_result); return 1; }
    return 0;
}


int get_move_string(const FullCube *result, char *buf, int buf_len) {
    int pos = 0, n = 0;
    for (int i = 0; i < result->move_length && pos < buf_len - 4; i++) {
        int m = result->move_buffer[i];
        int written = snprintf(buf + pos, (size_t)(buf_len - pos),
                               "%s ", move2str[m]);
        if (written > 0) pos += written;
        n++;
    }
    if (pos > 0 && buf[pos-1] == ' ') buf[--pos] = '\0';
    return n;
}

void normalize_orientation(FullCube *fc) {
    /* Matches center_group[] order in fullcube_to_333_facelet. */
    static const int face_cg[6] = {0, 16, 8, 4, 20, 12};

    CenterCube *cen = fullcube_get_center(fc);
    int col[6];
    for (int i = 0; i < 6; i++) col[i] = cen->ct[face_cg[i]];

    fullcube_get_edge(fc);
    fullcube_get_corner(fc);

    /* rotate to put U-color on U */
    int u = 0;
    for (int i = 0; i < 6; i++) if (col[i] == 0) { u = i; break; }

    switch (u) {
        case 0: break;
        case 1: fullcube_do_alg(fc, (int[]){Fwx3, Bwx1}, 2); break; /* z' */
        case 2: fullcube_do_alg(fc, (int[]){Rwx1, Lwx3}, 2); break; /* x' */
        case 3: fullcube_do_alg(fc, (int[]){Rwx2, Lwx2}, 2); break; /* x2 */
        case 4: fullcube_do_alg(fc, (int[]){Fwx1, Bwx3}, 2); break; /* z  */
        case 5: fullcube_do_alg(fc, (int[]){Rwx3, Lwx1}, 2); break; /* x  */
    }

    /* rotate to put F-color on F */
    cen = fullcube_get_center(fc);
    for (int i = 0; i < 6; i++) col[i] = cen->ct[face_cg[i]];

    int f = 2;
    for (int i = 0; i < 6; i++) if (col[i] == 2) { f = i; break; }

    switch (f) {
        case 2: break;
        case 1: fullcube_do_alg(fc, (int[]){Uwx1, Dwx3}, 2); break; /* y  */
        case 4: fullcube_do_alg(fc, (int[]){Uwx3, Dwx1}, 2); break; /* y' */
        case 5: fullcube_do_alg(fc, (int[]){Uwx2, Dwx2}, 2); break; /* y2 */
    }
}


void tpr_init(void) {
    build_pascal_triangle();
    moves_init();
    corner_cube_init_moves();
    center1_init();
    center2_init();
    center3_init();
    edge3_init();
}

static double ms_since(struct timespec t0) {
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) * 1e-6;
}

int tpr_solve(const char *facelet96, char *buf, int buf_len) {
    tpr_diag.p1_ms = 0; tpr_diag.p2_ms = 0; tpr_diag.p3_ms = 0;
    tpr_diag.n1 = 0;

    FullCube state;
    fullcube_from_facelet(&state, facelet96);

    struct timespec tp;
    FullCube beam1[SEARCH_BEAM1_MAX];
    clock_gettime(CLOCK_MONOTONIC, &tp);
    int n1 = search1(&state, beam1, SEARCH_BEAM1_MAX);
    tpr_diag.p1_ms = ms_since(tp);
    tpr_diag.n1 = n1;
    if (n1 == 0) return -1;

    FullCube beam2[SEARCH_BEAM2_MAX];
    clock_gettime(CLOCK_MONOTONIC, &tp);
    int n2 = search2(beam1, n1, beam2, SEARCH_BEAM2_MAX);
    tpr_diag.p2_ms = ms_since(tp);
    if (n2 == 0) return -1;

    FullCube result;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    if (!search3(beam2, n2, &result)) return -1;
    tpr_diag.p3_ms = ms_since(tp);

    normalize_orientation(&result);

    int n = get_move_string(&result, buf, buf_len);

    char facelet54[55];
    fullcube_to_333_facelet(&result, facelet54);

    static const int center_pos[6] = {4, 13, 22, 31, 40, 49};
    static const char expected[6] = "URFDLB";
    char cmap[128] = {0};
    for (int i = 0; i < 6; i++)
        cmap[(unsigned char)facelet54[center_pos[i]]] = expected[i];
    for (int i = 0; i < 54; i++)
        facelet54[i] = cmap[(unsigned char)facelet54[i]];

    /* solution() returns NULL for OLL parity (instant) and timeout.
     * timeOut is now in ms; elapsed time distinguishes the two NULL causes. */
    const char *KOK_PATH = kok_path ? kok_path : "../4x4-solver/ckociemba/cprunetables";
#define KOK_TIMEOUT_MS 100
    struct timespec t_kok;
    clock_gettime(CLOCK_MONOTONIC, &t_kok);
    char *sol_333 = solution(facelet54, 21, KOK_TIMEOUT_MS, 0, KOK_PATH);
    bool sol_instant = (ms_since(t_kok) < KOK_TIMEOUT_MS / 2.0);

    if (sol_333 == NULL && sol_instant) {
        /* OLL parity: verification fails before the timer starts. */
        char tmp = facelet54[7]; facelet54[7] = facelet54[19]; facelet54[19] = tmp;
        sol_333 = solution(facelet54, 21, KOK_TIMEOUT_MS, 0, KOK_PATH);
    }
    if (sol_333 == NULL)
        sol_333 = solution(facelet54, 25, 60000, 0, KOK_PATH);
#undef KOK_TIMEOUT_MS

    if (sol_333) {
        int pos = (int)strlen(buf);
        if (pos > 0 && pos < buf_len - 1) buf[pos++] = ' ';
        snprintf(buf + pos, (size_t)(buf_len - pos), "%s", sol_333);
        free(sol_333);
    }
    return n;
}
