/*
 * test_tpr.c — TPR solver test suite
 *
 * Two sections:
 *   1. Unit tests  — verify math primitives, move tables, and cubie operations
 *      before any search infrastructure exists.
 *
 *   2. Integration tests  — generate random scrambles, run the solve pipeline,
 *      validate each phase goal independently, and report how far each scramble
 *      made it.
 *
 * Usage:
 *   ./test_tpr            (fixed seed, normal output)
 *   ./test_tpr -v         (verbose: full pipeline trace per scramble)
 *   TPR_SEED=0xABCD ./test_tpr
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "../4x4-solver/include/search.h"
#include "../4x4-solver/include/cubie.h"
#include "../4x4-solver/include/center1.h"
#include "../4x4-solver/include/center2.h"
#include "../4x4-solver/include/center3.h"
#include "../4x4-solver/include/edge3.h"
#include "../4x4-solver/include/moves.h"
#include "../4x4-solver/include/tpr_util.h"

/* =========================================================================
 * ANSI colours
 * ========================================================================= */
#define C_RED    "\033[31m"
#define C_GREEN  "\033[32m"
#define C_YELLOW "\033[33m"
#define C_CYAN   "\033[36m"
#define C_BOLD   "\033[1m"
#define C_RESET  "\033[0m"

/* =========================================================================
 * Solved-state reference
 *
 * Center positions (matches centerFacelet[] ordering in cubie.c):
 *   0-3  = U face,  4-7  = D face,  8-11 = F face,
 *   12-15 = B face, 16-19 = R face, 20-23 = L face.
 * Colors (TPR ordering): U=0 R=1 F=2 D=3 L=4 B=5.
 * ========================================================================= */
#define COL_U 0
#define COL_R 1
#define COL_F 2
#define COL_D 3
#define COL_L 4
#define COL_B 5

static const int SOLVED_CT[24] = {
    COL_U, COL_U, COL_U, COL_U,   /* 0-3   U-face centers  */
    COL_D, COL_D, COL_D, COL_D,   /* 4-7   D-face centers  */
    COL_F, COL_F, COL_F, COL_F,   /* 8-11  F-face centers  */
    COL_B, COL_B, COL_B, COL_B,   /* 12-15 B-face centers  */
    COL_R, COL_R, COL_R, COL_R,   /* 16-19 R-face centers  */
    COL_L, COL_L, COL_L, COL_L,   /* 20-23 L-face centers  */
};

/* =========================================================================
 * Phase goal validators
 *
 * Each function checks one phase's invariant on the sub-cube(s) of a FullCube
 * that has been flushed (get_center/get_edge already called).
 * ========================================================================= */

/* Phase 1: all 8 U/D-colored stickers are on U (positions 0-3) or D (4-7). */
static bool goal_p1(const CenterCube *ct) {
    for (int i = 0; i < 8; i++) {
        if (ct->ct[i] != COL_U && ct->ct[i] != COL_D) return false;
    }
    /* Ensure no U/D stickers leaked onto equatorial faces. */
    for (int i = 8; i < 24; i++) {
        if (ct->ct[i] == COL_U || ct->ct[i] == COL_D) return false;
    }
    return true;
}

/* Phase 2: R centers in R slots (16-19), L centers in L slots (20-23).
 * Phase 1 condition must also hold. */
static bool goal_p2(const CenterCube *ct) {
    if (!goal_p1(ct)) return false;
    for (int i = 16; i < 20; i++) if (ct->ct[i] != COL_R) return false;
    for (int i = 20; i < 24; i++) if (ct->ct[i] != COL_L) return false;
    return true;
}

/* Phase 3 centres: every center is in its home slot. */
static bool goal_p3_centers(const CenterCube *ct) {
    for (int i = 0; i < 24; i++)
        if (ct->ct[i] != SOLVED_CT[i]) return false;
    return true;
}

/* Phase 3 edges: every A-wing at position i (0..11) and B-wing at position
 * i+12 belong to the same edge pair.  ep[i] % 12 == ep[i+12] % 12. */
static bool goal_p3_edges(const EdgeCube *ep) {
    for (int i = 0; i < 12; i++) {
        if (ep->ep[i] % 12 != ep->ep[i + 12] % 12) return false;
    }
    return true;
}

/* Fully solved: all sub-cubes are identity. */
static bool goal_solved(const FullCube *fc) {
    /* Centers */
    for (int i = 0; i < 24; i++)
        if (fc->center.ct[i] != (uint8_t)SOLVED_CT[i]) return false;
    /* Edges */
    for (int i = 0; i < 24; i++)
        if (fc->edge.ep[i] != (uint8_t)i) return false;
    /* Corners */
    for (int i = 0; i < 8; i++) {
        if (fc->corner.cp[i] != (uint8_t)i) return false;
        if (fc->corner.co[i] != 0) return false;
    }
    return true;
}

/* =========================================================================
 * Unit-test harness
 * ========================================================================= */

static int g_pass = 0, g_fail = 0;

#define EXPECT(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; \
           printf("  " C_RED "FAIL" C_RESET " %s\n", msg); } \
} while (0)

#define EXPECT_EQ(a, b, msg) do { \
    long long _a=(long long)(a), _b=(long long)(b); \
    if (_a==_b) { g_pass++; } \
    else { g_fail++; \
           printf("  " C_RED "FAIL" C_RESET " %s  (got %lld, expected %lld)\n", \
                  msg, _a, _b); } \
} while (0)

/* =========================================================================
 * Unit tests — tpr_util
 * ========================================================================= */

static void ut_tpr_util(void) {
    printf(C_BOLD "--- tpr_util ---" C_RESET "\n");

    /* Binomial coefficients */
    EXPECT_EQ(Cnk[0][0],    1,       "C(0,0)");
    EXPECT_EQ(Cnk[1][0],    1,       "C(1,0)");
    EXPECT_EQ(Cnk[4][2],    6,       "C(4,2)");
    EXPECT_EQ(Cnk[8][4],   70,       "C(8,4)   [slice coord range]");
    EXPECT_EQ(Cnk[16][4], 1820,      "C(16,4)  [Center2 rl range]");
    EXPECT_EQ(Cnk[24][8], 735471,    "C(24,8)  [Center1 raw range]");

    /* Pascal symmetry: C(n,k) == C(n, n-k) */
    EXPECT_EQ(Cnk[10][3], Cnk[10][7], "C(10,3)==C(10,7)");

    /* Factorials */
    EXPECT_EQ(fact[0],  1,         "0!");
    EXPECT_EQ(fact[1],  1,         "1!");
    EXPECT_EQ(fact[5],  120,       "5!");
    EXPECT_EQ(fact[8],  40320,     "8!  [corner/edge perm range]");
    EXPECT_EQ(fact[12], 479001600, "12!");

    /* Parity: identity is even */
    uint8_t id8[8] = {0,1,2,3,4,5,6,7};
    EXPECT_EQ(parity_u8(id8, 8), 0, "parity(identity) == 0");

    uint8_t sw8[8] = {1,0,2,3,4,5,6,7};  /* single swap → odd */
    EXPECT_EQ(parity_u8(sw8, 8), 1, "parity(swap 0↔1) == 1");

    uint8_t cyc8[8] = {1,2,0,3,4,5,6,7};  /* 3-cycle → even */
    EXPECT_EQ(parity_u8(cyc8, 8), 0, "parity(3-cycle) == 0");

    /* set8perm: decode Lehmer rank */
    uint8_t p[8];
    set8perm(p, 0);
    bool id_ok = true;
    for (int i = 0; i < 8; i++) id_ok &= (p[i] == (uint8_t)i);
    EXPECT(id_ok, "set8perm(0) == {0,1,2,3,4,5,6,7}");

    set8perm(p, 40319);  /* last rank = reverse */
    bool rev_ok = true;
    for (int i = 0; i < 8; i++) rev_ok &= (p[i] == (uint8_t)(7-i));
    EXPECT(rev_ok, "set8perm(40319) == {7,6,5,4,3,2,1,0}");

    /* Round-trip: set8perm → get_perm_rank — spot-check every 1000th rank */
    bool rt_ok = true;
    for (int idx = 0; idx < 40320 && rt_ok; idx += 1000) {
        set8perm(p, idx);
        int a[8]; for (int i = 0; i < 8; i++) a[i] = p[i];
        if (get_perm_rank(a, 8) != idx) rt_ok = false;
    }
    EXPECT(rt_ok, "set8perm/get_perm_rank round-trip (every 1000th rank)");

    /* swap4 CW cycle: a←b←c←d←a  (key=0) */
    uint8_t arr[4] = {10,20,30,40};
    swap4_u8(arr, 0,1,2,3, 0);
    EXPECT(arr[0]==20 && arr[1]==30 && arr[2]==40 && arr[3]==10,
           "swap4_u8 CW");

    /* swap4 x4 = identity */
    uint8_t arr2[4] = {10,20,30,40};
    for (int k = 0; k < 4; k++) swap4_u8(arr2, 0,1,2,3, 0);
    EXPECT(arr2[0]==10 && arr2[1]==20 && arr2[2]==30 && arr2[3]==40,
           "swap4_u8 ×4 == identity");
}

/* =========================================================================
 * Unit tests — moves
 * ========================================================================= */

static void ut_moves(void) {
    printf(C_BOLD "--- moves ---" C_RESET "\n");

    /* Same face is always redundant */
    EXPECT( ckmv[Ux1][Ux2],   "ckmv: U2 after U  is redundant");
    EXPECT( ckmv[Ux1][Ux3],   "ckmv: U' after U  is redundant");
    EXPECT( ckmv[Rx2][Rx1],   "ckmv: R  after R2 is redundant");
    EXPECT( ckmv[Bx3][Bx3],   "ckmv: B' after B' is redundant");

    /* Different non-commuting faces are fine */
    EXPECT(!ckmv[Ux1][Rx1],   "ckmv: R after U is not redundant");
    EXPECT(!ckmv[Fx1][Ux1],   "ckmv: U after F is not redundant");

    /* Opposite commuting pairs: lower axis first is canonical */
    EXPECT( ckmv[Dx1][Ux1],   "ckmv: U after D is redundant (wrong order)");
    EXPECT(!ckmv[Ux1][Dx1],   "ckmv: D after U is not redundant (canonical)");
    EXPECT( ckmv[Lx1][Rx1],   "ckmv: R after L is redundant (wrong order)");
    EXPECT(!ckmv[Rx1][Lx1],   "ckmv: L after R is not redundant (canonical)");
    EXPECT( ckmv[Bx1][Fx1],   "ckmv: F after B is redundant (wrong order)");
    EXPECT(!ckmv[Fx1][Bx1],   "ckmv: B after F is not redundant (canonical)");

    /* Sentinel row: no previous move → nothing is redundant */
    bool sentinel_ok = true;
    for (int m = 0; m < 36; m++) sentinel_ok &= !ckmv[36][m];
    EXPECT(sentinel_ok, "ckmv[36][*] all false (sentinel)");

    /* skipAxis[U*] points past all U moves */
    EXPECT(skipAxis[Ux1] >= 3, "skipAxis[Ux1] >= 3 (past U moves)");
    EXPECT(skipAxis[Ux3] >= 3, "skipAxis[Ux3] >= 3");

    /* move2std contains valid indices in [0,35], terminated by EOM */
    int cnt2 = 0;
    bool std2_ok = true;
    for (int i = 0; move2std[i] != EOM; i++) {
        if (move2std[i] < 0 || move2std[i] >= 36) { std2_ok = false; break; }
        cnt2++;
    }
    EXPECT(std2_ok,     "move2std entries in [0,35]");
    EXPECT_EQ(cnt2, 28, "move2std has 28 moves");

    /* move3std has 20 moves */
    int cnt3 = 0;
    bool std3_ok = true;
    for (int i = 0; move3std[i] != EOM; i++) {
        if (move3std[i] < 0 || move3std[i] >= 36) { std3_ok = false; break; }
        cnt3++;
    }
    EXPECT(std3_ok,     "move3std entries in [0,35]");
    EXPECT_EQ(cnt3, 20, "move3std has 20 moves");

    /* std2move and std3move are consistent reverse maps */
    bool rmap2_ok = true, rmap3_ok = true;
    for (int i = 0; move2std[i] != EOM; i++) {
        if (std2move[move2std[i]] != i) { rmap2_ok = false; break; }
    }
    for (int i = 0; move3std[i] != EOM; i++) {
        if (std3move[move3std[i]] != i) { rmap3_ok = false; break; }
    }
    EXPECT(rmap2_ok, "std2move is inverse of move2std");
    EXPECT(rmap3_ok, "std3move is inverse of move3std");

    /* Phase 3 set must not contain wide quarter-turns (only ×2) */
    bool no_wide_qt = true;
    for (int i = 0; move3std[i] != EOM; i++) {
        int m = move3std[i];
        if (m >= 18) {  /* wide move */
            int power = m % 3;  /* 0=CW,1=half,2=CCW */
            if (power != 1) { no_wide_qt = false; break; }
        }
    }
    EXPECT(no_wide_qt, "Phase 3 wide moves are half-turns only");
}

/* =========================================================================
 * Unit tests — cubie: identity, move order, inverse, parity
 * ========================================================================= */

static void ut_cubie_identity(void) {
    printf(C_BOLD "--- cubie: identity ---" C_RESET "\n");

    /* CenterCube identity */
    CenterCube ct;
    center_cube_identity(&ct);
    bool ct_ok = true;
    for (int i = 0; i < 24; i++) ct_ok &= ((int)ct.ct[i] == SOLVED_CT[i]);
    EXPECT(ct_ok, "CenterCube identity: ct[i] == expected face color");

    /* EdgeCube identity: ep[i] == i */
    EdgeCube ep;
    edge_cube_identity(&ep);
    bool ep_ok = true;
    for (int i = 0; i < 24; i++) ep_ok &= (ep.ep[i] == (uint8_t)i);
    EXPECT(ep_ok, "EdgeCube identity: ep[i] == i");

    /* CornerCube identity */
    CornerCube cp;
    corner_cube_identity(&cp);
    bool cp_ok = true, co_ok = true;
    for (int i = 0; i < 8; i++) {
        cp_ok &= (cp.cp[i] == (uint8_t)i);
        co_ok &= (cp.co[i] == 0);
    }
    EXPECT(cp_ok, "CornerCube identity: cp[i] == i");
    EXPECT(co_ok, "CornerCube identity: co[i] == 0");

    /* FullCube identity */
    FullCube fc;
    fullcube_identity(&fc);
    EXPECT(fc.moveLength == 0, "FullCube identity: moveLength == 0");
    EXPECT(goal_solved(&fc),   "FullCube identity passes goal_solved()");
}

static void ut_cubie_move_order(void) {
    printf(C_BOLD "--- cubie: move order (X^n == I) ---" C_RESET "\n");

    /* Quarter-turn order-4 test for each face, outer and wide */
    struct { int m; const char *name; } qt_moves[] = {
        {Ux1,"U"}, {Rx1,"R"}, {Fx1,"F"}, {Dx1,"D"}, {Lx1,"L"}, {Bx1,"B"},
        {ux1,"u"}, {rx1,"r"}, {fx1,"f"}, {dx1,"d"}, {lx1,"l"}, {bx1,"b"},
    };
    int n_qt = (int)(sizeof(qt_moves)/sizeof(qt_moves[0]));

    for (int t = 0; t < n_qt; t++) {
        int m = qt_moves[t].m;
        char msg[64];

        /* CenterCube: m^4 == I */
        CenterCube ct; center_cube_identity(&ct);
        for (int k = 0; k < 4; k++) center_cube_move(&ct, m);
        bool ct_ok = true;
        for (int i = 0; i < 24; i++) ct_ok &= ((int)ct.ct[i] == SOLVED_CT[i]);
        snprintf(msg, sizeof(msg), "CenterCube: %s^4 == I", qt_moves[t].name);
        EXPECT(ct_ok, msg);

        /* EdgeCube: m^4 == I */
        EdgeCube ep; edge_cube_identity(&ep);
        for (int k = 0; k < 4; k++) edge_cube_move(&ep, m);
        bool ep_ok = true;
        for (int i = 0; i < 24; i++) ep_ok &= (ep.ep[i] == (uint8_t)i);
        snprintf(msg, sizeof(msg), "EdgeCube:   %s^4 == I", qt_moves[t].name);
        EXPECT(ep_ok, msg);

        /* CornerCube: m^4 == I  (wide moves don't affect corners, but m%18 does) */
        CornerCube cp; corner_cube_identity(&cp);
        for (int k = 0; k < 4; k++) corner_cube_move(&cp, m);
        bool cp_ok = true;
        for (int i = 0; i < 8; i++) {
            cp_ok &= (cp.cp[i] == (uint8_t)i) && (cp.co[i] == 0);
        }
        snprintf(msg, sizeof(msg), "CornerCube: %s^4 == I", qt_moves[t].name);
        EXPECT(cp_ok, msg);
    }

    /* Half-turn order-2 test */
    struct { int m; const char *name; } ht_moves[] = {
        {Ux2,"U2"},{Rx2,"R2"},{Fx2,"F2"},{Dx2,"D2"},{Lx2,"L2"},{Bx2,"B2"},
        {ux2,"u2"},{rx2,"r2"},{fx2,"f2"},{dx2,"d2"},{lx2,"l2"},{bx2,"b2"},
    };
    int n_ht = (int)(sizeof(ht_moves)/sizeof(ht_moves[0]));

    for (int t = 0; t < n_ht; t++) {
        int m = ht_moves[t].m;
        char msg[64];

        CenterCube ct; center_cube_identity(&ct);
        for (int k = 0; k < 2; k++) center_cube_move(&ct, m);
        bool ct_ok = true;
        for (int i = 0; i < 24; i++) ct_ok &= ((int)ct.ct[i] == SOLVED_CT[i]);
        snprintf(msg, sizeof(msg), "CenterCube: %s^2 == I", ht_moves[t].name);
        EXPECT(ct_ok, msg);
    }

    /* Commutator [U,R]^6 == I on CenterCube */
    CenterCube ct; center_cube_identity(&ct);
    for (int k = 0; k < 6; k++) {
        center_cube_move(&ct, Ux1);
        center_cube_move(&ct, Rx1);
        center_cube_move(&ct, Ux3);
        center_cube_move(&ct, Rx3);
    }
    bool comm_ok = true;
    for (int i = 0; i < 24; i++) comm_ok &= ((int)ct.ct[i] == SOLVED_CT[i]);
    EXPECT(comm_ok, "CenterCube: [U,R]^6 == I");
}

static void ut_cubie_inverse(void) {
    printf(C_BOLD "--- cubie: move * inverse == I ---" C_RESET "\n");

    struct { int m, mi; const char *name; } pairs[] = {
        {Ux1,Ux3,"U/U'"}, {Rx1,Rx3,"R/R'"}, {Fx1,Fx3,"F/F'"},
        {Dx1,Dx3,"D/D'"}, {Lx1,Lx3,"L/L'"}, {Bx1,Bx3,"B/B'"},
        {ux1,ux3,"u/u'"}, {rx1,rx3,"r/r'"}, {lx1,lx3,"l/l'"},
        {Ux2,Ux2,"U2^2"}, {Rx2,Rx2,"R2^2"}, {fx2,fx2,"f2^2"},
    };
    int n = (int)(sizeof(pairs)/sizeof(pairs[0]));

    bool ct_ok = true, ep_ok = true, cp_ok = true;
    char fail_ct[64]="", fail_ep[64]="", fail_cp[64]="";

    for (int t = 0; t < n; t++) {
        int m = pairs[t].m, mi = pairs[t].mi;

        CenterCube ct; center_cube_identity(&ct);
        center_cube_move(&ct, m);
        center_cube_move(&ct, mi);
        for (int i = 0; i < 24; i++) {
            if ((int)ct.ct[i] != SOLVED_CT[i]) {
                ct_ok = false;
                snprintf(fail_ct, sizeof(fail_ct), "CenterCube: %s", pairs[t].name);
            }
        }

        EdgeCube ep; edge_cube_identity(&ep);
        edge_cube_move(&ep, m);
        edge_cube_move(&ep, mi);
        for (int i = 0; i < 24; i++) {
            if (ep.ep[i] != (uint8_t)i) {
                ep_ok = false;
                snprintf(fail_ep, sizeof(fail_ep), "EdgeCube:   %s", pairs[t].name);
            }
        }

        CornerCube cp; corner_cube_identity(&cp);
        corner_cube_move(&cp, m);
        corner_cube_move(&cp, mi);
        for (int i = 0; i < 8; i++) {
            if (cp.cp[i] != (uint8_t)i || cp.co[i] != 0) {
                cp_ok = false;
                snprintf(fail_cp, sizeof(fail_cp), "CornerCube: %s", pairs[t].name);
            }
        }
    }

    if (ct_ok) g_pass++;
    else { g_fail++; printf("  " C_RED "FAIL" C_RESET " %s\n", fail_ct); }

    if (ep_ok) g_pass++;
    else { g_fail++; printf("  " C_RED "FAIL" C_RESET " %s\n", fail_ep); }

    if (cp_ok) g_pass++;
    else { g_fail++; printf("  " C_RED "FAIL" C_RESET " %s\n", fail_cp); }
}

static void ut_cubie_parity(void) {
    printf(C_BOLD "--- cubie: parity ---" C_RESET "\n");

    EdgeCube ep;   edge_cube_identity(&ep);
    CornerCube cp; corner_cube_identity(&cp);

    EXPECT_EQ(edge_cube_parity(&ep),   0, "EdgeCube:   solved parity == 0 (even)");
    EXPECT_EQ(corner_cube_parity(&cp), 0, "CornerCube: solved parity == 0 (even)");

    edge_cube_move(&ep, Ux1);
    EXPECT_EQ(edge_cube_parity(&ep), 1, "EdgeCube:   after U → parity 1 (odd)");

    corner_cube_move(&cp, Ux1);
    EXPECT_EQ(corner_cube_parity(&cp), 1, "CornerCube: after U → parity 1 (odd)");

    /* After two moves from different axes, parity returns to even. */
    edge_cube_move(&ep, Rx1);
    EXPECT_EQ(edge_cube_parity(&ep), 0, "EdgeCube:   after U,R → parity 0 (even)");

    corner_cube_move(&cp, Rx1);
    EXPECT_EQ(corner_cube_parity(&cp), 0, "CornerCube: after U,R → parity 0 (even)");

    /* Single half-turn doesn't change edge parity (two 4-cycles). */
    EdgeCube ep2; edge_cube_identity(&ep2);
    edge_cube_move(&ep2, Rx2);
    EXPECT_EQ(edge_cube_parity(&ep2), 0, "EdgeCube:   after R2 → parity 0 (even)");
}

static void ut_cubie_fullcube(void) {
    printf(C_BOLD "--- cubie: FullCube lazy buffering ---" C_RESET "\n");

    /* After fullcube_move (lazy), sub-cubes are unchanged until flushed. */
    FullCube fc; fullcube_identity(&fc);
    fullcube_move(&fc, Rx1);
    EXPECT_EQ(fc.moveLength,   1, "fullcube_move: moveLength increments");
    EXPECT_EQ(fc.edgeAvail,    0, "fullcube_move: edgeAvail not incremented");
    EXPECT_EQ(fc.centerAvail,  0, "fullcube_move: centerAvail not incremented");

    /* After get_edge, edgeAvail catches up. */
    fullcube_get_edge(&fc);
    EXPECT_EQ(fc.edgeAvail, 1, "fullcube_get_edge: edgeAvail caught up");

    /* fullcube_do_move flushes immediately. */
    fullcube_identity(&fc);
    fullcube_do_move(&fc, Ux1);
    EXPECT_EQ(fc.edgeAvail,    1, "fullcube_do_move: edgeAvail == 1");
    EXPECT_EQ(fc.centerAvail,  1, "fullcube_do_move: centerAvail == 1");
    EXPECT_EQ(fc.cornerAvail,  1, "fullcube_do_move: cornerAvail == 1");

    /* fullcube_from_moves: scramble + inverse returns to identity */
    int scramble[] = {Rx1, Ux1, Rx3, Ux3};
    fullcube_from_moves(&fc, scramble, 4);
    int inv[] = {Ux1, Rx1, Ux3, Rx3};
    for (int i = 0; i < 4; i++) fullcube_do_move(&fc, inv[i]);
    EXPECT(goal_solved(&fc), "fullcube_from_moves: scramble * inverse == I");
}

/* =========================================================================
 * Coordinate unit tests (validate once implementations exist)
 * ========================================================================= */

static void ut_coords(void) {
    printf(C_BOLD "--- coordinates ---" C_RESET "\n");

    /* Solved FullCube should give 0 for all phase coordinates. */
    FullCube fc; fullcube_identity(&fc);
    CenterCube *ct = fullcube_get_center(&fc);
    EdgeCube   *ep = fullcube_get_edge(&fc);

    /* center1_get on solved cube: all 8 U/D centers are in 0-7, gives
     * a specific raw coordinate value.  After sym-reduction it should map
     * to a solved class (csprun==0). */
    int raw1 = center1_get(ct->ct);
    EXPECT(raw1 >= 0 && raw1 < CENTER1_RAW_COORDS,
           "center1_get(solved) in valid range");

    int rl = center2_get_rl(ct->ct + 8);  /* equatorial slice */
    EXPECT(rl >= 0 && rl < CENTER2_RL_COORDS,
           "center2_get_rl(solved) in valid range");

    int ctv = center2_get_ct(ct->ct + 8);
    EXPECT(ctv >= 0 && ctv < CENTER2_CT_COORDS,
           "center2_get_ct(solved) in valid range");

    int su = center3_get_slice_u(ct->ct);
    int sr = center3_get_slice_r(ct->ct);
    int sf = center3_get_slice_f(ct->ct);
    EXPECT(su >= 0 && su < CENTER3_SLICE_COORDS, "center3 sliceU in range");
    EXPECT(sr >= 0 && sr < CENTER3_SLICE_COORDS, "center3 sliceR in range");
    EXPECT(sf >= 0 && sf < CENTER3_SLICE_COORDS, "center3 sliceF in range");

    int ep_raw = edge3_get_raw(ep->ep);
    EXPECT(ep_raw >= 0 && ep_raw < EDGE3_RAW_PERMS,
           "edge3_get_raw(solved) in valid range");

    /* A single move should change the Center1 coordinate. */
    FullCube fc2; fullcube_identity(&fc2);
    fullcube_do_move(&fc2, Rx1);
    CenterCube *ct2 = fullcube_get_center(&fc2);
    int raw1_after = center1_get(ct2->ct);
    EXPECT(raw1_after != raw1, "center1_get changes after R move");

    /* Phase goal validators on solved cube */
    EXPECT(goal_p1(ct),          "goal_p1: solved cube passes");
    EXPECT(goal_p2(ct),          "goal_p2: solved cube passes");
    EXPECT(goal_p3_centers(ct),  "goal_p3_centers: solved cube passes");
    EXPECT(goal_p3_edges(ep),    "goal_p3_edges: solved cube passes");
}

/* =========================================================================
 * Scramble generator  (simple LCG — portable, no rand() state issues)
 * ========================================================================= */

static int lcg(unsigned int *s) {
    *s = *s * 1664525u + 1013904223u;
    return (int)((*s >> 16) & 0x7fff);
}

static void gen_scramble(int *moves, int n, unsigned int *seed) {
    int prev = 36;   /* sentinel: no previous move */
    int i = 0;
    while (i < n) {
        int m = lcg(seed) % 36;
        if (prev < 36 && ckmv[prev][m]) continue;
        moves[i++] = m;
        prev = m;
    }
}

static void fmt_scramble(const int *moves, int n, char *buf, int buf_len) {
    int pos = 0;
    int show = n > 10 ? 10 : n;
    for (int i = 0; i < show && pos < buf_len - 8; i++) {
        int w = snprintf(buf+pos, (size_t)(buf_len-pos), "%s ", move2str[moves[i]]);
        if (w > 0) pos += w;
    }
    if (n > show) snprintf(buf+pos, (size_t)(buf_len-pos), "(+%d)", n-show);
    else if (pos > 0) buf[pos-1] = '\0';
}

/* =========================================================================
 * Pipeline stage enumeration
 * ========================================================================= */

typedef enum {
    ST_FAIL   = 0,   /* search returned 0 solutions (unimplemented or error) */
    ST_P1     = 1,   /* Phase 1 goal validated */
    ST_P2     = 2,   /* Phase 2 goal validated */
    ST_P3     = 3,   /* Phase 3 goal validated (centers + edges) */
    ST_SOLVED = 4,   /* Fully solved (all sub-cubes identity) */
} Stage;

static const char *STAGE_TAG[] = {
    C_RED    "FAIL  " C_RESET,
    C_YELLOW "P1 OK " C_RESET,
    C_YELLOW "P2 OK " C_RESET,
    C_GREEN  "P3 OK " C_RESET,
    C_GREEN  "SOLVED" C_RESET,
};

/* =========================================================================
 * Pipeline runner
 *
 * Takes a scrambled FullCube, runs all three search phases, validates each
 * phase goal independently, and returns the furthest stage reached.
 * ========================================================================= */

/* Static beam storage — avoid large VLAs on the stack. */
static FullCube s_p1[SEARCH_BEAM1_MAX];
static FullCube s_p2[SEARCH_BEAM2_MAX];
static FullCube s_p3;

typedef struct {
    Stage stage;
    int   n1, n2, n3;      /* solution counts at each phase  */
    int   len1, len2, len3; /* best solution lengths           */
    bool  goal1_ok;         /* goal validated (not just found) */
    bool  goal2_ok;
    bool  goal3c_ok;        /* Phase 3 centers                */
    bool  goal3e_ok;        /* Phase 3 edges                  */
    bool  sol_verified;     /* independent verification passed */
} PipelineResult;

static PipelineResult run_pipeline(const FullCube *scrambled) {
    PipelineResult r;
    memset(&r, 0, sizeof(r));

    /* ---- Phase 1 ---- */
    r.n1 = search1(scrambled, s_p1, SEARCH_BEAM1_MAX);
    if (r.n1 == 0) return r;

    CenterCube *ct1 = fullcube_get_center(&s_p1[0]);
    r.goal1_ok = goal_p1(ct1);
    r.len1 = s_p1[0].length1;
    if (!r.goal1_ok) return r;
    r.stage = ST_P1;

    /* ---- Phase 2 ---- */
    r.n2 = search2(s_p1, r.n1, s_p2, SEARCH_BEAM2_MAX);
    if (r.n2 == 0) return r;

    CenterCube *ct2 = fullcube_get_center(&s_p2[0]);
    r.goal2_ok = goal_p2(ct2);
    r.len2 = s_p2[0].length2;
    if (!r.goal2_ok) return r;
    r.stage = ST_P2;

    /* ---- Phase 3 ---- */
    r.n3 = search3(s_p2, r.n2, &s_p3);
    if (r.n3 == 0) return r;

    CenterCube *ct3 = fullcube_get_center(&s_p3);
    EdgeCube   *ep3 = fullcube_get_edge(&s_p3);
    r.goal3c_ok = goal_p3_centers(ct3);
    r.goal3e_ok = goal_p3_edges(ep3);
    r.len3 = s_p3.length3;
    if (!r.goal3c_ok || !r.goal3e_ok) return r;
    r.stage = ST_P3;

    /* ---- Full solve check ---- */
    if (!goal_solved(&s_p3)) return r;
    r.stage = ST_SOLVED;

    /* ---- Independent solution verification ----
     * Rebuild the scrambled cube from its buffered moves, apply the
     * solution portion on top, and check the goal again. */
    {
        int scr_len = scrambled->moveLength;
        int sol_len = s_p3.moveLength - scr_len;
        if (sol_len >= 0 && sol_len < FULLCUBE_MOVE_BUF) {
            FullCube verify;
            fullcube_from_moves(&verify,
                                (const int *)(const void *)s_p3.moveBuffer,
                                scr_len);
            for (int i = 0; i < sol_len; i++)
                fullcube_do_move(&verify, s_p3.moveBuffer[scr_len + i]);
            r.sol_verified = goal_solved(&verify);
        }
    }

    return r;
}

/* =========================================================================
 * Integration test runner
 * ========================================================================= */

static void integration_tests(bool verbose) {
    printf("\n" C_BOLD "=== Integration: scramble → pipeline ===" C_RESET "\n\n");

    /* Seed from environment or current time */
    unsigned int seed = (unsigned int)time(NULL);
    const char *env_s = getenv("TPR_SEED");
    if (env_s) seed = (unsigned int)strtoul(env_s, NULL, 0);
    printf("  Seed: 0x%08X  (override with TPR_SEED=<hex>)\n\n", seed);

    /* Lengths: 5 scrambles of each of 5 length bands */
    static const int BANDS[] = {5, 10, 20, 30, 40};
    static const int PER_BAND = 5;
    int n_bands = (int)(sizeof(BANDS)/sizeof(BANDS[0]));
    int n_total = n_bands * PER_BAND;

    /* Column header */
    printf("  %-5s %-5s | %-6s | %-5s %-5s %-5s | %s\n",
           "#", "len", "stage", "P1mv", "P2mv", "P3mv", "scramble");
    printf("  %-5s %-5s + %-6s + %-5s %-5s %-5s + %s\n",
           "-----","-----","------","-----","-----","-----",
           "-----------------------------------------");

    int by_stage[5] = {0};
    int goal_fail[5] = {0}; /* search found sol but goal invalid (bug) */

    for (int t = 0; t < n_total; t++) {
        int len = BANDS[t / PER_BAND];

        /* Generate scramble */
        int moves[50];
        gen_scramble(moves, len, &seed);

        char scr_str[128];
        fmt_scramble(moves, len, scr_str, sizeof(scr_str));

        /* Build scrambled cube */
        FullCube scrambled;
        fullcube_from_moves(&scrambled, moves, len);

        /* Run pipeline */
        PipelineResult r = run_pipeline(&scrambled);
        by_stage[(int)r.stage]++;

        /* Detect goal validation failures (search bug) */
        if (r.n1 > 0 && !r.goal1_ok) goal_fail[1]++;
        if (r.n2 > 0 && !r.goal2_ok) goal_fail[2]++;
        if (r.n3 > 0 && (!r.goal3c_ok || !r.goal3e_ok)) goal_fail[3]++;

        /* Format move counts */
        char mv1[8]="-", mv2[8]="-", mv3[8]="-";
        if (r.n1 > 0) snprintf(mv1, sizeof(mv1), "%d", r.len1);
        if (r.n2 > 0) snprintf(mv2, sizeof(mv2), "%d", r.len2);
        if (r.n3 > 0) snprintf(mv3, sizeof(mv3), "%d", r.len3);

        printf("  [%02d] len=%-2d | %s | %-5s %-5s %-5s | %s\n",
               t+1, len, STAGE_TAG[(int)r.stage],
               mv1, mv2, mv3, scr_str);

        if (verbose) {
            if (r.n1 > 0)
                printf("         P1: %d sols, best %d moves, goal=%s\n",
                       r.n1, r.len1, r.goal1_ok ? "OK" : C_RED "FAIL" C_RESET);
            if (r.n2 > 0)
                printf("         P2: %d sols, best %d moves, goal=%s\n",
                       r.n2, r.len2, r.goal2_ok ? "OK" : C_RED "FAIL" C_RESET);
            if (r.n3 > 0) {
                printf("         P3: found, %d moves, centers=%s edges=%s\n",
                       r.len3,
                       r.goal3c_ok ? "OK" : C_RED "FAIL" C_RESET,
                       r.goal3e_ok ? "OK" : C_RED "FAIL" C_RESET);
                if (r.stage == ST_SOLVED) {
                    char sol[512];
                    get_move_string(&s_p3, sol, sizeof(sol));
                    printf("         Sol: %s\n", sol);
                    printf("         Verified: %s\n",
                           r.sol_verified ? C_GREEN "YES" C_RESET
                                          : C_RED   "NO"  C_RESET);
                }
            }
        }
    }

    /* ---- Summary ---- */
    printf("\n" C_BOLD "  Summary (%d scrambles)" C_RESET "\n", n_total);
    printf("  ┌─────────────────────────────┬────────┐\n");
    printf("  │ Phase 1 goal reached        │ %3d/%d │\n",
           by_stage[1]+by_stage[2]+by_stage[3]+by_stage[4], n_total);
    printf("  │ Phase 2 goal reached        │ %3d/%d │\n",
           by_stage[2]+by_stage[3]+by_stage[4], n_total);
    printf("  │ Phase 3 goal reached        │ %3d/%d │\n",
           by_stage[3]+by_stage[4], n_total);
    printf("  │ Fully solved                │ %3d/%d │\n",
           by_stage[4], n_total);
    printf("  └─────────────────────────────┴────────┘\n");

    /* Goal-violation warnings (search found solutions that fail goal check) */
    for (int ph = 1; ph <= 3; ph++) {
        if (goal_fail[ph] > 0) {
            printf("  " C_RED "WARNING" C_RESET
                   ": Phase %d returned solutions that failed goal validation"
                   " (%d cases) — likely a search bug.\n", ph, goal_fail[ph]);
        }
    }
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(int argc, char **argv) {
    bool verbose = (argc > 1 && strcmp(argv[1], "-v") == 0);

    printf(C_BOLD "=== TPR Solver Test Suite ===" C_RESET "\n\n");

    /* Initialise all tables. */
    tpr_init();

    /* Unit tests */
    printf(C_BOLD "=== Unit Tests ===" C_RESET "\n\n");

    ut_tpr_util();
    printf("\n");
    ut_moves();
    printf("\n");
    ut_cubie_identity();
    printf("\n");
    ut_cubie_move_order();
    printf("\n");
    ut_cubie_inverse();
    printf("\n");
    ut_cubie_parity();
    printf("\n");
    ut_cubie_fullcube();
    printf("\n");
    ut_coords();

    printf("\n" C_BOLD "Unit tests: %d passed, %d failed" C_RESET "\n",
           g_pass, g_fail);

    /* Integration tests */
    integration_tests(verbose);

    printf("\n" C_BOLD "=== Done ===" C_RESET "\n");
    return (g_fail > 0) ? 1 : 0;
}
