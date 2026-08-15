// intentionally incorrect — BAD example: hard-coded VL instead of vsetvl.
//
// VLEN is implementation-defined (128..65536). `vsetvl` returns the actual VL =
// min(AVL, VLMAX); here the VL is fixed at 8. On VLEN=256 (VLMAX=4 for SEW=64,
// LMUL=1) the load reads 8 elements but only 4 fit — over-read past the tail,
// and the loop advances by 8 instead of 4, skipping half the data.
//
// Compare: examples/good/good_strip_mining.c

#include <riscv_vector.h>

// BUG: hard-coded VL=8; on VLEN=256 with SEW=64 the real VL is 4.
void bad_fixed_vl(const long *p, long *out, size_t n)
{
    const long *src = p;
    long *dst = out;
    for (size_t i = 0; i < n; i += 8) {
        vint64m1_t v = vle64_v_i64m1(src + i, 8);   // WRONG VL
        vse64_v_i64m1(dst + i, v, 8);
    }
}
