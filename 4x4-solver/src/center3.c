#include "../include/center3.h"
#include "../include/tpr_util.h"
#include "../include/moves.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */
uint16_t ctmove[CENTER3_STATE_COORDS][CENTER3_PHASE3_MOVES];
uint8_t  c3prun[CENTER3_STATE_COORDS];

/* -------------------------------------------------------------------------
 * rl sub-coordinate: only 12 of C(8,4)=70 values are reachable after Phase 2.
 * rl2std[i]   = the C(8,4) index for the i-th reachable rl value (i=0..11)
 * std2rl[idx] = reverse map; -1 for unreachable indices
 * ------------------------------------------------------------------------- */
static const int rl2std[12] = {0, 9, 14, 23, 27, 28, 41, 42, 46, 55, 60, 69};
static int std2rl[70];

/* Wide Phase 3 moves (p3m 14-19) toggle parity; outer moves (0-13) do not. */
static const int pmove[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1};

/* -------------------------------------------------------------------------
 * center3_set -- populate s from a CenterCube and edge parity.
 *
 * ud[i] = 1 if the piece at U/D position i (ct[i])   has a "top" color (U/R/F)
 * fb[i] = 1 if the piece at F/B position i (ct[i+8]) has a "top" color
 * rl[i] = similar for R/L positions, XOR'd with internal parity
 *
 * After Phase 2 only UD pieces occupy positions 0-7, only FB occupy 8-15, only
 * RL occupy 16-23, so:
 *   ud[i]==1 → U-colored piece, ud[i]==0 → D-colored
 *   fb[i]==1 → F-colored,       fb[i]==0 → B-colored
 *   rl[i] encodes R vs L after parity adjustment
 * ------------------------------------------------------------------------- */
void center3_set(Center3State *s, const uint8_t ct[24], int eparity) {
    int a = ct[0] % 3, b = ct[8] % 3, c = ct[16] % 3;
    int par = ((a > b) ^ (b > c) ^ (a > c)) ? 0 : 1;
    for (int i = 0; i < 8; i++) {
        s->ud[i] = (uint8_t)((ct[i]    / 3) ^ 1);
        s->fb[i] = (uint8_t)((ct[i+8]  / 3) ^ 1);
        s->rl[i] = (uint8_t)((ct[i+16] / 3) ^ 1 ^ par);
    }
    s->parity = par ^ eparity;
}

/* -------------------------------------------------------------------------
 * center3_getct -- encode to combined [0, 29399] coordinate.
 *
 * Layout: parity + 2 * (ud_rank*35*12 + fb_rank*12 + std2rl[rl_rank_raw])
 *   ud_rank: C(7,4) rank of positions 0-6 differing from ud[7]  → [0,34]
 *   fb_rank: same for fb                                          → [0,34]
 *   rl_rank: C(8,4) rank using check = fb[7]^ud[7] as reference → 12 values
 * ------------------------------------------------------------------------- */
int center3_getct(const Center3State *s) {
    int idx = 0, r = 4;
    for (int i = 6; i >= 0; i--)
        if (s->ud[i] != s->ud[7]) idx += Cnk[i][r--];
    idx *= 35;
    r = 4;
    for (int i = 6; i >= 0; i--)
        if (s->fb[i] != s->fb[7]) idx += Cnk[i][r--];
    idx *= 12;
    int check = s->fb[7] ^ s->ud[7];
    int idxrl = 0; r = 4;
    for (int i = 7; i >= 0; i--)
        if (s->rl[i] != check) idxrl += Cnk[i][r--];
    return s->parity + 2 * (idx + std2rl[idxrl]);
}

/* -------------------------------------------------------------------------
 * center3_setct -- decode combined coordinate back into s.
 * ------------------------------------------------------------------------- */
void center3_setct(Center3State *s, int idx) {
    s->parity = idx & 1;
    idx >>= 1;
    int idxrl = rl2std[idx % 12];
    idx /= 12;
    int r = 4;
    for (int i = 7; i >= 0; i--) {
        s->rl[i] = 0;
        if (idxrl >= Cnk[i][r]) { idxrl -= Cnk[i][r--]; s->rl[i] = 1; }
    }
    int idxfb = idx % 35;
    idx /= 35;
    r = 4;
    s->fb[7] = 0;
    for (int i = 6; i >= 0; i--) {
        if (idxfb >= Cnk[i][r]) { idxfb -= Cnk[i][r--]; s->fb[i] = 1; }
        else s->fb[i] = 0;
    }
    r = 4;
    s->ud[7] = 0;
    for (int i = 6; i >= 0; i--) {
        if (idx >= Cnk[i][r]) { idx -= Cnk[i][r--]; s->ud[i] = 1; }
        else s->ud[i] = 0;
    }
}

/* -------------------------------------------------------------------------
 * center3_move -- apply Phase 3 move p3m (0..19) to s.
 *
 * Move index → Java Center3 case mapping (identical ordering):
 *   0-2  : U, U2, U'           3  : R2        4-6  : F, F2, F'
 *   7-9  : D, D2, D'           10 : L2        11-13: B, B2, B'
 *   14-19: Uw2 Rw2 Fw2 Dw2 Lw2 Bw2
 * ------------------------------------------------------------------------- */
void center3_move(Center3State *s, int p3m) {
    s->parity ^= pmove[p3m];
    switch (p3m) {
    case 0: case 1: case 2:
        swap4_u8(s->ud, 0, 1, 2, 3, p3m % 3);
        break;
    case 3:
        swap4_u8(s->rl, 0, 1, 2, 3, 1);
        break;
    case 4: case 5: case 6:
        swap4_u8(s->fb, 0, 1, 2, 3, (p3m - 1) % 3);
        break;
    case 7: case 8: case 9:
        swap4_u8(s->ud, 4, 5, 6, 7, (p3m - 1) % 3);
        break;
    case 10:
        swap4_u8(s->rl, 4, 5, 6, 7, 1);
        break;
    case 11: case 12: case 13:
        swap4_u8(s->fb, 4, 5, 6, 7, (p3m + 1) % 3);
        break;
    case 14: /* Uw2 */
        swap4_u8(s->ud, 0, 1, 2, 3, 1);
        swap4_u8(s->rl, 0, 5, 4, 1, 1);
        swap4_u8(s->fb, 0, 5, 4, 1, 1);
        break;
    case 15: /* Rw2 */
        swap4_u8(s->rl, 0, 1, 2, 3, 1);
        swap4_u8(s->fb, 1, 4, 7, 2, 1);
        swap4_u8(s->ud, 1, 6, 5, 2, 1);
        break;
    case 16: /* Fw2 */
        swap4_u8(s->fb, 0, 1, 2, 3, 1);
        swap4_u8(s->ud, 3, 2, 5, 4, 1);
        swap4_u8(s->rl, 0, 3, 6, 5, 1);
        break;
    case 17: /* Dw2 */
        swap4_u8(s->ud, 4, 5, 6, 7, 1);
        swap4_u8(s->rl, 3, 2, 7, 6, 1);
        swap4_u8(s->fb, 3, 2, 7, 6, 1);
        break;
    case 18: /* Lw2 */
        swap4_u8(s->rl, 4, 5, 6, 7, 1);
        swap4_u8(s->fb, 0, 3, 6, 5, 1);
        swap4_u8(s->ud, 0, 3, 4, 7, 1);
        break;
    case 19: /* Bw2 */
        swap4_u8(s->fb, 4, 5, 6, 7, 1);
        swap4_u8(s->ud, 0, 7, 6, 1, 1);
        swap4_u8(s->rl, 1, 4, 7, 2, 1);
        break;
    }
}

/* -------------------------------------------------------------------------
 * Table accessors
 * ------------------------------------------------------------------------- */
int center3_ct_move(int ct_coord, int p3m) { return ctmove[ct_coord][p3m]; }
int center3_prun_get(int ct_coord)          { return c3prun[ct_coord]; }
int center3_is_solved(int ct_coord)         { return ct_coord == 0; }

/* -------------------------------------------------------------------------
 * center3_create_move_table -- build ctmove[29400][20] by simulation.
 * ------------------------------------------------------------------------- */
void center3_create_move_table(void) {
    Center3State s;
    for (int i = 0; i < CENTER3_STATE_COORDS; i++) {
        center3_setct(&s, i);
        for (int m = 0; m < CENTER3_PHASE3_MOVES; m++) {
            Center3State t = s;
            center3_move(&t, m);
            ctmove[i][m] = (uint16_t)center3_getct(&t);
        }
    }
}

/* -------------------------------------------------------------------------
 * center3_create_prun -- forward BFS from state 0 using first 17 of 20 moves.
 * (Matching Java: only p3m 0-16 used in BFS; Dw2/Lw2/Bw2 excluded.)
 * ------------------------------------------------------------------------- */
void center3_create_prun(void) {
    memset(c3prun, 0xFF, sizeof(c3prun));
    c3prun[0] = 0;
    int depth = 0, done = 1;
    while (done < CENTER3_STATE_COORDS) {
        for (int i = 0; i < CENTER3_STATE_COORDS; i++) {
            if (c3prun[i] != (uint8_t)depth) continue;
            for (int m = 0; m < 17; m++) {
                int ni = ctmove[i][m];
                if (c3prun[ni] == 0xFF) { c3prun[ni] = (uint8_t)(depth + 1); done++; }
            }
        }
        depth++;
    }
}

void center3_init(void) {
    for (int i = 0; i < 70; i++) std2rl[i] = -1;
    for (int i = 0; i < 12; i++) std2rl[rl2std[i]] = i;
    center3_create_move_table();
    center3_create_prun();
}
