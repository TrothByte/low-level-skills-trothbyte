/* GOOD: the same computations, written so the vectorizer can prove what it
 * needs to. Each function pairs with a blocker in examples/bad/loops.c.
 *
 *   gcc -O2 -fopt-info-vec=vec_good.txt -S loops.c
 *   grep -E "vectorized" vec_good.txt
 *
 * Runtime correctness self-check (enabled for evals):
 *   gcc -O2 -DRUN_CORRECTNESS loops.c -o good.exe && ./good.exe
 */

#include <stddef.h>
#include <stdint.h>

/* G1: aliasing fixed. Identical body to bad_alias(); restrict on both
 * pointers is the compiler's proof that the buffers are disjoint, so the
 * load-store pair is independent and the loop vectorizes.
 */
void good_restrict(const int32_t *restrict b, int32_t *restrict c, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        c[i] = b[i] + 1;
    }
}

/* G2: unknown trip count fixed. The data-dependent early exit of
 * bad_unknown_trip() is replaced by a full-range affine loop with a
 * per-element guard. The trip count n is computable before execution, which
 * is all the vectorizer requires.
 */
void good_affine_guard(int32_t *restrict a, size_t n, int32_t limit) {
    for (size_t i = 0; i < n; ++i) {
        if (a[i] > limit) {
            a[i] = -a[i];
        }
    }
}

/* G3: reduction. sum += a[i] is a recognized pattern: a single accumulator
 * with an associative operator. The compiler reassociates the additions,
 * vectorizes, and reduces the partial sums in the epilogue. Contrast with
 * bad_prefix(), where every iteration feeds the next one.
 */
int32_t good_reduction(const int32_t *restrict a, size_t n) {
    int32_t sum = 0;
    for (size_t i = 0; i < n; ++i) {
        sum += a[i];
    }
    return sum;
}

/* G4: known trip count and alignment. Constant bounds on aligned globals give
 * the vectorizer a compile-time trip count and known alignment. The extern
 * linkage keeps the loop alive across the compile.
 */
_Alignas(64) int32_t g_a[1024];
_Alignas(64) int32_t g_b[1024];
_Alignas(64) int32_t g_c[1024];

void good_known_bounds(void) {
    for (size_t i = 0; i < 1024; ++i) {
        g_c[i] = g_a[i] + g_b[i];
    }
}

/* G5: ivdep pragma. #pragma GCC ivdep tells the vectorizer to ignore the
 * (conservatively assumed) dependence between b and c. The aliasing loop
 * vectorizes without restrict, but the pragma is a promise: overlapping
 * buffers produce wrong results, and unlike restrict it changes nothing
 * about the caller contract.
 */
void good_ivdep(const int32_t *b, int32_t *c, size_t n) {
    #pragma GCC ivdep
    for (size_t i = 0; i < n; ++i) {
        c[i] = b[i] + 1;
    }
}

/* Runtime self-check: every good loop must produce the same results as the
 * scalar reference implementation.
 */
#ifdef RUN_CORRECTNESS
#include <stdio.h>

static int32_t scalar_reduction(const int32_t *a, size_t n) {
    int32_t sum = 0;
    for (size_t i = 0; i < n; ++i) {
        sum += a[i];
    }
    return sum;
}

static void scalar_affine_guard(int32_t *a, size_t n, int32_t limit) {
    for (size_t i = 0; i < n; ++i) {
        if (a[i] > limit) {
            a[i] = -a[i];
        }
    }
}

int main(void) {
    _Alignas(64) int32_t b[64];
    _Alignas(64) int32_t c[64];
    _Alignas(64) int32_t ref[64];
    for (size_t i = 0; i < 64; ++i) {
        b[i] = (int32_t)i - 32;
        ref[i] = b[i] + 1;
    }
    good_restrict(b, c, 64);
    for (size_t i = 0; i < 64; ++i) {
        if (c[i] != ref[i]) {
            fprintf(stderr, "G1 MISMATCH at %zu\n", i);
            return 1;
        }
    }
    if (good_reduction(b, 64) != scalar_reduction(b, 64)) {
        fprintf(stderr, "G3 MISMATCH\n");
        return 1;
    }
    for (size_t i = 0; i < 64; ++i) {
        c[i] = (int32_t)i - 32;
    }
    good_affine_guard(c, 64, 0);
    for (size_t i = 0; i < 64; ++i) {
        ref[i] = (int32_t)i - 32;
    }
    scalar_affine_guard(ref, 64, 0);
    for (size_t i = 0; i < 64; ++i) {
        if (c[i] != ref[i]) {
            fprintf(stderr, "G2 MISMATCH at %zu\n", i);
            return 1;
        }
    }
    for (size_t i = 0; i < 1024; ++i) {
        g_a[i] = (int32_t)i;
        g_b[i] = 2;
    }
    good_known_bounds();
    for (size_t i = 0; i < 1024; ++i) {
        if (g_c[i] != (int32_t)i + 2) {
            fprintf(stderr, "G4 MISMATCH at %zu\n", i);
            return 1;
        }
    }
    puts("OK");
    return 0;
}
#endif
