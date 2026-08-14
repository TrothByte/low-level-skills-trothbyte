/* BAD: loops that fail to vectorize (GCC 16.1, x86-64, generic target).
 *
 * Each function isolates one vectorization blocker. Read the reasons with
 * -fopt-info-missed-vec; -fopt-info-vec alone reports only successes, so
 * it will be empty for this file by design.
 *
 *   gcc -O2 -fopt-info-vec=vec_bad.txt -S loops.c
 *   gcc -O2 -fopt-info-missed-vec=vec_bad_missed.txt -S loops.c
 */

#include <stddef.h>
#include <stdint.h>

/* B1: aliasing blocker. c[i] = b[i] + 1 writes through c; const on the
 * pointee of b does NOT prove b and c are disjoint (the underlying object
 * may still be reachable through c). Without restrict the compiler must
 * assume overlap and cannot reorder the load and the store.
 */
void bad_alias(const int32_t *b, int32_t *c, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        c[i] = b[i] + 1;
    }
}

/* B2: loop-carried dependency, distance 1. a[i] reads the value written in
 * the previous iteration, so the iterations are ordered and no reordering is
 * legal. This is a serial prefix sum; it needs a scan algorithm, not a
 * keyword.
 */
void bad_prefix(int32_t *a, size_t n) {
    for (size_t i = 1; i < n; ++i) {
        a[i] += a[i - 1];
    }
}

/* B3: unknown trip count. The exit condition depends on runtime data
 * (a[i] > limit), so the iteration count is not computable before execution.
 */
void bad_unknown_trip(int32_t *a, size_t n, int32_t limit) {
    size_t i = 0;
    while (i < n && a[i] > limit) {
        a[i] = -a[i];
        ++i;
    }
}

/* B4: non-affine induction variable. The store index i*i grows quadratically
 * in the loop counter; the access pattern is not an affine function of i, so
 * the vectorizer cannot group consecutive elements into a vector.
 */
void bad_non_affine(int32_t *restrict a, const int32_t *restrict b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        a[i * i] = b[i] + 1;
    }
}
