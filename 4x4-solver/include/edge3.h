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
    int edge[12];
    int edgeo[12];
    int temp[12];   /* scratch for std() and set_from_edgecube() */
} Edge3State;

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */

/* sym-class → canonical cord1 representative */
extern int      e3sym2raw [EDGE3_SYM_CLASSES];

/* self-symmetry bitmask for each sym-class */
extern uint16_t e3symstate[EDGE3_SYM_CLASSES];

/* cord1 → packed (sym_class << 3) | syminv_element */
extern int      e3raw2sym [EDGE3_CORD1_MAX];

/* 2-bit pruning table: 16 entries per int, mod-3 depth encoding */
extern int      eprun[EDGE3_N_EPRUN / 16];

/* mvrot[m*8+r][i]  = where position i lands after move m then rotate r */
/* mvroto[m*8+r][i] = orientation-tracking counterpart (inverse perm via std) */
extern int      e3mvrot [EDGE3_PHASE3_MOVES * EDGE3_SYM_COUNT][12];
extern int      e3mvroto[EDGE3_PHASE3_MOVES * EDGE3_SYM_COUNT][12];

/* -------------------------------------------------------------------------
 * State operations
 * ------------------------------------------------------------------------- */

/* Decode raw index cord1*N_RAW+cord2 into s->edge[]. Sets edgeo[i]=i. */
void edge3_set_from_int(Edge3State *s, int idx);

/* Lehmer rank of first end elements of s->edge[]. Caller must have called std(). */
int  edge3_get(const Edge3State *s, int end);

/* Normalise: build inverse of edgeo[] into s->temp[], remap edge[], reset edgeo[i]=i. */
void edge3_std(Edge3State *s);

/* Apply phase-3 move p3m (0..19) to both edge[] and edgeo[]. */
void edge3_move(Edge3State *s, int p3m);

/* Elementary rotation generator (r=0,1,2). */
void edge3_rot(Edge3State *s, int r);

/* Compose rotations to produce one of 8 distinct orientations (r=0..7). */
void edge3_rotate(Edge3State *s, int r);

/* Convert EdgeCube ep[24] to Edge3State. Returns parity (1=odd). */
int  edge3_set_from_edgecube(Edge3State *s, const uint8_t ep[24]);

/* -------------------------------------------------------------------------
 * Read-only coordinate evaluation
 * ------------------------------------------------------------------------- */

/* Lehmer rank of first end elements of ep[] after applying move+rotation mrIdx,
 * without modifying ep[]. mrIdx = move*8 + rotation. */
int edge3_getmvrot(const int *ep, int mrIdx, int end);

/* -------------------------------------------------------------------------
 * Pruning
 * ------------------------------------------------------------------------- */

int  edge3_get_pruning(int idx);
void edge3_set_pruning(int idx, int val);

/* IDA* lower bound. prun = current bound%3 passed down the recursion. */
int  edge3_getprun(int edge_coord, int prun);

/* -------------------------------------------------------------------------
 * Initialisation (call in this order)
 * ------------------------------------------------------------------------- */

void edge3_init_mvrot(void);
void edge3_init_sym2raw(void);
void edge3_create_prun(void);
void edge3_init(void);

/* -------------------------------------------------------------------------
 * Solved detection
 * ------------------------------------------------------------------------- */

int edge3_is_solved(int edge_coord);   /* true iff edge_coord == 0 */

#endif /* EDGE3_H */
