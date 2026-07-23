#include "../include/moves.h"
#include <string.h>
#include <stdbool.h>

const char *move2str[36] = {
    "U","U2","U'","R","R2","R'","F","F2","F'",
    "D","D2","D'","L","L2","L'","B","B2","B'",
    "Uw","Uw2","Uw'","Rw","Rw2","Rw'","Fw","Fw2","Fw'",
    "Dw","Dw2","Dw'","Lw","Lw2","Lw'","Bw","Bw2","Bw'"
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
    Uwx2,             /* Uw2 only */
    Rwx1, Rwx2, Rwx3, /* all Rw   */
    Fwx2,             /* Fw2 only */
    Dwx2,             /* Dw2 only */
    Lwx1, Lwx2, Lwx3, /* all Lw   */
    Bwx2,             /* Bw2 only */
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
    Uwx2,
    Rwx2,
    Fwx2,
    Dwx2,
    Lwx2,
    Bwx2,
    EOM
};

int std2move[37];
int std3move[37];

bool ckmv [37][36];
bool ckmv2[29][28];
bool ckmv3[21][20];

int skip_axis [36];
int skip_axis2[28];
int skip_axis3[20];

static int move_axis(int m) { return m / 3; }

static bool opposite_axes(int a, int b) { return (a < 6 && b < 6 && (a-b==3 || b-a==3)); }

static void build_ckmv_for_set(bool *table,        /* [N+1][N] flattened */
                                int  *move_set,     /* move_set[N] = EOM sentinel */
                                int   N,
                                int  *skip_out)
{
    bool (*T)[N] = (bool (*)[N])table;  /* VLA pointer trick */

    /* Sentinel row N: no prev move -> nothing is redundant. */
    memset(T[N], 0, sizeof(bool) * (size_t)N);

    for (int prev = 0; prev < N; prev++) {
        int pa = move_axis(move_set[prev]);
        for (int cur = 0; cur < N; cur++) {
            int ca = move_axis(move_set[cur]);
            /* Redundant: same face, or commuting opposite-axis pair out of order. */
            bool same = (pa == ca);
            bool comm = (opposite_axes(pa, ca) && pa > ca);
            T[prev][cur] = same || comm;
        }
    }

    /* First index past the current axis — jump here on redundancy. */
    for (int m = 0; m < N; m++) {
        int axis = move_axis(move_set[m]);
        int skip = m;
        while (skip < N && move_axis(move_set[skip]) == axis) skip++;
        skip_out[m] = skip;
    }
}

void moves_init(void) {
    memset(std2move, -1, sizeof(std2move));
    memset(std3move, -1, sizeof(std3move));
    for (int i = 0; move2std[i] != EOM; i++) std2move[move2std[i]] = i;
    for (int i = 0; move3std[i] != EOM; i++) std3move[move3std[i]] = i;

    int N2 = 0; while (move2std[N2] != EOM) N2++;
    int N3 = 0; while (move3std[N3] != EOM) N3++;

    int all36[37];
    for (int i = 0; i < 36; i++) all36[i] = i;
    all36[36] = EOM;
    build_ckmv_for_set((bool *)ckmv,  all36,    36, skip_axis);
    build_ckmv_for_set((bool *)ckmv2, move2std, N2, skip_axis2);
    build_ckmv_for_set((bool *)ckmv3, move3std, N3, skip_axis3);
}
