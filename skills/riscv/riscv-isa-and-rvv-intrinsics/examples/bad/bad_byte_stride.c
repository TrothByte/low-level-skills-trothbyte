// intentionally incorrect — BAD example: strided load with element-count stride.
//
// vlse's stride operand is in BYTES. Passing the element count (1) for a 4-byte
// float reads the wrong addresses: base+0, base+4*1=base+4 (which is element 1,
// not the strided element). The agent must convert element stride -> byte stride.
//
// Compare: examples/good/good_strided.c

#include <riscv_vector.h>

// BUG: stride = 1 element, but vlse wants bytes. For float (4 bytes) the
// strided elements are 4 bytes apart, so stride must be 4.
vfloat32m1_t bad_stride(const float *base, size_t n)
{
    size_t vl = vsetvl_e32m1(n);
    return __riscv_vlse32_v_f32m1(base, 1, vl);   // WRONG: stride 1 byte
}
