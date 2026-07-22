#include "../include/center1.h"
#include "../include/tpr_util.h"
#include "../include/moves.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */

int      raw2sym[CENTER1_RAW_COORDS];
int      ctsmv  [CENTER1_SYM_CLASSES][36];
int      sym2raw [CENTER1_SYM_CLASSES];
uint8_t  csprun  [CENTER1_SYM_CLASSES];
uint8_t  symmult [CENTER1_SYM_COUNT][CENTER1_SYM_COUNT];
uint8_t  symmove [CENTER1_SYM_COUNT][36];
uint8_t  syminv  [CENTER1_SYM_COUNT];
int      finish  [CENTER1_SYM_COUNT];

/* Init helpers -- ct[24] used as binary or identity permutation. */

static const uint8_t ct_mv_cyc[12][3][4] = {
    /* U  */ {{ 0, 1, 2, 3}},
    /* R  */ {{16,17,18,19}},
    /* F  */ {{ 8, 9,10,11}},
    /* D  */ {{ 4, 5, 6, 7}},
    /* L  */ {{20,21,22,23}},
    /* B  */ {{12,13,14,15}},
    /* Uw */ {{ 0, 1, 2, 3}, { 8,20,12,16}, { 9,21,13,17}},
    /* Rw */ {{16,17,18,19}, { 1,15, 5, 9}, { 2,12, 6,10}},
    /* Fw */ {{ 8, 9,10,11}, { 2,19, 4,21}, { 3,16, 5,22}},
    /* Dw */ {{ 4, 5, 6, 7}, {10,18,14,22}, {11,19,15,23}},
    /* Lw */ {{20,21,22,23}, { 0, 8, 4,14}, { 3,11, 7,13}},
    /* Bw */ {{12,13,14,15}, { 1,20, 7,18}, { 0,23, 6,17}},
};
static const int ct_mv_n[12] = {1,1,1,1,1,1, 3,3,3,3,3,3};

static void ct1_move(uint8_t ct[24], int m) {
    const int axis = m / 3, key = m % 3;
    for (int i = 0; i < ct_mv_n[axis]; i++) {
        const uint8_t *cyc = ct_mv_cyc[axis][i];
        swap4_u8(ct, cyc[0], cyc[1], cyc[2], cyc[3], key);
    }
}

/* 4 primitive cube rotations */
static void ct1_rot(uint8_t ct[24], int r) {
    switch (r) {
    case 0: ct1_move(ct, Uwx2); ct1_move(ct, Dwx2); break;
    case 1: ct1_move(ct, Rwx1); ct1_move(ct, Lwx3); break;
    case 2:
        swap4_u8(ct,  0,  3,  1,  2, 1);
        swap4_u8(ct,  8, 11,  9, 10, 1);
        swap4_u8(ct,  4,  7,  5,  6, 1);
        swap4_u8(ct, 12, 15, 13, 14, 1);
        swap4_u8(ct, 16, 19, 21, 22, 1);
        swap4_u8(ct, 17, 18, 20, 23, 1);
        break;
    case 3: ct1_move(ct, Uwx1); ct1_move(ct, Dwx3); ct1_move(ct, Fwx1); ct1_move(ct, Bwx3); break;
    }
}

/* Apply the k-th symmetry element (k steps of the epilogue sequence). */
static void ct1_rotate(uint8_t ct[24], int k) {
    for (int j = 0; j < k; j++) {
        ct1_rot(ct, 0);
        if (j%2==1)  ct1_rot(ct, 1);
        if (j%8==7)  ct1_rot(ct, 2);
        if (j%16==15) ct1_rot(ct, 3);
    }
}

static int ct1_get(const uint8_t ct[24]) {
    int idx = 0, r = 8;
    for (int i = 23; i >= 0; i--)
        if (ct[i] == 1) idx += Cnk[i][r--];
    return idx;
}

static void ct1_set(uint8_t ct[24], int idx) {
    int r = 8;
    for (int i = 23; i >= 0; i--) {
        ct[i] = 0;
        if (idx >= Cnk[i][r]) {
            idx -= Cnk[i][r--];
            ct[i] = 1;
        }
    }
}

/* Raw coordinate: C(24,8) -- which 8 positions hold a U/D sticker. */

int center1_raw_urf(const uint8_t ct[24], int urf) {
    int rank = 0, r = 8;
    for (int i = 23; i >= 0; i--)
        if (ct[i] % 3 == urf) rank += Cnk[i][r--];
    return rank;
}

int center1_get(const uint8_t ct[24]) {
    int rank = 0, r = 8;
    for (int i = 23; i >= 0; i--)
        if (ct[i] == 0 || ct[i] == 3) rank += Cnk[i][r--];
    return rank;
}

/* -------------------------------------------------------------------------
 * Symmetry group tables (48-element, 4 primitive rotations):
 *   symmult[i][j]=k   : sym[k] = sym[i] . sym[j]
 *   syminv[i]=j       : sym[j] = sym[i]^{-1}
 *   symmove[i][j]=k   : sym[i] . move_j . sym[syminv[i]] = move_k
 *   finish[i]         : raw coord of solved state under sym[i]
 * ------------------------------------------------------------------------- */
void center1_init_sym(void) {
    uint8_t c[24], d[24], e[24], f[24];
    for (int i = 0; i < 24; i++) c[i] = d[i] = e[i] = f[i] = (uint8_t)i;

    /* d steps through all 48 elems; c tracks sym[i].sym[j] */
    for (int i = 0; i < 48; i++) {
        for (int j = 0; j < 48; j++) {
            for (int k = 0; k < 48; k++) {
                if (memcmp(c, d, 24) == 0) {
                    symmult[i][j] = (uint8_t)k;
                    if (k == 0) syminv[i] = (uint8_t)j;
                }
                ct1_rot(d, 0);
                if (k%2==1)  ct1_rot(d, 1);
                if (k%8==7)  ct1_rot(d, 2);
                if (k%16==15) ct1_rot(d, 3);
            }
            ct1_rot(c, 0);
            if (j%2==1)  ct1_rot(c, 1);
            if (j%8==7)  ct1_rot(c, 2);
            if (j%16==15) ct1_rot(c, 3);
        }
        ct1_rot(c, 0);
        if (i%2==1)  ct1_rot(c, 1);
        if (i%8==7)  ct1_rot(c, 2);
        if (i%16==15) ct1_rot(c, 3);
    }

    /* Conjugation: find k such that sym[i] . move_j . sym[syminv[i]] = move_k. */
    for (int i = 0; i < 48; i++) {
        memcpy(c, e, 24);
        ct1_rotate(c, syminv[i]);
        for (int j = 0; j < 36; j++) {
            memcpy(d, c, 24);
            ct1_move(d, j);
            ct1_rotate(d, i);
            for (int k = 0; k < 36; k++) {
                memcpy(f, e, 24);
                ct1_move(f, k);
                if (memcmp(f, d, 24) == 0) {
                    symmove[i][j] = (uint8_t)k;
                    break;
                }
            }
        }
    }

    /* finish[syminv[i]] = raw coord of the solved state after applying sym[i]. */
    uint8_t ct[24];
    ct1_set(ct, 0);
    for (int i = 0; i < 48; i++) {
        finish[syminv[i]] = ct1_get(ct);
        ct1_rot(ct, 0);
        if (i%2==1)  ct1_rot(ct, 1);
        if (i%8==7)  ct1_rot(ct, 2);
        if (i%16==15) ct1_rot(ct, 3);
    }
}

/* BFS: raw2sym[raw]=(class<<6)|syminv[j], sym2raw[class]=rep. */
void center1_init_sym2raw(void) {
    uint32_t occ[(CENTER1_RAW_COORDS + 31) / 32];
    memset(occ, 0, sizeof(occ));
    memset(raw2sym, 0, sizeof(raw2sym));

    uint8_t ct[24];
    int count = 0;

    for (int i = 0; i < CENTER1_RAW_COORDS; i++) {
        if (occ[i >> 5] & (1u << (i & 0x1f))) continue;
        ct1_set(ct, i);
        for (int j = 0; j < 48; j++) {
            int idx = ct1_get(ct);
            occ[idx >> 5] |= (1u << (idx & 0x1f));
            raw2sym[idx] = (count << 6) | syminv[j];
            ct1_rot(ct, 0);
            if (j%2==1)  ct1_rot(ct, 1);
            if (j%8==7)  ct1_rot(ct, 2);
            if (j%16==15) ct1_rot(ct, 3);
        }
        sym2raw[count++] = i;
    }
}

/* ctsmv[class][move] = (new_class<<6)|new_sym. */
void center1_create_move_table(void) {
    uint8_t ct[24], d[24];
    for (int i = 0; i < CENTER1_SYM_CLASSES; i++) {
        ct1_set(ct, sym2raw[i]);
        for (int m = 0; m < 36; m++) {
            memcpy(d, ct, 24);
            ct1_move(d, m);
            ctsmv[i][m] = raw2sym[ct1_get(d)];
        }
    }
}

/* pruning table; forward BFS depth≤4, backward beyond; 27-move set. */
void center1_create_prun(void) {
    memset(csprun, 0xFF, sizeof(csprun));
    csprun[0] = 0;
    int depth = 0, done = 1;

    while (done < CENTER1_SYM_CLASSES) {
        int inv     = depth > 4;
        uint8_t sel = inv ? 0xFF : (uint8_t)depth;
        uint8_t chk = inv ? (uint8_t)depth : 0xFF;
        depth++;
        for (int i = 0; i < CENTER1_SYM_CLASSES; i++) {
            if (csprun[i] != sel) continue;
            for (int m = 0; m < 27; m++) {
                int idx = ctsmv[i][m] >> 6;
                if (csprun[idx] != chk) continue;
                done++;
                if (inv) {
                    csprun[i] = (uint8_t)depth;
                    break;
                }
                csprun[idx] = (uint8_t)depth;
            }
        }
    }
}

void center1_init(void) {
    center1_init_sym();
    center1_init_sym2raw();
    center1_create_move_table();
    center1_create_prun();
}
