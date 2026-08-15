// intentionally incorrect — BAD example: reduction over an agnostic tail.
//
// With the default (agnostic) tail/mask policy, lanes past VL hold arbitrary
// values. The code below reduces over the FULL register `vredsum` with a
// pre-loaded accumulator that was initialized with vmv.v.x (only vl lanes set),
// and the loop may read the agnostic tail lanes — garbage enters the sum.
//
// Compare: examples/good/good_policies.c

#include <riscv_vector.h>

// BUG: reduction reads the full register including agnostic tail lanes.
long bad_reduce_tail(const long *p, size_t n)
{
    size_t vl = vsetvl_e64m1(n);
    vint64m1_t acc = vmv_v_x_i64m1(0, vl);
    // vredsum reduces over the whole vector group; tail lanes past vl are
    // agnostic (may be non-zero) with the default policy -> garbage in the sum.
    vint64m1_t sum = vredsum_vs_i64m1_i64m1(acc, vle64_v_i64m1(p, vl), acc, vl);
    return vmv_x_s_i64m1_i64(sum);
}
