#include "../include/center2.h"
#include "../include/tpr_util.h"
#include "../include/moves.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */
uint8_t  rlmv  [CENTER2_RL_COORDS][CENTER2_PHASE2_MOVES];
uint16_t ctmv  [CENTER2_CT_COORDS][CENTER2_PHASE2_MOVES];
uint8_t  ctprun[CENTER2_PRUN_SIZE];   /* indexed: ct * CENTER2_RL_COORDS + rl */

/* -------------------------------------------------------------------------
 * Center2State: populate, get/set coordinates
 * ------------------------------------------------------------------------- */

void center2_set(Center2State *s, const uint8_t cc_ct[24], int edge_parity) {
    for (int i = 0; i < 16; i++)
        s->ct[i] = cc_ct[i] % 3;
    for (int i = 0; i < 8; i++)
        s->rl[i] = cc_ct[i + 16];
    s->parity = edge_parity;
}

int center2_getrl(const Center2State *s) {
    int idx = 0, r = 4;
    for (int i = 6; i >= 0; i--) {
        if (s->rl[i] != s->rl[7])
            idx += tpr_Cnk[i][r--];
    }
    return idx * 2 + s->parity;
}

void center2_setrl(Center2State *s, int idx) {
    s->parity = idx & 1;
    idx >>= 1;
    int r = 4;
    s->rl[7] = 0;
    for (int i = 6; i >= 0; i--) {
        if (idx >= tpr_Cnk[i][r]) {
            idx -= tpr_Cnk[i][r--];
            s->rl[i] = 1;
        } else {
            s->rl[i] = 0;
        }
    }
}

int center2_getct(const Center2State *s) {
    int idx = 0, r = 8;
    for (int i = 14; i >= 0; i--) {
        if (s->ct[i] != s->ct[15])
            idx += tpr_Cnk[i][r--];
    }
    return idx;
}

void center2_setct(Center2State *s, int idx) {
    int r = 8;
    s->ct[15] = 0;
    for (int i = 14; i >= 0; i--) {
        if (idx >= tpr_Cnk[i][r]) {
            idx -= tpr_Cnk[i][r--];
            s->ct[i] = 1;
        } else {
            s->ct[i] = 0;
        }
    }
}

/* -------------------------------------------------------------------------
 * Move simulation
 *
 * pmv[m] = 1 for moves that toggle edge parity: Rwx1(21), Rwx3(23), Lwx1(30), Lwx3(32).
 * ------------------------------------------------------------------------- */
static const int pmv[36] = {
    0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0,  /* outer U R F D L B */
    0,0,0,                                        /* Uw */
    1,0,1,                                        /* Rw: Rwx1 toggles, Rwx2 no, Rwx3 toggles */
    0,0,0,                                        /* Fw */
    0,0,0,                                        /* Dw */
    1,0,1,                                        /* Lw: Lwx1 toggles, Lwx2 no, Lwx3 toggles */
    0,0,0                                         /* Bw */
};

void center2_move(Center2State *s, int m) {
    s->parity ^= pmv[m];
    int key  = m % 3;
    int axis = m / 3;
    switch (axis) {
    case 0:  /* U  */ swap4_int(s->ct, 0,1,2,3, key); break;
    case 1:  /* R  */ swap4_int(s->rl, 0,1,2,3, key); break;
    case 2:  /* F  */ swap4_int(s->ct, 8,9,10,11, key); break;
    case 3:  /* D  */ swap4_int(s->ct, 4,5,6,7, key); break;
    case 4:  /* L  */ swap4_int(s->rl, 4,5,6,7, key); break;
    case 5:  /* B  */ swap4_int(s->ct, 12,13,14,15, key); break;
    case 6:  /* Uw */
        swap4_int(s->ct, 0,1,2,3, key);
        swap4_int(s->rl, 0,5,4,1, key);
        swap4_int(s->ct, 8,9,12,13, key);
        break;
    case 7:  /* Rw */
        swap4_int(s->rl, 0,1,2,3, key);
        swap4_int(s->ct, 1,15,5,9, key);
        swap4_int(s->ct, 2,12,6,10, key);
        break;
    case 8:  /* Fw */
        swap4_int(s->ct, 8,9,10,11, key);
        swap4_int(s->rl, 0,3,6,5, key);
        swap4_int(s->ct, 3,2,5,4, key);
        break;
    case 9:  /* Dw */
        swap4_int(s->ct, 4,5,6,7, key);
        swap4_int(s->rl, 3,2,7,6, key);
        swap4_int(s->ct, 11,10,15,14, key);
        break;
    case 10: /* Lw */
        swap4_int(s->rl, 4,5,6,7, key);
        swap4_int(s->ct, 0,8,4,14, key);
        swap4_int(s->ct, 3,11,7,13, key);
        break;
    case 11: /* Bw */
        swap4_int(s->ct, 12,13,14,15, key);
        swap4_int(s->rl, 1,4,7,2, key);
        swap4_int(s->ct, 1,0,7,6, key);
        break;
    }
}

/* -------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */

void center2_create_rl_move_table(void) {
    Center2State s;
    for (int i = 0; i < CENTER2_RL_COORDS; i++) {
        for (int mi = 0; mi < CENTER2_PHASE2_MOVES; mi++) {
            center2_setrl(&s, i);
            center2_move(&s, move2std[mi]);
            rlmv[i][mi] = (uint8_t)center2_getrl(&s);
        }
    }
}

void center2_create_ct_move_table(void) {
    Center2State s;
    for (int i = 0; i < CENTER2_CT_COORDS; i++) {
        for (int mi = 0; mi < CENTER2_PHASE2_MOVES; mi++) {
            center2_setct(&s, i);
            center2_move(&s, move2std[mi]);
            ctmv[i][mi] = (uint16_t)center2_getct(&s);
        }
    }
}

void center2_create_prun(void) {
    memset(ctprun, 0xFF, sizeof(ctprun));

    /* Seed the 6 solved states (ct=0, rl in {0,18,28,46,54,56}). */
    static const int solved_rl[6] = {0, 18, 28, 46, 54, 56};
    for (int i = 0; i < 6; i++)
        ctprun[solved_rl[i]] = 0;   /* ct=0 -> index = 0*70 + rl = rl */

    /* Forward BFS using first 23 of the 28 phase-2 moves. */
    int done = 6;
    for (int depth = 0; done < CENTER2_PRUN_SIZE; depth++) {
        for (int idx = 0; idx < CENTER2_PRUN_SIZE; idx++) {
            if (ctprun[idx] != (uint8_t)depth) continue;
            int ct = idx / CENTER2_RL_COORDS;
            int rl = idx % CENTER2_RL_COORDS;
            for (int m = 0; m < 23; m++) {
                int nidx = (int)ctmv[ct][m] * CENTER2_RL_COORDS + rlmv[rl][m];
                if (ctprun[nidx] == 0xFF) {
                    ctprun[nidx] = (uint8_t)(depth + 1);
                    done++;
                }
            }
        }
    }
}

void center2_init(void) {
    center2_create_rl_move_table();
    center2_create_ct_move_table();
    center2_create_prun();
}

int center2_is_solved(int rl, int ct) {
    if (ct != 0) return 0;
    return rl==0 || rl==18 || rl==28 || rl==46 || rl==54 || rl==56;
}
