/*
 * GOOD: constant-time compare benchmark — measures wall-clock time of an
 * early-exit memcmp vs a constant-time comparison using a monotonic clock.
 * Demonstrates the timing channel with real numbers (recorded in evals/README.md).
 *
 * Build: gcc -O2 timing_demo.c -o timing_demo
 * Run:   timing_demo  NITER  LEN
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* secret-dependent branch: returns 1 if a == b, exits at the first difference */
static int memcmp_early_exit(const uint8_t *a, const uint8_t *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return 0;
    }
    return 1;
}

/* constant-time: reads every byte, accumulates the difference into one word */
static int memcmp_consttime(const uint8_t *a, const uint8_t *b, size_t n) {
    uint8_t acc = 0;
    for (size_t i = 0; i < n; i++)
        acc |= (uint8_t)(a[i] ^ b[i]);
    return acc == 0;
}

int main(int argc, char **argv) {
    const size_t n = argc > 2 ? (size_t)strtoul(argv[2], NULL, 10) : 64;
    const size_t iters = argc > 1 ? (size_t)strtoul(argv[1], NULL, 10) : 200000;

    uint8_t *a = malloc(n);
    uint8_t *b = malloc(n);
    if (!a || !b) { fprintf(stderr, "alloc failed\n"); return 1; }
    memset(a, 0x11, n);
    memset(b, 0x11, n);

    /* force the result to be used so neither function is optimized away */
    volatile int sink = 0;

    /* 1. early-exit, first byte differs: fastest path */
    b[0] ^= 1;
    double t0 = now_sec();
    for (size_t i = 0; i < iters; i++)
        sink += memcmp_early_exit(a, b, n);
    double t_first = now_sec() - t0;

    /* 2. early-exit, LAST byte differs: must scan the whole buffer */
    b[0] ^= 1; b[n - 1] ^= 1;
    t0 = now_sec();
    for (size_t i = 0; i < iters; i++)
        sink += memcmp_early_exit(a, b, n);
    double t_last = now_sec() - t0;
    b[n - 1] ^= 1;

    /* 3. constant-time, first byte differs */
    b[0] ^= 1;
    t0 = now_sec();
    for (size_t i = 0; i < iters; i++)
        sink += memcmp_consttime(a, b, n);
    double ct_first = now_sec() - t0;

    /* 4. constant-time, last byte differs: same work, same time */
    b[0] ^= 1; b[n - 1] ^= 1;
    t0 = now_sec();
    for (size_t i = 0; i < iters; i++)
        sink += memcmp_consttime(a, b, n);
    double ct_last = now_sec() - t0;

    printf("n=%zu iters=%zu sink=%d\n", n, iters, sink);
    printf("early_exit first-byte-diff: %.6f s\n", t_first);
    printf("early_exit last-byte-diff : %.6f s\n", t_last);
    printf("consttime  first-byte-diff: %.6f s\n", ct_first);
    printf("consttime  last-byte-diff : %.6f s\n", ct_last);
    printf("early-exit leak (last-first): %.6f s\n", t_last - t_first);
    printf("consttime leak (last-first) : %.6f s\n", ct_last - ct_first);

    free(a);
    free(b);
    return 0;
}
