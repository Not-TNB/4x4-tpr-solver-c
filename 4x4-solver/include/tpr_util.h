#ifndef TPR_UTIL_H
#define TPR_UTIL_H

#include <stdint.h>

/* Pascal's triangle C(n,k) for n,k in [0,24]. */
extern int Cnk[25][25];

/* Factorials 0..12. */
extern int fact[13];

void build_pascal_triangle(void); /* populate Cnk */

/*
 * 4-position cyclic rotate on an array of int or uint8_t.
 *   key=0: CW  a<-d<-c<-b<-a
 *   key=1: 2×swap  a↔c, b↔d
 *   key=2: CCW a->b->c->d->a
 */
void swap4_int  (int    *arr, int a, int b, int c, int d, int key);
void swap4_u8   (uint8_t *arr, int a, int b, int c, int d, int key);

/* Permutation parity (number of inversions mod 2). */
int parity_u8 (const uint8_t *arr, int len);

/*
 * Decode integer idx (0..40319) into a permutation of 8 elements
 * using the packed-nibble Lehmer-code trick (matches CornerCube.setCPerm).
 */
void set8perm(uint8_t *arr, int idx);

/* Encode a permutation of n elements (n ≤ 12) to its Lehmer rank. */
int get_perm_rank(const int *arr, int n);

#endif /* TPR_UTIL_H */
