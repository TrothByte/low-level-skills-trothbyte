// GOOD: VL-independent strip-mining loop.
//
// vl = vsetvl_e64m1(remaining) is recomputed every iteration; the returned vl
// drives both the load and the index advance. Correct for ANY VLEN (128..65536)
// and any n, including partial tails. Uses the returned VL only, never a
// constant.
//
// Target toolchain: clang --target=riscv64-unknown-elf -march=rv64gcv (absent on
// this machine; documented command). The vsetvl VL math is exercised by
// examples/good/sim_vsetvl.py.

#include <riscv_vector.h>

void good_strip_mining(const long *p, long *out, size_t n)
{
    const long *src = p;
    long *dst = out;
    for (size_t i = 0; i < n; ) {
        size_t vl = vsetvl_e64m1(n - i);     // vl = min(n - i, VLMAX)
        vint64m1_t v = vle64_v_i64m1(src + i, vl);
        vse64_v_i64m1(dst + i, v, vl);
        i += vl;
    }
}
