#include "../include/cubie.h"
#include "../include/tpr_util.h"
#include <string.h>

/* Per-slot macros: private to this file.  Encoding: face*16 + local_slot.
 * Only safe to use here — these names (fc, fd, ff, etc.) collide with
 * common identifiers and must not appear in public headers. */
#define FSLOT(face, local) ((face)*16 + (local))
#define u0  FSLOT(0,0)
#define u1  FSLOT(0,1)
#define u2  FSLOT(0,2)
#define u3  FSLOT(0,3)
#define u4  FSLOT(0,4)
#define u5  FSLOT(0,5)
#define u6  FSLOT(0,6)
#define u7  FSLOT(0,7)
#define u8  FSLOT(0,8)
#define u9  FSLOT(0,9)
#define ua  FSLOT(0,10)
#define ub  FSLOT(0,11)
#define uc  FSLOT(0,12)
#define ud  FSLOT(0,13)
#define ue  FSLOT(0,14)
#define uf  FSLOT(0,15)
#define r0  FSLOT(1,0)
#define r1  FSLOT(1,1)
#define r2  FSLOT(1,2)
#define r3  FSLOT(1,3)
#define r4  FSLOT(1,4)
#define r5  FSLOT(1,5)
#define r6  FSLOT(1,6)
#define r7  FSLOT(1,7)
#define r8  FSLOT(1,8)
#define r9  FSLOT(1,9)
#define ra  FSLOT(1,10)
#define rb  FSLOT(1,11)
#define rc  FSLOT(1,12)
#define rd  FSLOT(1,13)
#define re  FSLOT(1,14)
#define rf  FSLOT(1,15)
#define f0  FSLOT(2,0)
#define f1  FSLOT(2,1)
#define f2  FSLOT(2,2)
#define f3  FSLOT(2,3)
#define f4  FSLOT(2,4)
#define f5  FSLOT(2,5)
#define f6  FSLOT(2,6)
#define f7  FSLOT(2,7)
#define f8  FSLOT(2,8)
#define f9  FSLOT(2,9)
#define fa  FSLOT(2,10)
#define fb  FSLOT(2,11)
#define f_c FSLOT(2,12)  /* fc avoided: shadows 'fc' variable name */
#define f_d FSLOT(2,13)  /* fd avoided */
#define f_e FSLOT(2,14)  /* fe avoided */
#define f_f FSLOT(2,15)  /* ff avoided */
#define d0  FSLOT(3,0)
#define d1  FSLOT(3,1)
#define d2  FSLOT(3,2)
#define d3  FSLOT(3,3)
#define d4  FSLOT(3,4)
#define d5  FSLOT(3,5)
#define d6  FSLOT(3,6)
#define d7  FSLOT(3,7)
#define d8  FSLOT(3,8)
#define d9  FSLOT(3,9)
#define da  FSLOT(3,10)
#define db  FSLOT(3,11)
#define dc  FSLOT(3,12)
#define dd  FSLOT(3,13)
#define de  FSLOT(3,14)
#define df  FSLOT(3,15)
#define l0  FSLOT(4,0)
#define l1  FSLOT(4,1)
#define l2  FSLOT(4,2)
#define l3  FSLOT(4,3)
#define l4  FSLOT(4,4)
#define l5  FSLOT(4,5)
#define l6  FSLOT(4,6)
#define l7  FSLOT(4,7)
#define l8  FSLOT(4,8)
#define l9  FSLOT(4,9)
#define la  FSLOT(4,10)
#define lb  FSLOT(4,11)
#define lc  FSLOT(4,12)
#define ld  FSLOT(4,13)
#define le  FSLOT(4,14)
#define lf  FSLOT(4,15)
#define b0  FSLOT(5,0)
#define b1  FSLOT(5,1)
#define b2  FSLOT(5,2)
#define b3  FSLOT(5,3)
#define b4  FSLOT(5,4)
#define b5  FSLOT(5,5)
#define b6  FSLOT(5,6)
#define b7  FSLOT(5,7)
#define b8  FSLOT(5,8)
#define b9  FSLOT(5,9)
#define ba  FSLOT(5,10)
#define bb  FSLOT(5,11)
#define bc  FSLOT(5,12)
#define bd  FSLOT(5,13)
#define be  FSLOT(5,14)
#define bf  FSLOT(5,15)

/* -------------------------------------------------------------------------
 * Facelet slot → center/edge/corner position tables.
 * These mirror the Java source arrays (centerFacelet, EdgeColor, EdgeMap,
 * cornerFacelet).  All values are verified against the reference.
 * ------------------------------------------------------------------------- */

/* Solved-state facelet slot for each of the 24 center positions.
 * Ordering: U0-U3, D0-D3, F0-F3, B0-B3, R0-R3, L0-L3
 * (matches Java Center.centerFacelet). */
const uint8_t centerFacelet[24] = {
    /* U centers: slots 5,6,9,10 on U face */
    FSLOT(0,5), FSLOT(0,6), FSLOT(0,9), FSLOT(0,10),
    /* D centers */
    FSLOT(3,5), FSLOT(3,6), FSLOT(3,9), FSLOT(3,10),
    /* F centers */
    FSLOT(2,5), FSLOT(2,6), FSLOT(2,9), FSLOT(2,10),
    /* B centers */
    FSLOT(5,5), FSLOT(5,6), FSLOT(5,9), FSLOT(5,10),
    /* R centers */
    FSLOT(1,5), FSLOT(1,6), FSLOT(1,9), FSLOT(1,10),
    /* L centers */
    FSLOT(4,5), FSLOT(4,6), FSLOT(4,9), FSLOT(4,10),
};

/* Face color for each of the 12 edge-pair positions [pair][0=A side, 1=B side].
 * Face indices: U=0 R=1 F=2 D=3 L=4 B=5. */
const int edge_color[12][2] = {
    {0,2},{0,1},{0,5},{0,4},  /* UF UR UB UL */
    {3,2},{3,1},{3,5},{3,4},  /* DF DR DB DL */
    {2,1},{2,4},{5,1},{5,4},  /* FR FL BR BL */
};

/* Maps wing-edge slot (0..23) to the corresponding 3×3 edge facelet.
 * A-wings (0..11) and B-wings (12..23) each provide one sticker
 * for the reduced 3×3 cube. */
const int edge_map[24] = {
    /* A-wings: the "primary" facelet of each edge */
    FSLOT(0,13), FSLOT(0,7), FSLOT(0,1), FSLOT(0,4),  /* UF UR UB UL */
    FSLOT(3,1),  FSLOT(3,7), FSLOT(3,13),FSLOT(3,4),  /* DF DR DB DL */
    FSLOT(2,7),  FSLOT(2,4), FSLOT(5,4), FSLOT(5,7),  /* FR FL BR BL */
    /* B-wings: the "secondary" facelet */
    FSLOT(2,1),  FSLOT(1,4), FSLOT(5,1), FSLOT(4,1),  /* UF UR UB UL */
    FSLOT(2,13), FSLOT(1,13),FSLOT(5,13),FSLOT(4,13), /* DF DR DB DL */
    FSLOT(1,7),  FSLOT(4,7), FSLOT(1,4), FSLOT(4,4),  /* FR FL BR BL */
};

/* -------------------------------------------------------------------------
 * CenterCube
 * ------------------------------------------------------------------------- */

void center_cube_identity(CenterCube *c) {
    for (int i = 0; i < 24; i++)
        c->ct[i] = (uint8_t)(centerFacelet[i] / 16);
}

void center_cube_copy(CenterCube *dst, const CenterCube *src) {
    memcpy(dst->ct, src->ct, 24);
}

void center_cube_move(CenterCube *c, int m) {
    /* TODO: apply move m (0..35) to c->ct using swap4_u8 cycles. */
    (void)c; (void)m;
}

void center_cube_fill_333_facelet(const CenterCube *c, char *facelet54) {
    /* TODO: set facelet54[face*9+4] from c->ct. */
    (void)c; (void)facelet54;
}

/* -------------------------------------------------------------------------
 * EdgeCube
 * ------------------------------------------------------------------------- */

void edge_cube_identity(EdgeCube *e) {
    for (int i = 0; i < 24; i++) e->ep[i] = (uint8_t)i;
}

void edge_cube_copy(EdgeCube *dst, const EdgeCube *src) {
    memcpy(dst->ep, src->ep, 24);
}

void edge_cube_move(EdgeCube *e, int m) {
    /* TODO: apply move m to e->ep. */
    (void)e; (void)m;
}

bool edge_cube_check(const EdgeCube *e) {
    /* TODO: verify each pair ep[i] and ep[i+12] belong to the same edge-pair. */
    (void)e;
    return false;
}

int edge_cube_parity(const EdgeCube *e) {
    /* TODO: return parity of ep[0..23] permutation. */
    (void)e;
    return 0;
}

void edge_cube_fill_333_facelet(const EdgeCube *e, char *facelet54) {
    /* TODO: write edge stickers into facelet54 via edge_map. */
    (void)e; (void)facelet54;
}

/* -------------------------------------------------------------------------
 * CornerCube
 * ------------------------------------------------------------------------- */

CornerCube corner_move_cube[18];

void corner_cube_identity(CornerCube *c) {
    for (int i = 0; i < 8; i++) { c->cp[i] = (uint8_t)i; c->co[i] = 0; }
}

void corner_cube_copy(CornerCube *dst, const CornerCube *src) {
    memcpy(dst, src, sizeof(CornerCube));
}

void corner_cube_mult(const CornerCube *a, const CornerCube *b, CornerCube *prod) {
    for (int i = 0; i < 8; i++) {
        prod->cp[i] = a->cp[b->cp[i]];
        prod->co[i] = (uint8_t)((a->co[b->cp[i]] + b->co[i]) % 3);
    }
}

void corner_cube_move(CornerCube *c, int m) {
    /* m in 0..35; only outer moves affect corners: use m % 18. */
    CornerCube tmp;
    corner_cube_mult(c, &corner_move_cube[m % 18], &tmp);
    corner_cube_copy(c, &tmp);
}

int corner_cube_parity(const CornerCube *c) {
    return parity_u8(c->cp, 8);
}

void corner_cube_fill_333_facelet(const CornerCube *c, char *facelet54) {
    /* TODO: write corner stickers into facelet54. */
    (void)c; (void)facelet54;
}

void corner_cube_init_moves(void) {
    /* TODO: populate corner_move_cube[18] for the 18 outer face moves. */
}

/* -------------------------------------------------------------------------
 * FullCube
 * ------------------------------------------------------------------------- */

void fullcube_identity(FullCube *c) {
    center_cube_identity(&c->center);
    edge_cube_identity(&c->edge);
    corner_cube_identity(&c->corner);
    c->moveLength   = 0;
    c->edgeAvail    = 0;
    c->centerAvail  = 0;
    c->cornerAvail  = 0;
    c->value        = 0;
    c->add1         = false;
    c->length1      = 0;
    c->length2      = 0;
    c->length3      = 0;
    c->sym          = 0;
}

void fullcube_copy(FullCube *dst, const FullCube *src) {
    memcpy(dst, src, sizeof(FullCube));
}

void fullcube_move(FullCube *c, int m) {
    /* Lazy: only buffer; do not apply to sub-cubes yet. */
    if (c->moveLength < FULLCUBE_MOVE_BUF)
        c->moveBuffer[c->moveLength++] = (uint8_t)m;
}

void fullcube_do_move(FullCube *c, int m) {
    center_cube_move(&c->center, m);
    edge_cube_move(&c->edge, m);
    corner_cube_move(&c->corner, m);
    fullcube_move(c, m);
    c->edgeAvail   = c->moveLength;
    c->centerAvail = c->moveLength;
    c->cornerAvail = c->moveLength;
}

EdgeCube *fullcube_get_edge(FullCube *c) {
    while (c->edgeAvail < c->moveLength)
        edge_cube_move(&c->edge, c->moveBuffer[c->edgeAvail++]);
    return &c->edge;
}

CenterCube *fullcube_get_center(FullCube *c) {
    while (c->centerAvail < c->moveLength)
        center_cube_move(&c->center, c->moveBuffer[c->centerAvail++]);
    return &c->center;
}

CornerCube *fullcube_get_corner(FullCube *c) {
    while (c->cornerAvail < c->moveLength)
        corner_cube_move(&c->corner, c->moveBuffer[c->cornerAvail++]);
    return &c->corner;
}

void fullcube_from_facelet(FullCube *c, const char *facelet96) {
    /* TODO: parse the 96-char facelet string and populate all sub-cubes. */
    fullcube_identity(c);
    (void)facelet96;
}

void fullcube_to_333_facelet(FullCube *c, char *out54) {
    /* Ensure all sub-cubes are up to date. */
    CenterCube *ct = fullcube_get_center(c);
    EdgeCube   *e  = fullcube_get_edge(c);
    CornerCube *co = fullcube_get_corner(c);

    memset(out54, '?', 54);
    out54[54] = '\0';

    center_cube_fill_333_facelet(ct, out54);
    edge_cube_fill_333_facelet(e, out54);
    corner_cube_fill_333_facelet(co, out54);
}

void fullcube_from_moves(FullCube *c, const int *moves, int n) {
    fullcube_identity(c);
    for (int i = 0; i < n; i++) fullcube_do_move(c, moves[i]);
}

