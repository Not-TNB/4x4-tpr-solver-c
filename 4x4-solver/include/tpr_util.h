#ifndef TPR_UTIL_H
#define TPR_UTIL_H

#include <stdint.h>

extern int tpr_Cnk[25][25];
extern int fact[13];

void build_pascal_triangle(void);

/* key=0: CW a<-d<-c<-b<-a  key=1: 2×swap a↔c,b↔d  key=2: CCW a->b->c->d->a */
void swap4_int(int     *arr, int a, int b, int c, int d, int key);
void swap4_u8 (uint8_t *arr, int a, int b, int c, int d, int key);

int  parity_u8    (const uint8_t *arr, int len);
void set8perm     (uint8_t *arr, int idx);
int  get_perm_rank(const int *arr, int n);

#endif /* TPR_UTIL_H */
