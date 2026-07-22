#include "../include/edge3.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */

int      e3sym2raw [EDGE3_SYM_CLASSES];
uint16_t e3symstate[EDGE3_SYM_CLASSES];
int      e3raw2sym [EDGE3_CORD1_MAX];

uint8_t  eprun[EDGE3_N_EPRUN];

int      e3mvrot [EDGE3_PHASE3_MOVES * EDGE3_SYM_COUNT][12];
int      e3mvroto[EDGE3_PHASE3_MOVES * EDGE3_SYM_COUNT][12];

/* factX[k] = k!/2  (factX[0]=factX[1]=1) */
static const int factX[13] = {
    1, 1, 1, 3, 12, 60, 360, 2520, 20160, 181440, 1814400, 19958400, 239500800
};

/* -------------------------------------------------------------------------
 * Decode raw combined index (cord1 * N_RAW + cord2) into s->edge[12].
 * Uses the packed-nibble half-factorial Lehmer decode.
 * Sets edgeo[i] = i (std state).
 * ------------------------------------------------------------------------- */
void edge3_set_from_int(Edge3State *s, int idx) {
    long long val = 0xba9876543210LL;
    int parity = 0;
    for (int i = 0; i < 11; i++) {
        int p = factX[11 - i];
        int v = idx / p;
        idx  %= p;
        parity ^= v;
        v <<= 2;
        s->edge[i] = (int)((val >> v) & 0xf);
        long long m = (1LL << v) - 1;
        val = (val & m) + ((val >> 4) & ~m);
    }
    if ((parity & 1) == 0) {
        s->edge[11] = (int)val;
    } else {
        s->edge[11] = s->edge[10];
        s->edge[10] = (int)val;
    }
    for (int i = 0; i < 12; i++) s->edgeo[i] = i;
}

/* -------------------------------------------------------------------------
 * Normalise via edgeo[]: build inverse of edgeo[] into s->temp[],
 * remap edge[i] = temp[edge[i]], reset edgeo[i] = i.
 * ------------------------------------------------------------------------- */
void edge3_std(Edge3State *s) {
    for (int i = 0; i < 12; i++)
        s->temp[s->edgeo[i]] = i;
    for (int i = 0; i < 12; i++) {
        s->edge[i]  = s->temp[s->edge[i]];
        s->edgeo[i] = i;
    }
}

/* -------------------------------------------------------------------------
 * Lehmer rank of first `end` elements of s->edge[].
 * Caller must have called std() first.
 * end=4  -> cord1 in [0, 11879]
 * end=10 -> combined index; %20160 gives cord2
 * ------------------------------------------------------------------------- */
int edge3_get(const Edge3State *s, int end) {
    long long val = 0xba9876543210LL;
    int idx = 0;
    for (int i = 0; i < end; i++) {
        int v = s->edge[i] << 2;
        idx = idx * (12 - i) + (int)((val >> v) & 0xf);
        val -= 0x111111111110LL << v;
    }
    return idx;
}

/* -------------------------------------------------------------------------
 * 20-case switch applying circle/swap to both edge[] and edgeo[].
 * circle(a,b,c,d): d<-c<-b<-a<-d  (CW)
 * swap4(a,b,c,d):  a↔c, b↔d
 * swap2(x,y):      x↔y
 * ------------------------------------------------------------------------- */

static void e3_circle(int *a, int p, int q, int r, int s) {
    int t = a[s]; a[s] = a[r]; a[r] = a[q]; a[q] = a[p]; a[p] = t;
}
static void e3_swap4(int *a, int p, int q, int r, int s) {
    int t; t = a[p]; a[p] = a[r]; a[r] = t; t = a[q]; a[q] = a[s]; a[s] = t;
}
static void e3_swap2(int *a, int x, int y) {
    int t = a[x]; a[x] = a[y]; a[y] = t;
}

void edge3_move(Edge3State *s, int p3m) {
    int *e = s->edge, *o = s->edgeo;
    switch (p3m) {
        case 0:  e3_circle(e,0,4,1,5);   e3_circle(o,0,4,1,5);  break; // U
        case 1:  e3_swap4 (e,0,4,1,5);   e3_swap4 (o,0,4,1,5);  break; // U2
        case 2:  e3_circle(e,0,5,1,4);   e3_circle(o,0,5,1,4);  break; // U'
        case 3:  e3_swap4 (e,5,10,6,11); e3_swap4(o,5,10,6,11); break; // R2
        case 4:  e3_circle(e,0,11,3,8);  e3_circle(o,0,11,3,8); break; // F
        case 5:  e3_swap4 (e,0,11,3,8);  e3_swap4 (o,0,11,3,8); break; // F2
        case 6:  e3_circle(e,0,8,3,11);  e3_circle(o,0,8,3,11); break; // F'
        case 7:  e3_circle(e,2,7,3,6);   e3_circle(o,2,7,3,6);  break; // D
        case 8:  e3_swap4 (e,2,7,3,6);   e3_swap4 (o,2,7,3,6);  break; // D2
        case 9:  e3_circle(e,2,6,3,7);   e3_circle(o,2,6,3,7);  break; // D'
        case 10: e3_swap4 (e,4,8,7,9);   e3_swap4 (o,4,8,7,9);  break; // L2
        case 11: e3_circle(e,1,9,2,10);  e3_circle(o,1,9,2,10); break; // B
        case 12: e3_swap4 (e,1,9,2,10);  e3_swap4 (o,1,9,2,10); break; // B2
        case 13: e3_circle(e,1,10,2,9);  e3_circle(o,1,10,2,9); break; // B'
        case 14: // Uw2
            e3_swap4(e,0,4,1,5); e3_swap4(o,0,4,1,5);
            e3_swap2(e,9,11);        e3_swap2(o,8,10);
            break;
        case 15: // Rw2
            e3_swap4(e,5,10,6,11); e3_swap4(o,5,10,6,11);
            e3_swap2(e,1,3);           e3_swap2(o,0,2);
            break;
        case 16: // Fw2
            e3_swap4(e,0,11,3,8); e3_swap4(o,0,11,3,8);
            e3_swap2(e,5,7);          e3_swap2(o,4,6);
            break;
        case 17: // Dw2
            e3_swap4(e,2,7,3,6); e3_swap4(o,2,7,3,6);
            e3_swap2(e,8,10);        e3_swap2(o,9,11);
            break;
        case 18: // Lw2
            e3_swap4(e,4,8,7,9); e3_swap4(o,4,8,7,9);
            e3_swap2(e,0,2);         e3_swap2(o,1,3);
            break;
        case 19: // Bw2
            e3_swap4(e,1,9,2,10); e3_swap4(o,1,9,2,10);
            e3_swap2(e,4,6);          e3_swap2(o,5,7);
            break;
    }
}

/* -------------------------------------------------------------------------
 * rot(0) = move(14); move(17)
 * rot(1) and rot(2) use cross-array circlex/swapx ops.
 * rotate(r): decompose r into pairs of rot(1)+rot(2) then optional rot(0).
 * ------------------------------------------------------------------------- */

/* swapx(x,y): swap edge[x] with edgeo[y]. */
static void e3_swapx(int *edge, int *edgeo, int x, int y) {
    int t = edge[x]; edge[x] = edgeo[y]; edgeo[y] = t;
}
/* circlex(a,b,c,d): edgeo[d]<-edge[c]<-edgeo[b]<-edge[a]<-edgeo[d]. */
static void e3_circlex(int *edge, int *edgeo, int a, int b, int c, int d) {
    int t = edgeo[d]; edgeo[d] = edge[c]; edge[c] = edgeo[b]; edgeo[b] = edge[a]; edge[a] = t;
}

void edge3_rot(Edge3State *s, int r) {
    int *e = s->edge, *o = s->edgeo;
    switch (r) {
        case 0:
            edge3_move(s, 14);
            edge3_move(s, 17);
            break;
        case 1:
            e3_circlex(e,o,11,5,10,6);
            e3_circlex(e,o, 5,10, 6,11);
            e3_circlex(e,o, 1, 2, 3, 0);
            e3_circlex(e,o, 4, 9, 7, 8);
            e3_circlex(e,o, 8, 4, 9, 7);
            e3_circlex(e,o, 0, 1, 2, 3);
            break;
        case 2:
            e3_swapx(e,o,4,5);  e3_swapx(e,o,5,4);
            e3_swapx(e,o,11,8); e3_swapx(e,o,8,11);
            e3_swapx(e,o,7,6);  e3_swapx(e,o,6,7);
            e3_swapx(e,o,9,10); e3_swapx(e,o,10,9);
            e3_swapx(e,o,1,1);  e3_swapx(e,o,0,0);
            e3_swapx(e,o,3,3);  e3_swapx(e,o,2,2);
            break;
    }
}

void edge3_rotate(Edge3State *s, int r) {
    while (r >= 2) {
        r -= 2;
        edge3_rot(s, 1);
        edge3_rot(s, 2);
    }
    if (r != 0) edge3_rot(s, 0);
}

/* Convert an EdgeCube ep[24] to Edge3State */
static const int FullEdgeMap[12] = {0, 2, 4, 6, 1, 3, 7, 5, 8, 9, 10, 11};

int edge3_set_from_edgecube(Edge3State *s, const uint8_t ep[24]) {
    for (int i = 0; i < 12; i++) {
        s->temp[i] = i;
        s->edge[i] = ep[FullEdgeMap[i] + 12] % 12;
    }
    int parity = 1;  /* FullEdgeMap introduces one transposition */
    for (int i = 0; i < 12; i++) {
        while (s->edge[i] != i) {
            int t       = s->edge[i];
            s->edge[i]  = s->edge[t];
            s->edge[t]  = t;
            int tmp     = s->temp[i];
            s->temp[i]  = s->temp[t];
            s->temp[t]  = tmp;
            parity     ^= 1;
        }
    }
    for (int i = 0; i < 12; i++) s->edge[i] = s->temp[ep[FullEdgeMap[i]] % 12];
    for (int i = 0; i < 12; i++) s->edgeo[i] = i;
    return parity;
}

/* -------------------------------------------------------------------------
 * For each of the 20 p3 moves × 8 rotations, simulate from identity,
 * record where each position maps (mvrot) and the value-relabelling
 * inverse permutation (mvroto) after std().
 * ------------------------------------------------------------------------- */
void edge3_init_mvrot(void) {
    Edge3State s;
    for (int m = 0; m < EDGE3_PHASE3_MOVES; m++) {
        for (int r = 0; r < EDGE3_SYM_COUNT; r++) {
            edge3_set_from_int(&s, 0);
            edge3_move(&s, m);
            edge3_rotate(&s, r);
            const int idx = m * EDGE3_SYM_COUNT + r;
            for (int i = 0; i < 12; i++) e3mvrot[idx][i] = s.edge[i];
            edge3_std(&s);
            for (int i = 0; i < 12; i++) e3mvroto[idx][i] = s.temp[i];
        }
    }
}

/* -------------------------------------------------------------------------
 * Compute Lehmer rank of ep[] after applying combined move+rotation mrIdx,
 * without modifying ep[]. Uses precomputed mvrot/mvroto tables.
 * mrIdx = move * EDGE3_SYM_COUNT + rotation.
 * end=4  -> cord1 (P(12,4) range)
 * end=10 -> combined rank; caller takes %N_RAW for cord2
 * ------------------------------------------------------------------------- */
int edge3_getmvrot(const int *ep, int mrIdx, int end) {
    long long val = 0xba9876543210LL;
    int idx = 0;
    for (int i = 0; i < end; i++) {
        int v = e3mvroto[mrIdx][ep[e3mvrot[mrIdx][i]]] << 2;
        idx = idx * (12 - i) + (int)((val >> v) & 0xf);
        val -= 0x111111111110LL << v;
    }
    return idx;
}

/* -------------------------------------------------------------------------
 * Pruning
 * ------------------------------------------------------------------------- */
int edge3_getprun(int edge_coord) {
    int v = eprun[edge_coord];
    return v == 255 ? 10 : v;
}

/* -------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * Enumerate all 11,880 cord1 values with 8 symmetry elements.
 * Builds e3sym2raw[1538], e3raw2sym[11880], e3symstate[1538].
 * Rotation sequence: rot(0) after each j; additionally rot(1)+rot(2) on odd j.
 * syminv = {0,1,6,3,4,5,2,7}.
 * ------------------------------------------------------------------------- */
void edge3_init_sym2raw(void) {
    static const int syminv[8] = {0, 1, 6, 3, 4, 5, 2, 7};
    Edge3State e;
    uint8_t occ[EDGE3_CORD1_MAX / 8];
    memset(occ, 0, sizeof(occ));
    memset(e3symstate, 0, sizeof(e3symstate));
    int count = 0;

    for (int i = 0; i < EDGE3_CORD1_MAX; i++) {
        if (occ[i >> 3] & (1 << (i & 7))) continue;

        edge3_set_from_int(&e, i * EDGE3_RAW_PERMS);

        for (int j = 0; j < EDGE3_SYM_COUNT; j++) {
            edge3_std(&e);
            int idx = edge3_get(&e, 4);
            if (idx == i)
                e3symstate[count] |= (uint16_t)(1 << j);
            occ[idx >> 3] |= (uint8_t)(1 << (idx & 7));
            e3raw2sym[idx] = (count << 3) | syminv[j];

            edge3_rot(&e, 0);
            if (j % 2 == 1) {
                edge3_rot(&e, 1);
                edge3_rot(&e, 2);
            }
        }
        e3sym2raw[count++] = i;
    }
}

/* -------------------------------------------------------------------------
 * Forward BFS over the N_EPRUN = 1538*20160 = 31,006,080 sym×cord2 state space
 *
 * Symstate propagation: after setting idx, expand sym-equivalent cord2 values
 * within the same sym-class without additional move applications.
 * ------------------------------------------------------------------------- */
void edge3_create_prun(void) {
    memset(eprun, 0xFF, sizeof(eprun));  /* 0xFF = unseen */
    eprun[0] = 0;

    Edge3State e3, f3, g3;

    /* Forward */
    for (int depth = 0; depth < 9; depth++) {
        for (int i = 0; i < EDGE3_N_EPRUN; i++) {
            if (eprun[i] != (uint8_t)depth) continue;

            int symcord1 = i / EDGE3_RAW_PERMS;
            int cord1    = e3sym2raw[symcord1];
            int cord2    = i % EDGE3_RAW_PERMS;
            edge3_set_from_int(&e3, cord1 * EDGE3_RAW_PERMS + cord2);

            for (int m = 0; m < 17; m++) {
                int cord1x    = edge3_getmvrot(e3.edge, m << 3, 4);
                int sym_raw   = e3raw2sym[cord1x];
                int symx      = sym_raw & 7;
                int symcord1x = sym_raw >> 3;
                int cord2x    = edge3_getmvrot(e3.edge, (m << 3) | symx, 10) % EDGE3_RAW_PERMS;
                int idx       = symcord1x * EDGE3_RAW_PERMS + cord2x;

                if (eprun[idx] != 0xFF) continue;

                eprun[idx] = (uint8_t)(depth + 1);

                uint16_t ss = e3symstate[symcord1x];
                if (ss == 1) continue;

                memcpy(&f3, &e3, sizeof(Edge3State));
                edge3_move(&f3, m);
                edge3_rotate(&f3, symx);

                for (int j = 1; (ss >>= 1) != 0; j++) {
                    if (!(ss & 1)) continue;
                    memcpy(&g3, &f3, sizeof(Edge3State));
                    edge3_rotate(&g3, j);
                    edge3_std(&g3);
                    int idxx = symcord1x * EDGE3_RAW_PERMS
                               + edge3_get(&g3, 10) % EDGE3_RAW_PERMS;
                    if (eprun[idxx] == 0xFF)
                        eprun[idxx] = (uint8_t)(depth + 1);
                }
            }
        }
    }

    /* Backward */
    int changed;
    do {
        changed = 0;
        for (int i = 0; i < EDGE3_N_EPRUN; i++) {
            if (eprun[i] != 0xFF) continue;

            int symcord1 = i / EDGE3_RAW_PERMS;
            int cord1    = e3sym2raw[symcord1];
            int cord2    = i % EDGE3_RAW_PERMS;
            edge3_set_from_int(&e3, cord1 * EDGE3_RAW_PERMS + cord2);

            for (int m = 0; m < 17; m++) {
                int cord1x    = edge3_getmvrot(e3.edge, m << 3, 4);
                int sym_raw   = e3raw2sym[cord1x];
                int symx      = sym_raw & 7;
                int symcord1x = sym_raw >> 3;
                int cord2x    = edge3_getmvrot(e3.edge, (m << 3) | symx, 10) % EDGE3_RAW_PERMS;
                int idx       = symcord1x * EDGE3_RAW_PERMS + cord2x;

                int nval = (int)eprun[idx];
                if (nval == 0xFF) continue;

                eprun[i] = (uint8_t)(nval + 1);
                changed = 1;
                break;
            }
        }
    } while (changed);
}

void edge3_init(void) {
    edge3_init_mvrot();
    edge3_init_sym2raw();
    edge3_create_prun();
}

