#ifndef MOVES_H
#define MOVES_H

#include <stdbool.h>

/*
 * Move index encoding: m = axis*3 + power
 *   axis  0..5  = outer faces U R F D L B
 *   axis  6..11 = wide  faces Uw Rw Fw Dw Lw Bw
 *   power 0=CW  1=180°  2=CCW
 *
 * Total: 36 moves (0..35).  eom=36 marks end-of-list sentinels.
 */

/* Outer face moves */
#define Ux1  0
#define Ux2  1
#define Ux3  2
#define Rx1  3
#define Rx2  4
#define Rx3  5
#define Fx1  6
#define Fx2  7
#define Fx3  8
#define Dx1  9
#define Dx2 10
#define Dx3 11
#define Lx1 12
#define Lx2 13
#define Lx3 14
#define Bx1 15
#define Bx2 16
#define Bx3 17

/* Wide (2-layer) moves */
#define ux1 18
#define ux2 19
#define ux3 20
#define rx1 21
#define rx2 22
#define rx3 23
#define fx1 24
#define fx2 25
#define fx3 26
#define dx1 27
#define dx2 28
#define dx3 29
#define lx1 30
#define lx2 31
#define lx3 32
#define bx1 33
#define bx2 34
#define bx3 35

#define EOM 36  /* end-of-move-set sentinel */

/* Human-readable move strings indexed by move index 0..35. */
extern const char *move2str[36];
/* Inverse move strings (same index → inverse move label). */
extern const char *moveIstr[36];

/*
 * Phase 2 move set: 28 moves.
 * All 18 outer moves + Uw2 Fw2 Dw2 Bw2 + all Rw Lw (×3).
 * move2std[i] = standard move index for phase-2 move i.
 * move2std[28] = EOM.
 */
extern int move2std[29];

/*
 * Phase 3 move set: 20 moves.
 * All outer U/F/D/B (×3), R2 L2 only, all wide as half-turns only.
 * move3std[i] = standard move index for phase-3 move i.
 * move3std[20] = EOM.
 */
extern int move3std[21];

/* Reverse mappings: standard index → phase index (-1 if not in set). */
extern int std2move[37];
extern int std3move[37];

/*
 * Move-redundancy tables.
 *   ckmv[i][j]  = true  iff move j is redundant immediately after move i
 *                 (same face, or commuting opposite-face pair out of canonical order).
 *   ckmv[36][j] = false for all j  (sentinel: no previous move).
 */
extern bool ckmv [37][36];
extern bool ckmv2[29][28]; /* phase-2 version */
extern bool ckmv3[21][20]; /* phase-3 version */

/*
 * skipAxis[i]: when ckmv[i][m] is true, jump m to skipAxis[m]
 * to skip all remaining redundant moves on the same axis.
 */
extern int skipAxis [36];
extern int skipAxis2[28];
extern int skipAxis3[20];

/* Build all tables above. Must be called once before any search. */
void moves_init(void);

#endif /* MOVES_H */
