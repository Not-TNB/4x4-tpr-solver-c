#include "../include/moves.h"
#include <string.h>
#include <stdbool.h>

const char *move2str[36] = {
    "U","U2","U'","R","R2","R'","F","F2","F'",
    "D","D2","D'","L","L2","L'","B","B2","B'",
    "u","u2","u'","r","r2","r'","f","f2","f'",
    "d","d2","d'","l","l2","l'","b","b2","b'"
};

/*
 * Phase 2 move set (28 moves): all 18 outer + Uw2 Fw2 Dw2 Bw2 + all Rw Lw.
 * Indices match moves.h definitions.
 */
int move2std[29] = {
    Ux1, Ux2, Ux3,
    Rx1, Rx2, Rx3,
    Fx1, Fx2, Fx3,
    Dx1, Dx2, Dx3,
    Lx1, Lx2, Lx3,
    Bx1, Bx2, Bx3,
    ux2,             /* Uw2 only */
    rx1, rx2, rx3,   /* all Rw   */
    fx2,             /* Fw2 only */
    dx2,             /* Dw2 only */
    lx1, lx2, lx3,  /* all Lw   */
    bx2,             /* Bw2 only */
    EOM
};

/*
 * Phase 3 move set (20 moves): outer U/F/D/B (×3), R2/L2 only, wide as
 * half-turns only.
 */
int move3std[21] = {
    Ux1, Ux2, Ux3,
    Rx2,
    Fx1, Fx2, Fx3,
    Dx1, Dx2, Dx3,
    Lx2,
    Bx1, Bx2, Bx3,
    ux2,
    rx2,
    fx2,
    dx2,
    lx2,
    bx2,
    EOM
};

int std2move[37];
int std3move[37];

bool ckmv [37][36];
bool ckmv2[29][28];
bool ckmv3[21][20];

int skipAxis [36];
int skipAxis2[28];
int skipAxis3[20];

/* Return the face axis (0..5) for standard move index m (0..35). */
static int move_axis(int m) { return m / 3; }

/* Two axes are "opposite" if they differ by 3 (U-D, R-L, F-B). */
static bool opposite_axes(int a, int b) { return (a < 6 && b < 6 && (a-b==3 || b-a==3)); }

static void build_ckmv_for_set(bool *table,        /* [N+1][N] flattened */
                                int  *move_set,     /* move_set[N] = EOM sentinel */
                                int   N,
                                int  *skip_out)
{
    /* table is (N+1) × N, row-major, caller passes flattened pointer. */
    bool (*T)[N] = (bool (*)[N])table;  /* VLA pointer trick */

    /* Sentinel row N: no prev move -> nothing is redundant. */
    memset(T[N], 0, sizeof(bool) * (size_t)N);

    for (int prev = 0; prev < N; prev++) {
        int pa = move_axis(move_set[prev]);
        for (int cur = 0; cur < N; cur++) {
            int ca = move_axis(move_set[cur]);
            /* Redundant if same face, or same-axis commuting pair out of order. */
            bool same = (pa == ca);
            bool comm = (opposite_axes(pa, ca) && pa > ca);
            T[prev][cur] = same || comm;
        }
    }

    /* skipAxis[m]: next index to try after detecting a redundancy on axis of m. */
    for (int m = 0; m < N; m++) {
        int axis = move_axis(move_set[m]);
        int skip = m;
        while (skip < N && move_axis(move_set[skip]) == axis) skip++;
        skip_out[m] = skip;
    }
}

void moves_init(void) {
    /* Build reverse maps. */
    memset(std2move, -1, sizeof(std2move));
    memset(std3move, -1, sizeof(std3move));
    for (int i = 0; move2std[i] != EOM; i++) std2move[move2std[i]] = i;
    for (int i = 0; move3std[i] != EOM; i++) std3move[move3std[i]] = i;

    /* Count phase set sizes. */
    int N2 = 0; while (move2std[N2] != EOM) N2++;
    int N3 = 0; while (move3std[N3] != EOM) N3++;

    /* Phase 1: all 36 moves. */
    int all36[37];
    for (int i = 0; i < 36; i++) all36[i] = i;
    all36[36] = EOM;
    build_ckmv_for_set((bool *)ckmv,  all36,    36, skipAxis);
    build_ckmv_for_set((bool *)ckmv2, move2std, N2, skipAxis2);
    build_ckmv_for_set((bool *)ckmv3, move3std, N3, skipAxis3);
}
