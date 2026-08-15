// intentionally incorrect
// BAD: NEON per-lane accumulator with no overflow guard. Each 32-bit lane of
// v0 adds 1 every iteration; a 32-bit lane overflows after 2^32 iterations,
// and 16-bit lanes after ~65536. The Lemire failure mode: a SIMD loop that
// "should work" silently wraps per lane unless you either use a wide lane or
// periodically reduce (horizontal add) into a scalar guard. Here, for
// uint32 lanes the wrap is invisible until the sum is consumed.
#include <arm_neon.h>

uint32_t bad_neon_sum(const uint32_t *src, size_t n) {
    uint32x4_t acc = vdupq_n_u32(0);
    for (size_t i = 0; i < n; i += 4) {
        uint32x4_t v = vld1q_u32(src + i);
        acc = vaddq_u32(acc, v);   // per-lane add; lanes can overflow
    }
    uint32_t tmp[4];
    vst1q_u32(tmp, acc);
    return tmp[0] + tmp[1] + tmp[2] + tmp[3];
}
