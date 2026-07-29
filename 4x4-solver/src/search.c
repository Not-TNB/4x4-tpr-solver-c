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
 * Goal: U/D axis oriented. Move set: 27 moves.
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

/*
 * Determine T1 (0-5) and T2 (0-3) rotation indices from center colors after
 * Phase 3.  Does not modify fc.
 *
 * T1: which of {I, x, x', x2, z, z'} puts the U-color on U.
 * T2: which of {I, y, y', y2}  puts the F-color on F after T1.
 *
 * These are the same rotations normalize_orientation() applies; recording
 * them lets tpr_solve conjugate ckociemba's output instead of rotating the
 * cube.
 */
static void get_orientation(FullCube *fc, int *t1_out, int *t2_out) {
    static const int face_cg[6] = {0, 16, 8, 4, 20, 12};
    CenterCube *cen = fullcube_get_center(fc);
    int col[6];
    for (int i = 0; i < 6; i++) col[i] = cen->ct[face_cg[i]];

    int u = 0;
    for (int i = 0; i < 6; i++) if (col[i] == 0) { u = i; break; }

    /* t1_idx[u]: T1 rotation index for each white-face position.
     * u=0→I(0), u=1→z'(5), u=2→x'(2), u=3→x2(3), u=4→z(4), u=5→x(1) */
    static const int t1_idx[6] = {0, 5, 2, 3, 4, 1};
    *t1_out = t1_idx[u];

    /* For each T1 (indexed by u), which original face ends up at each
     * canonical position?  face_after_t1[u][canonical] = original_face.
     * Face indices: U=0 R=1 F=2 D=3 L=4 B=5.                           */
    static const uint8_t face_after_t1[6][6] = {
        {0,1,2,3,4,5},  /* I:  identity                                  */
        {1,3,2,4,0,5},  /* z': R→U, U→L, L→D, D→R                       */
        {2,1,3,5,4,0},  /* x': F→U, U→B, B→D, D→F                       */
        {3,1,5,0,4,2},  /* x2: D↔U, F↔B                                  */
        {4,0,2,1,3,5},  /* z:  L→U, U→R, R→D, D→L                       */
        {5,1,0,2,4,3},  /* x:  B→U, U→F, F→D, D→B                       */
    };

    /* Find which canonical face (not U=0, not D=3) holds green (color 2)
     * after T1.  Default f=2 handles "already at F".                    */
    int f = 2;
    for (int ci = 0; ci < 6; ci++) {
        if (ci == 0 || ci == 3) continue;
        if (col[face_after_t1[u][ci]] == 2) { f = ci; break; }
    }

    /* t2_map[f]: T2 index.  f=2→I(0), f=1→y(1), f=4→y'(2), f=5→y2(3) */
    static const int t2_map[6] = {-1, 1, 0, -1, 2, 3};
    *t2_out = t2_map[f];
}

/*
 * Tokenise ckociemba's space-separated move string, conjugate each move by
 * T = T2·T1 (T1 applied first during normalisation), and append to buf.
 *
 * Conjugation rule: φ_T(m) = T1⁻¹(T2⁻¹ m T2)T1.
 * Applied as: look up fo_t2[t2][face], then fo_t1[t1][result].
 * This is REVERSE of normalisation order (T2 first, then T1).
 */
static void conjugate_kok_sol(const char *sol333, int t1, int t2,
                               char *buf, int *pos, int buf_len) {
    if (!sol333) return;

    /* fo_t1[t1][canonical_face] = original_face
     * t1: 0=I, 1=x, 2=x', 3=x2, 4=z, 5=z'                            */
    static const uint8_t fo_t1[6][6] = {
        {0,1,2,3,4,5},  /* I  */
        {5,1,0,2,4,3},  /* x  */
        {2,1,3,5,4,0},  /* x' */
        {3,1,5,0,4,2},  /* x2 */
        {4,0,2,1,3,5},  /* z  */
        {1,3,2,4,0,5},  /* z' */
    };
    /* fo_t2[t2][canonical_face] = original_face
     * t2: 0=I, 1=y, 2=y', 3=y2                                         */
    static const uint8_t fo_t2[4][6] = {
        {0,1,2,3,4,5},  /* I  */
        {0,5,1,3,2,4},  /* y  */
        {0,2,4,3,5,1},  /* y' */
        {0,4,5,3,1,2},  /* y2 */
    };
    static const char face_chars[6] = "URFDLB";

    const char *p = sol333;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;

        int f;
        switch (*p) {
            case 'U': f = 0; break; case 'R': f = 1; break;
            case 'F': f = 2; break; case 'D': f = 3; break;
            case 'L': f = 4; break; case 'B': f = 5; break;
            default:  while (*p && *p != ' ') p++; continue;
        }
        p++;

        int power = 0;
        if      (*p == '2')  { power = 1; p++; }
        else if (*p == '\'') { power = 2; p++; }

        int orig_f = fo_t1[t1][fo_t2[t2][f]];

        if (*pos > 0 && *pos < buf_len - 1) buf[(*pos)++] = ' ';
        if (*pos < buf_len - 1) buf[(*pos)++] = face_chars[orig_f];
        if (power == 1 && *pos < buf_len - 1) buf[(*pos)++] = '2';
        if (power == 2 && *pos < buf_len - 1) buf[(*pos)++] = '\'';
    }
    if (*pos < buf_len) buf[*pos] = '\0';
}

/* Append the T1 then T2 rotation wide-moves so the solved cube ends up in
 * white-top / green-front orientation.  Only called when orient == true.  */
static void append_orient_moves(int t1, int t2, char *buf, int *pos, int buf_len) {
    static const int t1_alg[6][2] = {
        {-1,   -1  },  /* I  */
        {Rwx3, Lwx1},  /* x  */
        {Rwx1, Lwx3},  /* x' */
        {Rwx2, Lwx2},  /* x2 */
        {Fwx1, Bwx3},  /* z  */
        {Fwx3, Bwx1},  /* z' */
    };
    static const int t2_alg[4][2] = {
        {-1,   -1  },  /* I  */
        {Uwx1, Dwx3},  /* y  */
        {Uwx3, Dwx1},  /* y' */
        {Uwx2, Dwx2},  /* y2 */
    };

    for (int i = 0; i < 2; i++) {
        int m = t1_alg[t1][i];
        if (m < 0) break;
        if (*pos > 0 && *pos < buf_len - 1) buf[(*pos)++] = ' ';
        int w = snprintf(buf + *pos, (size_t)(buf_len - *pos), "%s", move2str[m]);
        if (w > 0 && *pos + w < buf_len) *pos += w;
    }
    for (int i = 0; i < 2; i++) {
        int m = t2_alg[t2][i];
        if (m < 0) break;
        if (*pos > 0 && *pos < buf_len - 1) buf[(*pos)++] = ' ';
        int w = snprintf(buf + *pos, (size_t)(buf_len - *pos), "%s", move2str[m]);
        if (w > 0 && *pos + w < buf_len) *pos += w;
    }
    if (*pos < buf_len) buf[*pos] = '\0';
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

int tpr_solve(const char *facelet96, char *buf, int buf_len, bool orient) {
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

    /* Determine orientation from center colors without mutating result. */
    int t1, t2;
    get_orientation(&result, &t1, &t2);

    get_move_string(&result, buf, buf_len);
    int pos = (int)strlen(buf);

    /* Normalise a copy for fullcube_to_333_facelet; result stays unmutated. */
    FullCube norm;
    fullcube_copy(&norm, &result);
    normalize_orientation(&norm);

    char facelet54[55];
    fullcube_to_333_facelet(&norm, facelet54);

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

    /* Conjugate ckociemba's canonical-frame moves into the original frame. */
    conjugate_kok_sol(sol_333, t1, t2, buf, &pos, buf_len);
    free(sol_333);

    /* Count total solving moves (P1-P3 + kok) before appending orient moves. */
    int solve_moves = 0;
    for (int i = 0, in_tok = 0; i < pos; i++) {
        if (buf[i] == ' ') in_tok = 0;
        else if (!in_tok) { in_tok = 1; solve_moves++; }
    }

    /* If orient requested, append rotation moves to reach white-top/green-front. */
    if (orient)
        append_orient_moves(t1, t2, buf, &pos, buf_len);

    return solve_moves;
}
