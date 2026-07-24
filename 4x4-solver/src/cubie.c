#include "../include/cubie.h"
#include "../include/tpr_util.h"
#include <string.h>
#include <stdio.h>

/* face*16+local_slot shorthands; private -- fc/fd/ff aliased to avoid clashes */
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

/* Solved-state slot for each centre position. Within each group: not row-major. */
const uint8_t center_facelet[24] = {
    /* U centres */
    FSLOT(0,5), FSLOT(0,6), FSLOT(0,10), FSLOT(0,9),
    /* D centres */
    FSLOT(3,5), FSLOT(3,6), FSLOT(3,10), FSLOT(3,9),
    /* F centres */
    FSLOT(2,5), FSLOT(2,6), FSLOT(2,10), FSLOT(2,9),
    /* B centres */
    FSLOT(5,5), FSLOT(5,6), FSLOT(5,10), FSLOT(5,9),
    /* R centres */
    FSLOT(1,5), FSLOT(1,6), FSLOT(1,10), FSLOT(1,9),
    /* L centres */
    FSLOT(4,5), FSLOT(4,6), FSLOT(4,10), FSLOT(4,9),
};


void center_cube_identity(CenterCube *c) {
    for (int i = 0; i < 24; i++)
        c->ct[i] = (uint8_t)(center_facelet[i] / 16);
}

static const uint8_t ct_cycles[12][3][4] = {
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
static const int ct_ncycles[12] = {1,1,1,1,1,1, 3,3,3,3,3,3};

void center_cube_move(CenterCube *c, int m) {
    const int axis = m / 3, key = m % 3;
    for (int i = 0; i < ct_ncycles[axis]; i++) {
        const uint8_t *cyc = ct_cycles[axis][i];
        swap4_u8(c->ct, cyc[0], cyc[1], cyc[2], cyc[3], key);
    }
}


void edge_cube_identity(EdgeCube *e) {
    for (int i = 0; i < 24; i++) e->ep[i] = (uint8_t)i;
}

static const uint8_t ep_cycles[12][3][4] = {
    /* U  */ {{ 0, 1, 2, 3}, {12,13,14,15}},
    /* R  */ {{11,15,10,19}, {23, 3,22, 7}},
    /* F  */ {{ 0,11, 6, 8}, {12,23,18,20}},
    /* D  */ {{ 4, 5, 6, 7}, {16,17,18,19}},
    /* L  */ {{ 1,20, 5,21}, {13, 8,17, 9}},
    /* B  */ {{ 2, 9, 4,10}, {14,21,16,22}},
    /* Uw */ {{ 0, 1, 2, 3}, {12,13,14,15}, { 9,22,11,20}},
    /* Rw */ {{11,15,10,19}, {23, 3,22, 7}, { 2,16, 6,12}},
    /* Fw */ {{ 0,11, 6, 8}, {12,23,18,20}, { 3,19, 5,13}},
    /* Dw */ {{ 4, 5, 6, 7}, {16,17,18,19}, { 8,23,10,21}},
    /* Lw */ {{ 1,20, 5,21}, {13, 8,17, 9}, {14, 0,18, 4}},
    /* Bw */ {{ 2, 9, 4,10}, {14,21,16,22}, { 7,15, 1,17}},
};
static const int ep_ncycles[12] = {2,2,2,2,2,2, 3,3,3,3,3,3};

void edge_cube_move(EdgeCube *e, int m) {
    const int axis = m / 3, key = m % 3;
    for (int i = 0; i < ep_ncycles[axis]; i++) {
        const uint8_t *cyc = ep_cycles[axis][i];
        swap4_u8(e->ep, cyc[0], cyc[1], cyc[2], cyc[3], key);
    }
}

bool edge_cube_check(const EdgeCube *e) {
    int ck = 0, parity = 0;
    for (int i = 0; i < 12; i++) {
        ck |= 1 << e->ep[i];
        if (e->ep[i] >= 12) parity ^= 1;
    }
    ck &= (ck >> 12);
    return ck == 0 && parity == 0;
}

int edge_cube_parity(const EdgeCube *e) {
    uint8_t perm[12];
    for (int i = 0; i < 12; i++)
        perm[i] = e->ep[i] % 12;
    return parity_u8(perm, 12);
}


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
    CornerCube tmp;
    corner_cube_mult(c, &corner_move_cube[m % 18], &tmp);
    corner_cube_copy(c, &tmp);
}

int corner_cube_parity(const CornerCube *c) {
    return parity_u8(c->cp, 8);
}

static void set_twist(uint8_t *co, int idx) {
    int twst = 0;
    for (int i = 6; i >= 0; i--) {
        twst += co[i] = (uint8_t)(idx % 3);
        idx /= 3;
    }
    co[7] = (uint8_t)((15 - twst) % 3);
}

void corner_cube_init_moves(void) {
    static const int base[6][2] = {
        {15120,    0},   /* U */
        {21021, 1494},   /* R */
        { 8064, 1236},   /* F */
        {    9,    0},   /* D */
        { 1230,  412},   /* L */
        {  224,  137},   /* B */
    };

    for (int f = 0; f < 6; f++) {
        const int a = 3*f;
        set8perm(corner_move_cube[a].cp, base[f][0]);
        set_twist(corner_move_cube[a].co, base[f][1]);
        corner_cube_mult(&corner_move_cube[a],   &corner_move_cube[a], &corner_move_cube[a+1]);
        corner_cube_mult(&corner_move_cube[a+1], &corner_move_cube[a], &corner_move_cube[a+2]);
    }
}


void fullcube_identity(FullCube *c) {
    center_cube_identity(&c->center);
    edge_cube_identity(&c->edge);
    corner_cube_identity(&c->corner);
    c->move_length   = 0;
    c->edge_avail    = 0;
    c->center_avail  = 0;
    c->corner_avail  = 0;
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
    if (c->move_length < FULLCUBE_MOVE_BUF)
        c->move_buffer[c->move_length++] = (uint8_t)m;
}

void fullcube_do_move(FullCube *c, int m) {
    center_cube_move(&c->center, m);
    edge_cube_move(&c->edge, m);
    corner_cube_move(&c->corner, m);
    fullcube_move(c, m);
    c->edge_avail   = c->move_length;
    c->center_avail = c->move_length;
    c->corner_avail = c->move_length;
}

EdgeCube *fullcube_get_edge(FullCube *c) {
    while (c->edge_avail < c->move_length)
        edge_cube_move(&c->edge, c->move_buffer[c->edge_avail++]);
    return &c->edge;
}

CenterCube *fullcube_get_center(FullCube *c) {
    while (c->center_avail < c->move_length)
        center_cube_move(&c->center, c->move_buffer[c->center_avail++]);
    return &c->center;
}

CornerCube *fullcube_get_corner(FullCube *c) {
    while (c->corner_avail < c->move_length)
        corner_cube_move(&c->corner, c->move_buffer[c->corner_avail++]);
    return &c->corner;
}

void fullcube_from_facelet(FullCube *c, const char *facelet96) {
    /* ef[i][0..1] = solved-state slots of wing i; /16 gives face colour. */
    static const uint8_t ef[24][2] = {
        {FSLOT(0,13), FSLOT(2, 1)},  /* 0:  UF A */
        {FSLOT(0, 4), FSLOT(4, 1)},  /* 1:  UL A */
        {FSLOT(0, 2), FSLOT(5, 1)},  /* 2:  UB A */
        {FSLOT(0,11), FSLOT(1, 1)},  /* 3:  UR A */
        {FSLOT(3,13), FSLOT(5,14)},  /* 4:  DB A */
        {FSLOT(3, 4), FSLOT(4,14)},  /* 5:  DL A */
        {FSLOT(3, 2), FSLOT(2,14)},  /* 6:  DF A */
        {FSLOT(3,11), FSLOT(1,14)},  /* 7:  DR A */
        {FSLOT(4,11), FSLOT(2, 8)},  /* 8:  FL A */
        {FSLOT(4, 4), FSLOT(5, 7)},  /* 9:  BL A */
        {FSLOT(1,11), FSLOT(5, 8)},  /* 10: BR A */
        {FSLOT(1, 4), FSLOT(2, 7)},  /* 11: FR A */
        {FSLOT(2, 2), FSLOT(0,14)},  /* 12: UF B */
        {FSLOT(4, 2), FSLOT(0, 8)},  /* 13: UL B */
        {FSLOT(5, 2), FSLOT(0, 1)},  /* 14: UB B */
        {FSLOT(1, 2), FSLOT(0, 7)},  /* 15: UR B */
        {FSLOT(5,13), FSLOT(3,14)},  /* 16: DB B */
        {FSLOT(4,13), FSLOT(3, 8)},  /* 17: DL B */
        {FSLOT(2,13), FSLOT(3, 1)},  /* 18: DF B */
        {FSLOT(1,13), FSLOT(3, 7)},  /* 19: DR B */
        {FSLOT(2, 4), FSLOT(4, 7)},  /* 20: FL B */
        {FSLOT(5,11), FSLOT(4, 8)},  /* 21: BL B */
        {FSLOT(5, 4), FSLOT(1, 7)},  /* 22: BR B */
        {FSLOT(2,11), FSLOT(1, 8)},  /* 23: FR B */
    };
    /* cf[i]: [0]=U/D slot [1]=R/L slot [2]=F/B slot. */
    static const uint8_t cf[8][3] = {
        {FSLOT(0,15), FSLOT(1, 0), FSLOT(2, 3)},  /* 0: URF */
        {FSLOT(0,12), FSLOT(2, 0), FSLOT(4, 3)},  /* 1: UFL */
        {FSLOT(0, 0), FSLOT(4, 0), FSLOT(5, 3)},  /* 2: ULB */
        {FSLOT(0, 3), FSLOT(5, 0), FSLOT(1, 3)},  /* 3: UBR */
        {FSLOT(3, 3), FSLOT(2,15), FSLOT(1,12)},  /* 4: DFR */
        {FSLOT(3, 0), FSLOT(4,15), FSLOT(2,12)},  /* 5: DLF */
        {FSLOT(3,12), FSLOT(5,15), FSLOT(4,12)},  /* 6: DBL */
        {FSLOT(3,15), FSLOT(1,15), FSLOT(5,12)},  /* 7: DRB */
    };

    fullcube_identity(c);

    uint8_t f[96];
    for (int i = 0; i < 96; i++) {
        const char *p = strchr("URFDLB", facelet96[i]);
        f[i] = p ? (uint8_t)(p - "URFDLB") : 0;
    }

    for (int i = 0; i < 24; i++)
        c->center.ct[i] = f[center_facelet[i]];

    for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 24; j++) {
            if (f[ef[i][0]] == ef[j][0] / 16 && f[ef[i][1]] == ef[j][1] / 16) {
                c->edge.ep[i] = (uint8_t)j;
                break;
            }
        }
    }

    for (int i = 0; i < 8; i++) {
        int ori;
        for (ori = 0; ori < 3; ori++)
            if (f[cf[i][ori]] == 0 || f[cf[i][ori]] == 3)
                break;
        uint8_t col1 = f[cf[i][(ori + 1) % 3]];
        uint8_t col2 = f[cf[i][(ori + 2) % 3]];
        for (int j = 0; j < 8; j++) {
            if (col1 == cf[j][1] / 16 && col2 == cf[j][2] / 16) {
                c->corner.cp[i] = (uint8_t)j;
                c->corner.co[i] = (uint8_t)(ori % 3);
                break;
            }
        }
    }
}

void fullcube_from_moves(FullCube *c, const int *moves, int n) {
    fullcube_identity(c);
    for (int i = 0; i < n; i++) fullcube_do_move(c, moves[i]);
}

void fullcube_do_alg(FullCube *c, const int *moves, int n) {
    for (int i = 0; i < n; i++) fullcube_do_move(c, moves[i]);
}

void fullcube_to_333_facelet(FullCube *c, char out54[55]) {
    static const char face_chars[] = "URFDLB?";
    static const int edge_color[12][2] = {
        {2,0}, {4,0}, {5,0}, {1,0},
        {5,3}, {4,3}, {2,3}, {1,3},
        {2,4}, {5,4}, {5,1}, {2,1}
    };
    static const int corner_facelet[8][3] = {
        { 8,  9, 20}, { 6, 18, 38}, { 0, 36, 47}, { 2, 45, 11},
        {29, 26, 15}, {27, 44, 24}, {33, 53, 42}, {35, 17, 51}
    };

    CenterCube *cen = fullcube_get_center(c);
    EdgeCube   *edg = fullcube_get_edge(c);
    CornerCube *cor = fullcube_get_corner(c);

    int sticker[54];
    for (int i=0; i<54; i++) { sticker[i] = 6; }

    for (int i = 0; i < 8; i++) {
        int p = cor->cp[i], o = cor->co[i];
        for (int k = 0; k < 3; k++)
            sticker[corner_facelet[i][k]] = corner_facelet[p][(k-o+3)%3] / 9;
    }

    static const int center_group[6] = {0, 16, 8, 4, 20, 12};
    for (int i = 0; i < 6; i++)
        sticker[4 + 9*i] = cen->ct[center_group[i]];

    static const int edge_map[24] = {
        19, 37, 46, 10, 52, 43, 25, 16,
        21, 50, 48, 23,
         7,  3,  1,  5, 34, 30, 28, 32,
        41, 39, 14, 12                                                                                   
    };
    for (int s = 0; s < 24; s++)
        sticker[edge_map[s]] = edg->ep[s] < 12
            ? edge_color[edg->ep[s]][0]
            : edge_color[edg->ep[s]-12][1];

    for (int i=0; i<54; i++) out54[i] = face_chars[sticker[i]];

    out54[54] = '\0';
}
