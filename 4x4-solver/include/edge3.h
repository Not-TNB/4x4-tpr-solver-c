#ifndef EDGE3_H
#define EDGE3_H

#include <stdint.h>

#define EDGE3_SYM_CLASSES   1538
#define EDGE3_RAW_PERMS     20160   /* N_RAW = P(8,6) = 8!/2 */
#define EDGE3_CORD1_MAX     11880   /* cord1 range = P(12,4) */
#define EDGE3_PHASE3_MOVES  20
#define EDGE3_SYM_COUNT     8
#define EDGE3_N_EPRUN       (EDGE3_SYM_CLASSES * EDGE3_RAW_PERMS)

typedef struct {
    uint8_t edge[12];
    uint8_t edgeo[12];
    uint8_t temp[12];   /* scratch for std() and set_from_edgecube() */
} Edge3State;

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */

/* sym-class -> canonical cord1 representative */
extern int      e3sym2raw [EDGE3_SYM_CLASSES];

/* self-symmetry bitmask for each sym-class */
extern uint16_t e3symstate[EDGE3_SYM_CLASSES];

/* cord1 -> packed (sym_class << 3) | syminv_element */
extern int      e3raw2sym [EDGE3_CORD1_MAX];

/* Pruning table: 2 bits per entry, stores depth%3; 0x3 = unseen sentinel.
 * EDGE3_N_EPRUN/16 uint32_t entries = ~7.75 MB (vs 31 MB for 1 byte/entry). */
#define EDGE3_MAX_DEPTH 15
extern uint32_t eprun_packed[EDGE3_N_EPRUN / 16];

/* mvrot[m*8+r][i]  = where position i lands after move m then rotate r */
/* mvroto[m*8+r][i] = orientation-tracking counterpart (inverse perm via std) */
extern uint8_t  e3mvrot [EDGE3_PHASE3_MOVES * EDGE3_SYM_COUNT][12];
extern uint8_t  e3mvroto[EDGE3_PHASE3_MOVES * EDGE3_SYM_COUNT][12];

/* Compact r=0 slices of e3mvrot/e3mvroto for combined move+std update */
extern uint8_t e3cpos[EDGE3_PHASE3_MOVES][12];
extern uint8_t e3cval[EDGE3_PHASE3_MOVES][12];

extern uint8_t e3sym_min_prun[EDGE3_SYM_CLASSES];

/* Decode cord1*N_RAW+cord2 into s->edge[]; sets edgeo[i]=i. */
void edge3_set_from_int(Edge3State *s, int idx);

/* Lehmer rank of first end elements of s->edge[] (requires prior std()). */
int  edge3_get(const Edge3State *s, int end);

/* Normalise: invert edgeo[] into temp[], remap edge[], reset edgeo[i]=i. */
void edge3_std(Edge3State *s);

void edge3_move(Edge3State *s, int p3m);

/* Elementary rotation generator (r=0,1,2). */
void edge3_rot(Edge3State *s, int r);

/* Compose rotations into one of 8 orientations (r=0..7). */
void edge3_rotate(Edge3State *s, int r);

/* Convert EdgeCube ep[24] to Edge3State. Returns parity (1=odd). */
int  edge3_set_from_edgecube(Edge3State *s, const uint8_t ep[24]);

/* Lehmer rank of ep[] after move+rotation mr_idx. end=4→cord1; end=10→cord2 (caller takes %N_RAW). */
int edge3_getmvrot(const uint8_t *ep, int mr_idx, int end);

/* Fixed-bound specializations: compiler can fully unroll. */
static inline int edge3_getmvrot4(const uint8_t *ep, int mr_idx) {
    const uint8_t *mov  = e3mvrot [mr_idx];
    const uint8_t *movo = e3mvroto[mr_idx];
    long long val = 0xba9876543210LL;
    int idx = 0;
    for (int i = 0; i < 4; i++) {
        int v = movo[ep[mov[i]]] << 2;
        idx = idx * (12 - i) + (int)((val >> v) & 0xf);
        val -= 0x111111111110LL << v;
    }
    return idx;
}
static inline int edge3_getmvrot10(const uint8_t *ep, int mr_idx) {
    const uint8_t *mov  = e3mvrot [mr_idx];
    const uint8_t *movo = e3mvroto[mr_idx];
    long long val = 0xba9876543210LL;
    int idx = 0;
    for (int i = 0; i < 10; i++) {
        int v = movo[ep[mov[i]]] << 2;
        idx = idx * (12 - i) + (int)((val >> v) & 0xf);
        val -= 0x111111111110LL << v;
    }
    return idx;
}

/* Recover depth from 2-bit mod-3 pruning table; prun disambiguates. Returns EDGE3_MAX_DEPTH for unseen. */
int edge3_getprun(int edge_coord, int prun);

/* Exact depth via backward BFS; call once per P3 candidate, not inside the IDA* loop. */
int edge3_getprun_init(int edge_coord);

void edge3_init_mvrot(void);
void edge3_init_sym2raw(void);
void edge3_create_prun(void);
void edge3_init_combined(void);
void edge3_init(void);

#endif /* EDGE3_H */
