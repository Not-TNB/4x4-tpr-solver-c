#include "../include/edge3.h"
#include <string.h>

int      e3sym2raw [EDGE3_SYM_CLASSES];
uint16_t e3symstate[EDGE3_SYM_CLASSES];
int      e3raw2sym [EDGE3_CORD1_MAX];

uint32_t eprun_packed[EDGE3_N_EPRUN / 16];

static inline int get_eprun_raw(int idx) {
    return (int)((eprun_packed[idx >> 4] >> ((idx & 0xf) << 1)) & 0x3u);
}

/* Only call when the 2-bit field at idx is currently 0x3 (unseen). */
static inline void set_eprun(int idx, int val) {
    eprun_packed[idx >> 4] ^= (uint32_t)(0x3 ^ val) << ((idx & 0xf) << 1);
}

uint8_t  e3mvrot [EDGE3_PHASE3_MOVES * EDGE3_SYM_COUNT][12];
uint8_t  e3mvroto[EDGE3_PHASE3_MOVES * EDGE3_SYM_COUNT][12];

uint8_t  e3cpos[EDGE3_PHASE3_MOVES][12];
uint8_t  e3cval[EDGE3_PHASE3_MOVES][12];

uint8_t  e3sym_min_prun[EDGE3_SYM_CLASSES];

/* half_fact[k] = k!/2  (half_fact[0]=half_fact[1]=1) */
static const int half_fact[13] = {
    1, 1, 1, 3, 12, 60, 360, 2520, 20160, 181440, 1814400, 19958400, 239500800
};

void edge3_set_from_int(Edge3State *s, int idx) {
    long long val = 0xba9876543210LL;
    int parity = 0;
    for (int i = 0; i < 11; i++) {
        int p = half_fact[11 - i];
        int v = idx / p;
        idx  %= p;
        parity ^= v;
        v <<= 2;
        s->edge[i] = (uint8_t)((val >> v) & 0xf);
        long long m = (1LL << v) - 1;
        val = (val & m) + ((val >> 4) & ~m);
    }
    if ((parity & 1) == 0) {
        s->edge[11] = (uint8_t)val;
    } else {
        s->edge[11] = s->edge[10];
        s->edge[10] = (uint8_t)val;
    }
    for (int i = 0; i < 12; i++) s->edgeo[i] = (uint8_t)i;
}

void edge3_std(Edge3State *s) {
    for (int i = 0; i < 12; i++)
        s->temp[s->edgeo[i]] = (uint8_t)i;
    for (int i = 0; i < 12; i++) {
        s->edge[i]  = s->temp[s->edge[i]];
        s->edgeo[i] = (uint8_t)i;
    }
}

/* end=4 -> cord1; end=10 -> %20160 gives cord2. */
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

/* circle(a,b,c,d): d<-c<-b<-a<-d  swap4(a,b,c,d): a↔c,b↔d  swap2(x,y): x↔y */

static void e3_circle(uint8_t *a, int p, int q, int r, int s) {
    int t = a[s]; a[s] = a[r]; a[r] = a[q]; a[q] = a[p]; a[p] = (uint8_t)t;
}
static void e3_swap4(uint8_t *a, int p, int q, int r, int s) {
    int t; t = a[p]; a[p] = a[r]; a[r] = (uint8_t)t; t = a[q]; a[q] = a[s]; a[s] = (uint8_t)t;
}
static void e3_swap2(uint8_t *a, int x, int y) {
    int t = a[x]; a[x] = a[y]; a[y] = (uint8_t)t;
}

void edge3_move(Edge3State *s, int p3m) {
    uint8_t *e = s->edge, *o = s->edgeo;
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

/* swapx(x,y): swap edge[x] with edgeo[y]. */
static void e3_swapx(uint8_t *edge, uint8_t *edgeo, int x, int y) {
    int t = edge[x]; edge[x] = edgeo[y]; edgeo[y] = (uint8_t)t;
}
/* circlex(a,b,c,d): edgeo[d]<-edge[c]<-edgeo[b]<-edge[a]<-edgeo[d]. */
static void e3_circlex(uint8_t *edge, uint8_t *edgeo, int a, int b, int c, int d) {
    int t = edgeo[d]; edgeo[d] = edge[c]; edge[c] = edgeo[b]; edgeo[b] = edge[a]; edge[a] = (uint8_t)t;
}

void edge3_rot(Edge3State *s, int r) {
    uint8_t *e = s->edge, *o = s->edgeo;
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

static const int FullEdgeMap[12] = {0, 2, 4, 6, 1, 3, 7, 5, 8, 9, 10, 11};

int edge3_set_from_edgecube(Edge3State *s, const uint8_t ep[24]) {
    for (int i = 0; i < 12; i++) {
        s->temp[i] = (uint8_t)i;
        s->edge[i] = (uint8_t)(ep[FullEdgeMap[i] + 12] % 12);
    }
    int parity = 1;  /* FullEdgeMap introduces one transposition */
    for (int i = 0; i < 12; i++) {
        while (s->edge[i] != i) {
            int t       = s->edge[i];
            s->edge[i]  = s->edge[t];
            s->edge[t]  = (uint8_t)t;
            int tmp     = s->temp[i];
            s->temp[i]  = s->temp[t];
            s->temp[t]  = (uint8_t)tmp;
            parity     ^= 1;
        }
    }
    for (int i = 0; i < 12; i++) s->edge[i] = s->temp[ep[FullEdgeMap[i]] % 12];
    for (int i = 0; i < 12; i++) s->edgeo[i] = (uint8_t)i;
    return parity;
}

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

int edge3_getmvrot(const uint8_t *ep, int mr_idx, int end) {
    const uint8_t *mov  = e3mvrot [mr_idx];
    const uint8_t *movo = e3mvroto[mr_idx];
    long long val = 0xba9876543210LL;
    int idx = 0;
    for (int i = 0; i < end; i++) {
        int v = movo[ep[mov[i]]] << 2;
        idx = idx * (12 - i) + (int)((val >> v) & 0xf);
        val -= 0x111111111110LL << v;
    }
    return idx;
}

int edge3_getprun(int edge_coord, int prun) {
    int depm3 = get_eprun_raw(edge_coord);
    if (depm3 == 0x3) return EDGE3_MAX_DEPTH;
    return (depm3 - prun + 16) % 3 + prun - 1;
}

/* Backward BFS trace: follow neighbors with decreasing depth%3 until we
 * reach the solved state (index 0), counting steps.  Called at most once
 * per Phase-3 candidate, so O(depth*17) coord work is negligible. */
int edge3_getprun_init(int edge_coord) {
    int depm3 = get_eprun_raw(edge_coord);
    if (depm3 == 0x3) return EDGE3_MAX_DEPTH;
    if (edge_coord == 0) return 0;

    Edge3State e;
    int cur   = edge_coord;
    int depth = 0;

    while (cur != 0) {
        int target = (depm3 == 0) ? 2 : depm3 - 1;

        int symcord1 = cur / EDGE3_RAW_PERMS;
        int cord1    = e3sym2raw[symcord1];
        int cord2    = cur % EDGE3_RAW_PERMS;
        edge3_set_from_int(&e, cord1 * EDGE3_RAW_PERMS + cord2);

        for (int m = 0; m < 17; m++) {
            int cord1x    = edge3_getmvrot(e.edge, m << 3, 4);
            int sym_raw   = e3raw2sym[cord1x];
            int symx      = sym_raw & 7;
            int symcord1x = sym_raw >> 3;
            int cord2x    = edge3_getmvrot(e.edge, (m << 3) | symx, 10) % EDGE3_RAW_PERMS;
            int idx       = symcord1x * EDGE3_RAW_PERMS + cord2x;

            if (get_eprun_raw(idx) == target) {
                depth++;
                cur   = idx;
                depm3 = target;
                break;
            }
        }
    }
    return depth;
}

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

/* Unified forward/backward BFS storing depth%3 in 2 bits.
 * Forward (depth 0-8):  scan entries at depm3, set unseen neighbors to dep1m3.
 * Backward (depth 9+):  scan unseen entries, set those adjacent to a dep-depth
 *                       neighbor (stored == depm3) to dep1m3. */
void edge3_create_prun(void) {
    memset(eprun_packed, 0xFF, sizeof(eprun_packed));
    set_eprun(0, 0);
    int done = 1;

    memset(e3sym_min_prun, 0xFF, sizeof(e3sym_min_prun));
    e3sym_min_prun[0] = 0;

    Edge3State e3, f3, g3;

    for (int depth = 0; done < EDGE3_N_EPRUN; depth++) {
        int depm3  = depth % 3;
        int dep1m3 = (depth + 1) % 3;
        int inv    = (depth >= 9);
        int find   = inv ? 0x3 : depm3;
        int chk    = inv ? depm3 : 0x3;

        for (int i_ = 0; i_ < EDGE3_N_EPRUN; i_ += 16) {
            uint32_t word = eprun_packed[i_ >> 4];
            /* Forward pass: skip words with no set entries (all unseen). */
            if (!inv && word == 0xFFFFFFFFu) continue;

            for (int i = i_; i < i_ + 16; i++) {
                if ((int)((word >> ((i & 0xf) << 1)) & 0x3u) != find) continue;

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

                    if (get_eprun_raw(idx) != chk) continue;

                    if (inv) {
                        /* Backward: mark the unseen entry i whose neighbor is at depth. */
                        set_eprun(i, dep1m3);
                        done++;
                        if ((uint8_t)(depth + 1) < e3sym_min_prun[i / EDGE3_RAW_PERMS])
                            e3sym_min_prun[i / EDGE3_RAW_PERMS] = (uint8_t)(depth + 1);
                        break;
                    }

                    /* Forward: mark the unseen neighbor idx. */
                    set_eprun(idx, dep1m3);
                    done++;
                    if ((uint8_t)(depth + 1) < e3sym_min_prun[symcord1x])
                        e3sym_min_prun[symcord1x] = (uint8_t)(depth + 1);

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
                        if (get_eprun_raw(idxx) == 0x3) {
                            set_eprun(idxx, dep1m3);
                            done++;
                        }
                    }
                }
            }
        }
    }
}

void edge3_init_combined(void) {
    for (int mi = 0; mi < EDGE3_PHASE3_MOVES; mi++) {
        int base = mi * EDGE3_SYM_COUNT;
        for (int k = 0; k < 12; k++) {
            e3cpos[mi][k] = e3mvrot[base][k];
            e3cval[mi][k] = e3mvroto[base][k];
        }
    }
}

void edge3_init(void) {
    edge3_init_mvrot();
    edge3_init_combined();
    edge3_init_sym2raw();
    edge3_create_prun();
}
