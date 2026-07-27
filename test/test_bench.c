/*
 * test_bench.c -- performance benchmark
 *
 * Usage:
 *   ./test_bench [N]            run N 60-move scrambles (default: 10)
 *   TPR_SEED=0xABCD ./test_bench [N]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#include "../4x4-solver/include/search.h"
#include "../4x4-solver/include/cubie.h"
#include "../4x4-solver/ckociemba/include/search.h"
#include "../4x4-solver/include/moves.h"
#include "../4x4-solver/include/tpr_util.h"

#define C_GREEN "\033[32m"
#define C_RED   "\033[31m"
#define C_BOLD  "\033[1m"
#define C_RESET "\033[0m"

#define SCRAMBLE_LEN  60
#define BAR_WIDTH     30
#define DEFAULT_N     10

static int count_digits(int n) {
    int d = 1;
    while (n >= 10) { d++; n /= 10; }
    return d;
}

static void print_dashes(int n) {
    for (int i = 0; i < n; i++) putchar('-');
}

static void print_bar(int count, int max_count) {
    int filled = (max_count > 0) ? count * BAR_WIDTH / max_count : 0;
    for (int i = 0; i < filled; i++) putchar('#');
    for (int i = filled; i < BAR_WIDTH; i++) putchar(' ');
}

static int lcg(unsigned int *s) {
    *s = *s * 1664525u + 1013904223u;
    return (int)((*s >> 16) & 0x7fff);
}

static void gen_scramble(int *moves, int n, unsigned int *seed) {
    int prev = 36;
    int i = 0;
    while (i < n) {
        int m = lcg(seed) % 36;
        if (prev < 36 && ckmv[prev][m]) continue;
        moves[i++] = m;
        prev = m;
    }
}

static int kok_str_to_move(const char *s) {
    int base;
    switch (s[0]) {
        case 'U': base =  0; break;
        case 'R': base =  3; break;
        case 'F': base =  6; break;
        case 'D': base =  9; break;
        case 'L': base = 12; break;
        case 'B': base = 15; break;
        default: return -1;
    }
    if (!s[1])        return base;
    if (s[1] == '2')  return base + 1;
    if (s[1] == '\'') return base + 2;
    return -1;
}

static const int SOLVED_CT[24] = {
    0,0,0,0,  /* U */
    3,3,3,3,  /* D */
    2,2,2,2,  /* F */
    5,5,5,5,  /* B */
    1,1,1,1,  /* R */
    4,4,4,4,  /* L */
};

static bool goal_solved(const FullCube *fc) {
    for (int i = 0; i < 24; i++)
        if (fc->center.ct[i] != (uint8_t)SOLVED_CT[i]) return false;
    for (int i = 0; i < 24; i++)
        if (fc->edge.ep[i] != (uint8_t)i) return false;
    for (int i = 0; i < 8; i++) {
        if (fc->corner.cp[i] != (uint8_t)i) return false;
        if (fc->corner.co[i] != 0) return false;
    }
    return true;
}

/* static: too large for the stack */
static FullCube s_p1[SEARCH_BEAM1_MAX];
static FullCube s_p2[SEARCH_BEAM2_MAX];
static FullCube s_p3;

typedef struct {
    bool      solved;
    double    ms;
    int       total_moves;
    TprDiag   diag;
} BenchResult;

static double elapsed_ms(struct timespec t0) {
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) * 1e-6;
}

static BenchResult run_one(const FullCube *scrambled) {
    BenchResult r = {false, 0.0, 0, {0}};

    struct timespec t0, tp;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    clock_gettime(CLOCK_MONOTONIC, &tp);
    int n1 = search1(scrambled, s_p1, SEARCH_BEAM1_MAX);
    tpr_diag.p1_ms = elapsed_ms(tp);
    tpr_diag.n1    = n1;
    if (n1 == 0) goto done;

    clock_gettime(CLOCK_MONOTONIC, &tp);
    int n2 = search2(s_p1, n1, s_p2, SEARCH_BEAM2_MAX);
    tpr_diag.p2_ms = elapsed_ms(tp);
    if (n2 == 0) goto done;

    clock_gettime(CLOCK_MONOTONIC, &tp);
    int n3 = search3(s_p2, n2, &s_p3);
    tpr_diag.p3_ms = elapsed_ms(tp);
    if (n3 == 0) goto done;

    {
        normalize_orientation(&s_p3);
        char facelet54[55];
        fullcube_to_333_facelet(&s_p3, facelet54);
        /* solution() returns NULL for both OLL parity (instant) and timeout;
         * elapsed time distinguishes them. timeOut is in ms. */
#define KOK_PATH     "../4x4-solver/ckociemba/cprunetables"
#define KOK_TIMEOUT_MS 100
        struct timespec t_kok;
        clock_gettime(CLOCK_MONOTONIC, &t_kok);
        char *kok = solution(facelet54, 20, KOK_TIMEOUT_MS, 0, KOK_PATH);
        bool kok_instant = (elapsed_ms(t_kok) < KOK_TIMEOUT_MS / 2.0);

        if (kok == NULL && kok_instant) {
            /* OLL parity: swap one wing-pair (invisible on non-supercube). */
            char tmp = facelet54[7]; facelet54[7] = facelet54[19]; facelet54[19] = tmp;
            kok = solution(facelet54, 20, KOK_TIMEOUT_MS, 0, KOK_PATH);
        }
        if (kok == NULL)
            kok = solution(facelet54, 25, 60000, 0, KOK_PATH);
#undef KOK_TIMEOUT_MS
#undef KOK_PATH
        if (kok) {
            if (kok[0] != 'E') {
                int tpr_len = s_p3.length1 + s_p3.length2 + s_p3.length3;
                int kok_len = 0;
                char *tok = strtok(kok, " ");
                while (tok) {
                    int m = kok_str_to_move(tok);
                    if (m >= 0) { fullcube_do_move(&s_p3, m); kok_len++; }
                    tok = strtok(NULL, " ");
                }
                if (goal_solved(&s_p3)) {
                    r.solved      = true;
                    r.total_moves = tpr_len + kok_len;
                }
            }
            free(kok);
        }
    }

done:
    r.ms   = elapsed_ms(t0);
    r.diag = tpr_diag;
    return r;
}

int main(int argc, char **argv) {
    int n_scrambles = DEFAULT_N;
    if (argc > 1) {
        int v = atoi(argv[1]);
        if (v > 0) n_scrambles = v;
    }

    setbuf(stdout, NULL);

    printf(C_BOLD "=== TPR Benchmark: %d × %d-move scrambles ===" C_RESET "\n\n",
           n_scrambles, SCRAMBLE_LEN);

    unsigned int seed = (unsigned int)time(NULL);
    const char *env_s = getenv("TPR_SEED");
    if (env_s) seed = (unsigned int)strtoul(env_s, NULL, 0);
    printf("Seed: 0x%08X  (override with TPR_SEED=<hex>)\n\n", seed);

    struct timespec init0, init1;
    clock_gettime(CLOCK_MONOTONIC, &init0);
    tpr_init();
    clock_gettime(CLOCK_MONOTONIC, &init1);
    printf("Init: %.1f ms\n\n",
           (init1.tv_sec - init0.tv_sec) * 1e3
           + (init1.tv_nsec - init0.tv_nsec) * 1e-6);

    int idx_digits = count_digits(n_scrambles);
    int col_idx    = idx_digits + 2;   /* '[' + digits + ']' */
    int col_mv     = 5;
    int col_time   = 9;

    printf("  %-*s | %-*s | %-*s | result\n",
           col_idx, "#", col_mv, "moves", col_time, "time");
    printf("  ");
    print_dashes(col_idx);
    printf("-+-");
    print_dashes(col_mv);
    printf("-+-");
    print_dashes(col_time);
    printf("-+-");
    printf("------\n");

    double   *times       = calloc((size_t)n_scrambles, sizeof(double));
    int      *move_counts = calloc((size_t)n_scrambles, sizeof(int));
    TprDiag  *diags       = calloc((size_t)n_scrambles, sizeof(TprDiag));
    int    (*scramble_store)[SCRAMBLE_LEN] =
        calloc((size_t)n_scrambles, SCRAMBLE_LEN * sizeof(int));
    int     n_solved    = 0;

    for (int i = 0; i < n_scrambles; i++) {
        int scramble[SCRAMBLE_LEN];
        gen_scramble(scramble, SCRAMBLE_LEN, &seed);

        FullCube sc;
        fullcube_from_moves(&sc, scramble, SCRAMBLE_LEN);

        memcpy(scramble_store[i], scramble, SCRAMBLE_LEN * sizeof(int));

        BenchResult r = run_one(&sc);
        times[i]       = r.ms;
        move_counts[i] = r.total_moves;
        diags[i]       = r.diag;
        if (r.solved) n_solved++;

        char tmbuf[16];
        if (r.ms < 1000.0) snprintf(tmbuf, sizeof(tmbuf), "%7.1fms", r.ms);
        else                snprintf(tmbuf, sizeof(tmbuf), "%8.2fs",  r.ms / 1000.0);

        char mv_str[8] = "-";
        if (r.solved) snprintf(mv_str, sizeof(mv_str), "%d", r.total_moves);

        printf("  [%*d] | %-*s | %s | %s\n",
               idx_digits, i + 1,
               col_mv, mv_str,
               tmbuf,
               r.solved ? C_GREEN "SOLVED" C_RESET : C_RED "FAIL" C_RESET);
    }

    printf("\n" C_BOLD "=== Stats (%d/%d solved) ===" C_RESET "\n\n",
           n_solved, n_scrambles);

    double t_min = 1e18, t_max = 0.0, t_sum = 0.0;
    double m_min = 1e9,  m_max = 0.0, m_sum = 0.0;
    int    n_mv  = 0;

    for (int i = 0; i < n_scrambles; i++) {
        t_sum += times[i];
        if (times[i] < t_min) t_min = times[i];
        if (times[i] > t_max) t_max = times[i];
        if (move_counts[i] > 0) {
            m_sum += move_counts[i];
            if (move_counts[i] < m_min) m_min = move_counts[i];
            if (move_counts[i] > m_max) m_max = move_counts[i];
            n_mv++;
        }
    }
    double t_mean = t_sum / n_scrambles;
    double t_var  = 0.0;
    for (int i = 0; i < n_scrambles; i++)
        t_var += (times[i] - t_mean) * (times[i] - t_mean);
    double t_std = sqrt(t_var / n_scrambles);

    printf("  Time  (ms):   min %8.1f   max %8.1f   mean %8.1f   std %8.1f\n",
           t_min, t_max, t_mean, t_std);
    if (n_mv > 0) {
        double m_mean = m_sum / n_mv;
        printf("  Moves:        min %8.0f   max %8.0f   mean %8.1f\n",
               m_min, m_max, m_mean);
    }
    printf("\n  Solved: %d / %d  (%.0f%%)\n",
           n_solved, n_scrambles, 100.0 * n_solved / n_scrambles);

    static const struct { double lo, hi; const char *label; } TIME_BINS[] = {
        {0,       10,    "<10ms    "},
        {10,      50,    "10-50ms  "},
        {50,      100,   "50-100ms "},
        {100,     200,   "100-200ms"},
        {200,     300,   "200-300ms"},
        {300,     500,   "300-500ms"},
        {500,     1000,  "0.5-1s   "},
        {1000,    2000,  "1-2s     "},
        {2000,    5000,  "2-5s     "},
        {5000,    10000, "5-10s    "},
        {10000,   30000, "10-30s   "},
        {30000,   60000, "30-60s   "},
        {60000,   1e18,  ">60s     "},
    };
    int n_tbins = (int)(sizeof(TIME_BINS) / sizeof(TIME_BINS[0]));
    int t_hist[13] = {0};

    for (int i = 0; i < n_scrambles; i++)
        for (int b = 0; b < n_tbins; b++)
            if (times[i] >= TIME_BINS[b].lo && times[i] < TIME_BINS[b].hi)
                { t_hist[b]++; break; }

    int t_hist_max = 0;
    for (int b = 0; b < n_tbins; b++)
        if (t_hist[b] > t_hist_max) t_hist_max = t_hist[b];

    printf("\n  Time distribution:\n");
    for (int b = 0; b < n_tbins; b++) {
        printf("    %-9s | ", TIME_BINS[b].label);
        print_bar(t_hist[b], t_hist_max);
        printf(" | %3d  %3d%%\n",
               t_hist[b], t_hist[b] * 100 / n_scrambles);
    }

    if (n_mv > 0) {
        int range    = (int)m_max - (int)m_min + 1;
        int bin_size = (range > 30) ? 2 : 1;
        int bin_lo   = (int)m_min;
        int n_mbins  = (range + bin_size - 1) / bin_size;
        if (n_mbins < 1) n_mbins = 1;

        int *m_hist = calloc((size_t)n_mbins, sizeof(int));
        for (int i = 0; i < n_scrambles; i++) {
            if (move_counts[i] > 0) {
                int b = (move_counts[i] - bin_lo) / bin_size;
                if (b >= 0 && b < n_mbins) m_hist[b]++;
            }
        }

        int m_hist_max = 0;
        for (int b = 0; b < n_mbins; b++)
            if (m_hist[b] > m_hist_max) m_hist_max = m_hist[b];

        printf("\n  Moves distribution:\n");
        for (int b = 0; b < n_mbins; b++) {
            int lo = bin_lo + b * bin_size;
            int hi = lo + bin_size - 1;
            if (bin_size == 1)
                printf("    %3d     | ", lo);
            else
                printf("    %3d-%-3d | ", lo, hi);
            print_bar(m_hist[b], m_hist_max);
            printf(" | %3d  %3d%%\n",
                   m_hist[b], m_hist[b] * 100 / n_scrambles);
        }
        free(m_hist);
    }

    int top_k = (n_scrambles <= 10) ? n_scrambles : 10;
    int *order = calloc((size_t)n_scrambles, sizeof(int));
    for (int i = 0; i < n_scrambles; i++) order[i] = i;
    for (int i = 0; i < top_k; i++) {
        for (int j = i + 1; j < n_scrambles; j++) {
            if (times[order[j]] > times[order[i]]) {
                int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }
        }
    }

    printf("\n" C_BOLD "=== Top %d slowest solves ===" C_RESET "\n\n", top_k);
    for (int r = 0; r < top_k; r++) {
        int idx = order[r];
        printf("  %2d. ", r + 1);
        for (int j = 0; j < SCRAMBLE_LEN; j++) {
            printf("%s", move2str[scramble_store[idx][j]]);
            if (j < SCRAMBLE_LEN - 1) putchar(' ');
        }
        if (times[idx] < 1000.0)
            printf("  |  %.1fms", times[idx]);
        else
            printf("  |  %.2fs",  times[idx] / 1000.0);
        TprDiag *d = &diags[idx];
        printf("  (P1: %.0fms n1=%d | P2: %.0fms | P3: %.0fms  nodes: %lld  bound: %d  h_e: %d  h_c: %d)\n",
               d->p1_ms, d->n1, d->p2_ms, d->p3_ms,
               d->p3_nodes, d->p3_bound, d->h_e_best, d->h_c_best);
    }

    free(order);
    free(scramble_store);
    free(times);
    free(move_counts);
    free(diags);
    return (n_solved == n_scrambles) ? 0 : 1;
}
