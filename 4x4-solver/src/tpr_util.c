#include "../include/tpr_util.h"
#include <string.h>

int tpr_Cnk[25][25];
int fact[13] = {
    1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800, 39916800, 479001600
};

void build_pascal_triangle(void) {
    for (int n = 0; n < 25; n++) {
        tpr_Cnk[n][0] = 1;
        for (int k = 1; k <= n; k++)
            tpr_Cnk[n][k] = tpr_Cnk[n-1][k-1] + tpr_Cnk[n-1][k];
        for (int k = n+1; k < 25; k++)
            tpr_Cnk[n][k] = 0;
    }
}

void swap4_int(int *arr, int a, int b, int c, int d, int key) {
    int tmp;
    switch (key) {
        case 0: tmp=arr[d]; arr[d]=arr[c]; arr[c]=arr[b]; arr[b]=arr[a]; arr[a]=tmp; break; /* CW:  a<-d<-c<-b<-a */
        case 1: tmp=arr[a]; arr[a]=arr[c]; arr[c]=tmp;
                tmp=arr[b]; arr[b]=arr[d]; arr[d]=tmp; break;
        case 2: tmp=arr[a]; arr[a]=arr[b]; arr[b]=arr[c]; arr[c]=arr[d]; arr[d]=tmp; break; /* CCW: a->b->c->d->a */
    }
}

void swap4_u8(uint8_t *arr, int a, int b, int c, int d, int key) {
    uint8_t tmp;
    switch (key) {
        case 0: tmp=arr[d]; arr[d]=arr[c]; arr[c]=arr[b]; arr[b]=arr[a]; arr[a]=tmp; break;
        case 1: tmp=arr[a]; arr[a]=arr[c]; arr[c]=tmp;
                tmp=arr[b]; arr[b]=arr[d]; arr[d]=tmp; break;
        case 2: tmp=arr[a]; arr[a]=arr[b]; arr[b]=arr[c]; arr[c]=arr[d]; arr[d]=tmp; break;
    }
}

int parity_u8(const uint8_t *arr, int len) {
    int visited[len];
    memset(visited, 0, sizeof(int) * (size_t)len);
    int parity = 0;
    for (int i = 0; i < len; i++) {
        if (!visited[i]) {
            int j = i;
            int cycle = 0;
            while (!visited[j]) {
                visited[j] = 1;
                j = arr[j];
                cycle++;
            }
            if ((cycle & 1) == 0) parity ^= 1;
        }
    }
    return parity;
}

void set8perm(uint8_t *arr, int idx) {
    int free[8] = {0,1,2,3,4,5,6,7};
    for (int i = 0; i < 8; i++) {
        int f = fact[7 - i];
        int q = idx / f;
        arr[i] = (uint8_t)free[q];
        for (int j = q; j < 7 - i; j++) free[j] = free[j+1];
        idx %= f;
    }
}

int get_perm_rank(const int *arr, int n) {
    int rank = 0;
    for (int i = 0; i < n; i++) {
        int cnt = 0;
        for (int j = i+1; j < n; j++)
            if (arr[j] < arr[i]) cnt++;
        rank += cnt * fact[n-1-i];
    }
    return rank;
}
