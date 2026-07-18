#include "../include/edge3.h"
#include "../include/tpr_util.h"
#include "../include/moves.h"
#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------- */
int      e3sym2raw[EDGE3_SYM_CLASSES];
int      e3raw2sym[EDGE3_RAW_PERMS];

uint16_t e3smv  [EDGE3_SYM_CLASSES][EDGE3_PHASE3_MOVES];
uint16_t e3rawmv[EDGE3_RAW_PERMS  ][EDGE3_PHASE3_MOVES];

/* 2-bit packed: 4 entries per byte, index = class*RAW + raw, packed /4 */
uint8_t eprun     [EDGE3_SYM_CLASSES * EDGE3_RAW_PERMS / 4];
uint8_t eprun_base[EDGE3_SYM_CLASSES];

uint8_t mvrot [EDGE3_PHASE3_MOVES][EDGE3_PHASE3_MOVES];
uint8_t mvroto[EDGE3_PHASE3_MOVES][EDGE3_SYM_COUNT];

/* -------------------------------------------------------------------------
 * Raw coordinate
 *
 * After Phase 2, the 24 wings form 12 pairs. The raw Edge3 coordinate
 * encodes the permutation of these 12 pairs.
 *
 * Encoding: factoradic rank of the 12-element permutation restricted to
 * even permutations only (parity is 0 after Phase 2 guarantees).
 * Range: 12!/2 = 239,500,800 / 2 — wait, the Java uses 20160 for the
 * "within-class" range because it tracks only the non-trivially-related
 * permutations. See docs/tpr-algorithm.md §Phase 3 for the factoradic
 * details: 8!/2 × 7!/2 doesn't match either; actual value from Java is
 * EDGE3_RAW_PERMS = 20160 = 8! / 2 (edge perm within one orbit).
 * TODO: reconcile with the Java EdgeCube.getIndex() implementation.
 * ------------------------------------------------------------------------- */

int edge3_get_raw(const uint8_t ep[24]) {
    /* TODO: compute the factoradic rank of the paired edge permutation. */
    (void)ep;
    return 0;
}

int edge3_raw_move(int raw, int p3m) {
    return e3rawmv[raw][p3m];
}

int edge3_sym_move(int sym_class, int p3m) {
    return e3smv[sym_class][p3m];
}

int edge3_getsym_class(int raw) {
    return e3raw2sym[raw] >> 4;
}

int edge3_getsym_elem(int raw) {
    return e3raw2sym[raw] & 0xF;
}

/* -------------------------------------------------------------------------
 * 2-bit pruning table access
 *
 * Entry at (sym_class, raw_within_class):
 *   flat_idx = sym_class * EDGE3_RAW_PERMS + raw_within_class
 *   byte     = flat_idx / 4
 *   shift    = (flat_idx % 4) * 2
 * ------------------------------------------------------------------------- */

int eprun_get(int sym_class, int raw) {
    int idx   = sym_class * EDGE3_RAW_PERMS + raw;
    int byte  = idx >> 2;
    int shift = (idx & 3) << 1;
    return (eprun[byte] >> shift) & 3;
}

void eprun_set(int sym_class, int raw, int val2bit) {
    int idx   = sym_class * EDGE3_RAW_PERMS + raw;
    int byte  = idx >> 2;
    int shift = (idx & 3) << 1;
    eprun[byte] = (uint8_t)((eprun[byte] & ~(3 << shift)) | ((val2bit & 3) << shift));
}

int edge3_prun_get(int sym_class, int raw_within_class) {
    int bits = eprun_get(sym_class, raw_within_class);
    /* bits encodes (depth mod 3); add base depth of the sym class. */
    int base = eprun_base[sym_class];
    int delta = (bits - base % 3 + 3) % 3;
    return base + delta;
}

/* -------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */

void edge3_init_sym2raw(void) {
    /* TODO: BFS over edge permutation space to build e3sym2raw and e3raw2sym.
     * Use the 16-element symmetry subgroup relevant to Phase 3. */
    memset(e3sym2raw, -1, sizeof(e3sym2raw));
    memset(e3raw2sym, -1, sizeof(e3raw2sym));
}

void edge3_create_move_tables(void) {
    /* TODO: populate e3rawmv and e3smv from raw edge permutation moves. */
    memset(e3rawmv, 0, sizeof(e3rawmv));
    memset(e3smv,   0, sizeof(e3smv));
    memset(mvrot,   0, sizeof(mvrot));
    memset(mvroto,  0, sizeof(mvroto));
}

void edge3_create_prun(void) {
    memset(eprun,      0, sizeof(eprun));
    memset(eprun_base, 0, sizeof(eprun_base));
    /* TODO: BFS from solved edge state, writing (depth mod 3) as 2-bit entries.
     * Uses e3rawmv for transitions; e3smv and sym2raw for class reduction. */
}

void edge3_init(void) {
    edge3_init_sym2raw();
    edge3_create_move_tables();
    edge3_create_prun();
}

int edge3_is_solved(int sym_class, int raw) {
    /* Solved when both sub-coords are 0 (identity permutation). */
    return (sym_class == 0 && raw == 0);
}
