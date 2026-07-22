#ifndef CENTER3_H
#define CENTER3_H

/*
 * Phase 3 -- Center3 coordinate.
 *
 * Combined coordinate over (ud C(7,4)=35) × (fb C(7,4)=35) × (rl 12-reachable) × (parity 2)
 * = 35 × 35 × 12 × 2 = 29,400 states.
 *
 * Tables:
 *   ctmove[29400][20]  -- combined move table for all 20 Phase 3 moves
 *   c3prun[29400]      -- BFS pruning distances (uses first 17 of 20 moves)
 */

#include <stdint.h>

#define CENTER3_STATE_COORDS   29400   /* 35 × 35 × 12 × 2 */
#define CENTER3_PHASE3_MOVES   20

extern uint16_t ctmove[CENTER3_STATE_COORDS][CENTER3_PHASE3_MOVES];
extern uint8_t  c3prun[CENTER3_STATE_COORDS];

/* Working state: binary arrays encoding which of the two colors each slot holds. */
typedef struct {
    uint8_t ud[8];  /* (ct[i]   / 3) ^ 1 for i=0..7  (1=U-colored, 0=D-colored) */
    uint8_t fb[8];  /* (ct[i+8] / 3) ^ 1 for i=0..7  (1=F-colored, 0=B-colored) */
    uint8_t rl[8];  /* (ct[i+16]/ 3) ^ 1 ^ par for i=0..7                        */
    int     parity;
} Center3State;

/* Populate s from a CenterCube ct[24] and edge permutation parity (0 or 1). */
void center3_set(Center3State *s, const uint8_t ct[24], int eparity);

/* Encode s -> combined coordinate [0, 29399]. */
int center3_getct(const Center3State *s);

/* Decode combined coordinate -> s. */
void center3_setct(Center3State *s, int idx);

/* Apply Phase 3 move p3m (0..19) to s in place. */
void center3_move(Center3State *s, int p3m);

/* Initialisation. */
void center3_create_move_table(void);
void center3_create_prun(void);
void center3_init(void);


#endif /* CENTER3_H */
