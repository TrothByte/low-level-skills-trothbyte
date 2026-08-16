// GOOD: differential timing demo — early-exit compare leaks the
// first-difference position; XOR-fold constant-time compare does not.
// Measured over many iterations for a stable contrast. The fold version has
// NO secret-dependent branch (verified by inspecting the emitted asm).
// Compile & run: gcc -O2 ct_compare_demo.c -o /tmp/ct && /tmp/ct
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static uint64_t ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

// GOOD: constant-time comparison — no early exit, one final equality check.
// The loop reads every byte; the only branch is on the folded result.
static int ct_equal(const uint8_t *a, const uint8_t *b, size_t n) {
    uint8_t acc = 0;
    for (size_t i = 0; i < n; i++) acc |= a[i] ^ b[i];
    return acc == 0;
}

// LEAKY: early-exit on the first differing byte — timing depends on the
// secret position. This is the pattern a mitigation MUST remove.
static int early_exit_equal(const uint8_t *a, const uint8_t *b, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

static void bench(const char *name, int (*cmp)(const uint8_t *, const uint8_t *, size_t),
                  const uint8_t *a, const uint8_t *b, size_t n, int iters) {
    uint64_t t0 = ns();
    volatile int acc = 0;
    for (int i = 0; i < iters; i++) acc ^= cmp(a, b, n);
    uint64_t t1 = ns();
    printf("%-28s %8.4f ms (acc=%d)\n", name, (t1 - t0) / 1e6, acc);
}

int main(void) {
    enum { N = 256, ITERS = 2000000 };
    static uint8_t a[N], b_early[N], b_late[N];
    for (int i = 0; i < N; i++) {
        a[i] = (uint8_t)i;
        b_early[i] = (uint8_t)i;
        b_late[i] = (uint8_t)i;
    }
    b_early[1] ^= 1;    // first difference near the START
    b_late[N - 1] ^= 1; // first difference near the END

    printf("n=%d iters=%d (gcc -O2, x86-64)\n", N, ITERS);
    bench("early_exit (early diff)", early_exit_equal, a, b_early, N, ITERS);
    bench("early_exit (late diff) ", early_exit_equal, a, b_late, N, ITERS);
    bench("ct_equal (early diff)  ", ct_equal, a, b_early, N, ITERS);
    bench("ct_equal (late diff)   ", ct_equal, a, b_late, N, ITERS);
    printf("leak = early_exit late-diff minus early-diff; fold delta should be ~0\n");
    return 0;
}
