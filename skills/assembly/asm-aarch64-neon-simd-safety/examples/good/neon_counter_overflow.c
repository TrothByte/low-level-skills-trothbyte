// GOOD: per-lane overflow guard. The 32-bit lanes still wrap individually, so
// the scalar guard reduces every N iterations (well below 2^32/step) into a
// wider accumulator, or you keep the guard count in a 64-bit lane. This is the
// Lemire-advised pattern: horizontal reduce + guard instead of trusting lanes.
#include <arm_neon.h>
#include <stdint.h>
#include <stddef.h>

#define GUARD_ITERS 65536

uint64_t good_neon_sum(const uint32_t *src, size_t n) {
    uint32x4_t acc = vdupq_n_u32(0);
    uint64_t total = 0;
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        uint32x4_t v = vld1q_u32(src + i);
        acc = vaddq_u32(acc, v);
        if ((i & (GUARD_ITERS * 4 - 1)) == GUARD_ITERS * 4 - 4) {
            uint32_t tmp[4];
            vst1q_u32(tmp, acc);              // horizontal reduce to scalar
            total += (uint64_t)tmp[0] + tmp[1] + tmp[2] + tmp[3];
            acc = vdupq_n_u32(0);
        }
    }
    uint32_t tmp[4];
    vst1q_u32(tmp, acc);
    total += (uint64_t)tmp[0] + tmp[1] + tmp[2] + tmp[3];
    return total;
}
