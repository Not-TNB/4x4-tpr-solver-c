#ifndef CENTER1_H
#define CENTER1_H

/*
 * Phase 1 -- Center1 coordinate.
 *
 * Goal: place all 8 U/D-colour centres on U or D (orient the U/D axis).
 * Raw: C(24,8)=735,471.  Sym-reduced: 15,582 classes under 48-element group.
 */

#include <stdint.h>

#define CENTER1_SYM_CLASSES 15582
#define CENTER1_RAW_COORDS  735471
#define CENTER1_SYM_COUNT   48

extern int      ctsmv  [CENTER1_SYM_CLASSES][36]; /* packed: (class<<6)|sym */
extern int      sym2raw[CENTER1_SYM_CLASSES];
extern int      raw2sym[CENTER1_RAW_COORDS];       /* packed: (class<<6)|sym */
extern uint8_t  csprun [CENTER1_SYM_CLASSES];

extern uint8_t  symmult[CENTER1_SYM_COUNT][CENTER1_SYM_COUNT];
extern uint8_t  symmove[CENTER1_SYM_COUNT][36];
extern uint8_t  syminv [CENTER1_SYM_COUNT];
extern int      finish [CENTER1_SYM_COUNT];  /* raw coord of solved state under each sym */

int center1_get(const uint8_t ct[24]);

/* Raw coord for axis urf (0=UD, 1=RL, 2=FB): ranks positions where ct[i]%3==urf. */
int center1_raw_urf(const uint8_t ct[24], int urf);

void center1_init_sym(void);
void center1_init_sym2raw(void);
void center1_create_move_table(void);
void center1_create_prun(void);
void center1_init(void);

#endif /* CENTER1_H */
