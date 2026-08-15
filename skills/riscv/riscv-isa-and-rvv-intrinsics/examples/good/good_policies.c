// GOOD: explicit tail/mask policy for a masked accumulate.
//
// Masked lanes and tail lanes are agnostic under ta,ma. When the result is
// consumed by a reduction over vl lanes only, agnostic is fine. When the result
// feeds a full-register consumer, use tu with pre-initialized values. Both
// choices are explicit here — the anti-pattern is relying on an implicit policy.

#include <riscv_vector.h>

// Masked accumulate into a pre-zeroed accumulator; the consumer reads only the
// first vl lanes, so ta,ma (agnostic masked/tail) is acceptable.
vint32m1_t good_masked_accum(const int32_t *p, vbool32_t mask, size_t n)
{
    size_t vl = vsetvl_e32m1(n);
    vint32m1_t acc = vmv_v_x_i32m1(0, vl);
    vint32m1_t x = vle32_v_i32m1_m(mask, p, vl);
    return vadd_vv_i32m1(acc, x, vl);   // caller reduces over vl lanes only
}

// Tail undisturbed: use TU so lanes past vl keep previous data, and initialize
// the tail explicitly to 0 before use. This is safe for full-register consumers.
vint32m1_t good_tail_undisturbed(const int32_t *p, size_t n)
{
    size_t vl = vsetvl_e32m1_tu(32, n);   // 32 = VLMAX here (explicit TU)
    vint32m1_t v = vmv_v_x_i32m1_tu(vmv_v_x_i32m1(0, 32), 0, n);
    (void)vl;
    return vle32_v_i32m1_tu(v, p, n);
}
