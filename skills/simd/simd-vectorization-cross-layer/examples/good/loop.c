/* GOOD: the same computations, written so the compiler can vectorize.
 *
 * Inspect with:
 *   gcc -O2 -fopt-info-vec=vec.txt -S loop.c
 *   grep -E "vectorized|missed" vec.txt
 *   grep -E "movdqa|movdqu|movups|paddd|pslld" loop.s
 *
 * Verified on GCC 16.1 (MinGW, x86-64, generic target): at -O2 only the
 * simple copy-style loops (G1, G2) vectorize with 16-byte vectors; the
 * reduction (G3) vectorizes at -O3, and the guarded loop (G4) vectorizes
 * with -mavx2 (32-byte vectors). GCC emits unaligned movdqu/movups even
 * for _Alignas(64) data; aligned moves do not appear on this target.
 */

#include <stddef.h>
#include <stdint.h>

/* G1: restrict tells GCC that b and c do not overlap, so the loop is
 * vectorized at -O2. Identical loop body to bad/loop.c bad_alias().
 */
void good_alias(const int32_t *restrict b, int32_t *restrict c, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        c[i] = b[i] + 1;
    }
}

/* G2: externally visible aligned buffers. _Alignas(64) on the objects gives
 * GCC known alignment and a known 1024-iteration trip count, so the loop
 * vectorizes with no scalar prologue or epilogue. Note: on modern x86-64
 * the generated memory accesses are still movdqu/movups, not movdqa.
 * (If these were `static` and unused, dead-code elimination would delete
 * the whole loop before the vectorizer could see it.)
 */
_Alignas(64) int32_t g_a[1024];
_Alignas(64) int32_t g_b[1024];
_Alignas(64) int32_t g_c[1024];

void good_aligned(void) {
    for (size_t i = 0; i < 1024; ++i) {
        g_c[i] = g_a[i] + g_b[i];
    }
}

/* G3: reduction is a vectorizable pattern. On this toolchain the int
 * reduction vectorizes at -O3 (accumulator in xmm, horizontal add in the
 * epilogue) but NOT at the default -O2 cost model.
 */
int32_t good_reduction(const int32_t *restrict a, size_t n) {
    int32_t sum = 0;
    for (size_t i = 0; i < n; ++i) {
        sum += a[i];
    }
    return sum;
}

/* G4: affine trip-count formulation of bad/loop.c bad_unknown_trip().
 * The data-dependent early exit is replaced by a full-range pass with a
 * per-element guard. With -O2 -mavx2 this vectorizes using 32-byte vectors
 * (unroll 8); at -O2/-O3 on plain SSE2 it stays scalar.
 */
void good_affine_trip(int32_t *restrict a, size_t n, int32_t limit) {
    for (size_t i = 0; i < n; ++i) {
        if (a[i] > limit) {
            a[i] = -a[i];
        }
    }
}

/* G5: aligned-store twin of bad/loop.c bad_unaligned(). The store lands on
 * 16-byte boundaries (c + i*4). On this toolchain BOTH versions stay scalar
 * at -O2; at -O3 both vectorize, bad_unaligned with a misaligned movups
 * store at +4 bytes, this one with a store at +0.
 */
void good_mul(int32_t *restrict c, const int32_t *restrict b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        c[i] = b[i] * 2;
    }
}

/* G6: pointers whose alignment is proven to the compiler with
 * __builtin_assume_aligned(p, 16). The loop vectorizes at -O3, but even
 * then GCC 16.1 on x86-64 keeps using movdqu/movups: unaligned SIMD
 * access is the default, not a fallback.
 */
void good_assume_aligned(const int32_t *b, int32_t *c, size_t n) {
    const int32_t *bb = (const int32_t *)__builtin_assume_aligned(b, 16);
    int32_t *cc = (int32_t *)__builtin_assume_aligned(c, 16);
    for (size_t i = 0; i < n; ++i) {
        cc[i] = bb[i] + 1;
    }
}

/* Runtime self-check (enabled only for correctness evals).
 *   gcc -O2 -DRUN_CORRECTNESS loop.c -o good.exe && ./good.exe
 */
#ifdef RUN_CORRECTNESS
#include <stdio.h>
int main(void) {
    _Alignas(64) int32_t b[64];
    _Alignas(64) int32_t c[64];
    _Alignas(64) int32_t ref[64];
    for (size_t i = 0; i < 64; ++i) b[i] = (int32_t)i - 32;
    good_alias(b, c, 64);
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
