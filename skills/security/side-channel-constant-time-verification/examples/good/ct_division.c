/*
 * GOOD: constant-time-ish division by avoiding value-dependent division.
 * Reciprocal multiplication plus shift has fixed latency per instruction,
 * unlike hardware division that can take a value-dependent number of cycles.
 * NOTE: it still reads `secret`; the point is the *timing* of the computation
 * no longer depends on the secret value's magnitude.
 */
#include <stdint.h>

static uint32_t approx_scale(uint32_t x) {
    /* 1/3 via multiply-high-by-magic + shift; fixed instruction count */
    return (uint32_t)(((uint64_t)x * 2863311531ull) >> 33);
}

uint32_t ct_scale(uint32_t secret) {
    return approx_scale(secret);
}
