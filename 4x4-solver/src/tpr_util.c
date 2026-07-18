#include "../include/tpr_util.h"
#include <string.h>

int Cnk[25][25];
int fact[13];

void tpr_util_init(void) {
    /* Pascal's triangle */
    for (int n = 0; n < 25; n++) {
        Cnk[n][0] = 1;
        for (int k = 1; k <= n; k++)
            Cnk[n][k] = Cnk[n-1][k-1] + Cnk[n-1][k];
        for (int k = n+1; k < 25; k++)
            Cnk[n][k] = 0;
    }

    /* Factorials */
    fact[0] = 1;
    for (int i = 1; i < 13; i++)
        fact[i] = fact[i-1] * i;
}

void swap4_int(int *arr, int a, int b, int c, int d, int key) {
    int tmp;
    switch (key) {
        case 0: tmp=arr[a]; arr[a]=arr[b]; arr[b]=arr[c]; arr[c]=arr[d]; arr[d]=tmp; break;
        case 1: tmp=arr[a]; arr[a]=arr[c]; arr[c]=tmp;
                tmp=arr[b]; arr[b]=arr[d]; arr[d]=tmp; break;
        case 2: tmp=arr[d]; arr[d]=arr[c]; arr[c]=arr[b]; arr[b]=arr[a]; arr[a]=tmp; break;
    }
}

void swap4_u8(uint8_t *arr, int a, int b, int c, int d, int key) {
    uint8_t tmp;
    switch (key) {
        case 0: tmp=arr[a]; arr[a]=arr[b]; arr[b]=arr[c]; arr[c]=arr[d]; arr[d]=tmp; break;
        case 1: tmp=arr[a]; arr[a]=arr[c]; arr[c]=tmp;
                tmp=arr[b]; arr[b]=arr[d]; arr[d]=tmp; break;
        case 2: tmp=arr[d]; arr[d]=arr[c]; arr[c]=arr[b]; arr[b]=arr[a]; arr[a]=tmp; break;
    }
}

void swap2_int(int *arr, int x, int y) {
    int tmp = arr[x]; arr[x] = arr[y]; arr[y] = tmp;
}

void swap2_u8(uint8_t *arr, int x, int y) {
    uint8_t tmp = arr[x]; arr[x] = arr[y]; arr[y] = tmp;
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

int parity_int(const int *arr, int len) {
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
    /* Decode Lehmer rank idx (0..40319) into a permutation of 0..7. */
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
