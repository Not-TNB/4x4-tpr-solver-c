#ifndef CENTER1_H
#define CENTER1_H

/*
 * Phase 1 -- Center1 coordinate.
 *
 * Goal: orient the U/D X-centres so that all 8 U/D-colour centres are on
 *       the U or D face (i.e., the U/D axis is chosen).
 *
 * Raw coordinate: C(24,8) = 735,471  (choose which 8 of 24 slots hold U/D stickers)
 * Symmetry-reduced: 15,582 classes under the 48-element cube symmetry group.
 *
 * Tables (all heap-allocated / global, populated by center1_init):
 *   ctsmv[15582][36]  -- symmetry-reduced move table
 *   sym2raw[15582]    -- representative raw coord for each sym class
 *   raw2sym[735471]   -- maps raw coord -> (class<<5)|sym  (packed)
 *   csprun[15582]     -- IDA* pruning distance (0..255, 255=uninitialised)
 *   symmult[48][48]   -- composition table for symmetry elements
 *   symmove[48][36]   -- conjugate: S*m*S^-1 maps move m under symmetry S
 *   syminv[48]        -- inverse symmetry index
 *   finish[48]        -- bitmask: which of 8 solved states each sym element maps to
 */

#include <stdint.h>

#define CENTER1_SYM_CLASSES 15582
#define CENTER1_RAW_COORDS  735471
#define CENTER1_SYM_COUNT   48

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */
extern int      ctsmv  [CENTER1_SYM_CLASSES][36]; /* packed: (class<<6)|sym */
extern int      sym2raw[CENTER1_SYM_CLASSES];
extern int      raw2sym[CENTER1_RAW_COORDS];       /* packed: (class<<6)|sym */
extern uint8_t  csprun [CENTER1_SYM_CLASSES];

extern uint8_t  symmult[CENTER1_SYM_COUNT][CENTER1_SYM_COUNT];
extern uint8_t  symmove[CENTER1_SYM_COUNT][36];
extern uint8_t  syminv [CENTER1_SYM_COUNT];
extern int      finish [CENTER1_SYM_COUNT];  /* raw coord of solved state under each sym */

/* -------------------------------------------------------------------------
 * Coordinate functions (operate on CenterCube.ct[24])
 * ------------------------------------------------------------------------- */

/* Compute the raw Center1 coordinate from a CenterCube. */
int center1_get(const uint8_t ct[24]);

/* Compute the raw Center1 coordinate for axis urf (0=UD, 1=RL, 2=FB).
 * Marks positions where ct[i] % 3 == urf. Used for three-axis Phase 1. */
int center1_raw_urf(const uint8_t ct[24], int urf);

/* -------------------------------------------------------------------------
 * Pruning
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */

/* Build symmult, symmove, syminv, finish. */
void center1_init_sym(void);

/* Build sym2raw and raw2sym from the identity CenterCube and BFS. */
void center1_init_sym2raw(void);

/* Build ctsmv[15582][36] move table. */
void center1_create_move_table(void);

/* Build csprun[15582] pruning table (BFS from solved classes). */
void center1_create_prun(void);

/* Top-level init: calls the above in order. */
void center1_init(void);

#endif /* CENTER1_H */
