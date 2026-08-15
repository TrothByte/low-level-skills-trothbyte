// intentionally incorrect — BAD example: fractional LMUL with SEW over the cap.
//
// Fractional LMUL (mf2/mf4/mf8) is legal only for SEW in [SEW_MIN, LMUL*ELEN]
// (SEW_MIN=8, ELEN=64 in the standard extensions). So:
//   mf2 -> SEW <= 32 ; mf4 -> SEW <= 16 ; mf8 -> SEW <= 8.
// Using SEW=64 with LMUL=mf2 exceeds 64 > 0.5*64 = 32: the vtype config is
// unsupported, `vill` is set and vl is set to 0 — the loop silently processes
// nothing instead of trapping.
//
// Common agent error: "mf2 means half the vector, so SEW=64 works". It does not.
// Compare: examples/good/good_strip_mining.c and the legality table in
// examples/good/sim_vsetvl.py.

#include <riscv_vector.h>

// BUG: SEW=64 with LMUL=mf2. LMUL*ELEN = 0.5*64 = 32 < 64 -> vill/VL=0.
void bad_fractional_sew(const long *p, long *out, size_t n)
{
    size_t vl = vsetvl_e64mf2(n);   // illegal config: 64 > mf2*ELEN = 32
    // vill is set, vl reads 0 -> the loop below never runs.
    vint64m1_t v = vle64_v_i64m1(p, vl);
    vse64_v_i64m1(out, v, vl);
}
