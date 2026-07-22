#ifndef CUBIE_H
#define CUBIE_H

/*
 * Cubie-level representation of the 4×4×4 cube.
 * Used by the TPR solver internally; distinct from the facelet-based
 * CubeState4 in the parent engine.
 *
 * Three independent sub-cubes:
 *   CenterCube  -- 24 X-centre stickers, one per centre slot
 *   EdgeCube    -- 24 wing-edge pieces, one per wing slot
 *   CornerCube  -- 8 corners with permutation + orientation
 *
 * Face/colour indices: U=0 R=1 F=2 D=3 L=4 B=5  (matches Moves U/R/F/D/L/B order)
 *
 * Facelet encoding: face*16 + local_slot (local_slot in 0..15, row-major 4×4).
 * Per-slot shorthand macros (u0, r5, fc, etc.) live in cubie.c only -- they
 * collide with common variable names and must not appear in public headers.
 */

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * CenterCube
 *
 * ct[i] = face colour (0–5) of the piece occupying centre position i.
 * 24 positions: U(0–3) D(4–7) F(8–11) B(12–15) R(16–19) L(20–23).
 *
 * Solved: ct[i] = centerFacelet[i] / 16.
 * ------------------------------------------------------------------------- */
typedef struct {
    uint8_t ct[24];
} CenterCube;

/* The 24 centre facelet slots in solved-state position order. */
extern const uint8_t centerFacelet[24];

void center_cube_identity(CenterCube *c);
void center_cube_copy(CenterCube *dst, const CenterCube *src);
void center_cube_move(CenterCube *c, int m); /* m = move index 0..35 */
void center_cube_fill_333_facelet(const CenterCube *c, const char *facelet54);

/* -------------------------------------------------------------------------
 * EdgeCube
 *
 * ep[i] = which wing-edge piece (0–23) occupies wing slot i.
 * 24 wing slots: positions 0–11 are "A" wings, 12–23 are "B" wings.
 *
 * Solved: ep[i] = i.
 * ------------------------------------------------------------------------- */
typedef struct {
    uint8_t ep[24];
} EdgeCube;

/* Two face-colours for each of the 12 edge pairs (indexed 0–11). */
extern const int edge_color[12][2];

/* Maps wing slots 0–23 to 3×3 facelet positions (for 333 conversion). */
extern const int edge_map[24];

void edge_cube_identity(EdgeCube *e);
void edge_cube_copy(EdgeCube *dst, const EdgeCube *src);
void edge_cube_move(EdgeCube *e, int m);
bool edge_cube_check(const EdgeCube *e);  /* valid state for phase 3 entry */
int  edge_cube_parity(const EdgeCube *e);
void edge_cube_fill_333_facelet(const EdgeCube *e, const char *facelet54);

/* -------------------------------------------------------------------------
 * CornerCube
 *
 * cp[i] = which corner piece (0–7) is in corner position i.
 * co[i] = orientation of that corner (0, 1, or 2).
 *
 * Only outer-face moves affect corners (wide moves: use m%18).
 * Solved: cp[i]=i, co[i]=0.
 * ------------------------------------------------------------------------- */
typedef struct {
    uint8_t cp[8];
    uint8_t co[8];
} CornerCube;

/* Precomputed move cubes for all 18 outer-face moves. */
extern CornerCube corner_move_cube[18];

void corner_cube_identity(CornerCube *c);
void corner_cube_copy(CornerCube *dst, const CornerCube *src);
void corner_cube_mult(const CornerCube *a, const CornerCube *b, CornerCube *prod);
void corner_cube_move(CornerCube *c, int m); /* m in 0..35, only outer 0..17 matter */
int  corner_cube_parity(const CornerCube *c);
void corner_cube_fill_333_facelet(const CornerCube *c, const char *facelet54);
void corner_cube_init_moves(void); /* build corner_move_cube[18] */

/* -------------------------------------------------------------------------
 * FullCube
 *
 * Combines all three sub-cubes with lazy-evaluation move buffering.
 * Moves are appended to moveBuffer and applied to sub-cubes on demand.
 * ------------------------------------------------------------------------- */
#define FULLCUBE_MOVE_BUF 60

typedef struct {
    EdgeCube   edge;
    CenterCube center;
    CornerCube corner;

    uint8_t moveBuffer[FULLCUBE_MOVE_BUF];
    int     moveLength;
    int     edgeAvail;    /* how many buffered moves are applied to edge   */
    int     centerAvail;  /* how many buffered moves are applied to centre */
    int     cornerAvail;  /* how many buffered moves are applied to corner */

    /* Search metadata (set by phase searches). */
    int  value;    /* priority = length1 + length2 + lower_bound */
    bool add1;     /* 2 normalization moves appended after phase 1 */
    int  length1;
    int  length2;
    int  length3;
    int  sym;      /* accumulated cube-rotation symmetry index (0–47) */
} FullCube;

void fullcube_identity(FullCube *c);
void fullcube_copy(FullCube *dst, const FullCube *src);

/* Append move to buffer (lazy -- does NOT apply to sub-cubes). */
void fullcube_move(FullCube *c, int m);

/* Apply move immediately to all three sub-cubes AND buffer. */
void fullcube_do_move(FullCube *c, int m);

/* Lazily flush pending moves into each sub-cube, return pointer. */
EdgeCube   *fullcube_get_edge  (FullCube *c);
CenterCube *fullcube_get_center(FullCube *c);
CornerCube *fullcube_get_corner(FullCube *c);

/* Build FullCube from a 96-char facelet string ("URFDLB" chars). */
void fullcube_from_facelet(FullCube *c, const char *facelet96);

/* Convert to 54-char 3×3 facelet string for min2phase. */
void fullcube_to_333_facelet(FullCube *c, char *out54);

/* Build FullCube by applying a sequence of move indices. */
void fullcube_from_moves(FullCube *c, const int *moves, int n);

#endif /* CUBIE_H */
