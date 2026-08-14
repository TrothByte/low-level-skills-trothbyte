/* BAD: loops that fail to vectorize at -O2 on x86-64 (GCC 16.1).
 *
 * Inspect with:
 *   gcc -O2 -fopt-info-vec=vec_bad.txt -S loop.c
 *   gcc -O2 -fopt-info-missed-vec=vec_bad_missed.txt -S loop.c
 *   grep -E "vectorized|missed" vec_bad.txt vec_bad_missed.txt
 *
 * Note: -fopt-info-vec alone reports only successful vectorization, so for
 * this file the missed reasons must be read with -fopt-info-missed-vec.
 * Each function isolates one vectorization blocker.
 */

#include <stddef.h>
#include <stdint.h>

/* B1: possible alias between b and c.
 * c[i] = b[i] + 1 writes through c; the const qualifier on b does NOT rule
 * out overlap with c (the underlying object may be non-const), so GCC must
 * assume b and c can alias. At -O2 the loop is not vectorized. At -O3 GCC
 * adds a runtime alias check and vectorizes anyway (loop versioning).
 */
void bad_alias(const int32_t *b, int32_t *c, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        c[i] = b[i] + 1;
    }
}

/* B2: loop-carried dependency (serial prefix sum).
 * a[i] depends on a[i-1] written in the previous iteration (distance 1),
 * so no safe vectorization as written, at any -O level.
 */
void bad_prefix(int32_t *a, size_t n) {
    for (size_t i = 1; i < n; ++i) {
        a[i] += a[i - 1];
    }
}

/* B3: unknown trip count - data-dependent exit condition.
 * The loop may stop anywhere; the iteration count is not computable before
 * execution, which blocks the vectorizer at every -O level.
 */
void bad_unknown_trip(int32_t *a, size_t n, int32_t limit) {
    size_t i = 0;
    while (i < n && a[i] > limit) {
        a[i] = -a[i];
        ++i;
    }
}

/* B4: misaligned access pattern.
 * The store lands at c + (i+1)*4, i.e. 4 bytes past the 16-byte boundary,
 * while the load b + i*4 is aligned. At -O2 the loop is not vectorized;
 * at -O3 it is, but with an unaligned movups store at +4 bytes. See the
 * aligned-store twin good_mul() in examples/good/loop.c.
 */
void bad_unaligned(int32_t *restrict c, const int32_t *restrict b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        c[i + 1] = b[i] * 2;
    }
}

/* Runtime self-check (enabled only for correctness evals).
 *   gcc -O2 -DRUN_CORRECTNESS loop.c -o bad.exe && ./bad.exe
 */
#ifdef RUN_CORRECTNESS
#include <stdio.h>
int main(void) {
    _Alignas(64) int32_t b[64];
    _Alignas(64) int32_t c[64];
    _Alignas(64) int32_t ref[64];
    for (size_t i = 0; i < 64; ++i) b[i] = (int32_t)i - 32;
    bad_alias(b, c, 64);
    for (size_t i = 0; i < 64; ++i) ref[i] = b[i] + 1;
    for (size_t i = 0; i < 64; ++i) {
        if (c[i] != ref[i]) {
            fprintf(stderr, "MISMATCH at %zu\n", i);
            return 1;
        }
    }
    puts("OK");
    return 0;
}
#endif
