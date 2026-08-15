// GOOD: strided load with byte stride.
//
// vlse takes a byte stride. For float (4 bytes) with elements 4 bytes apart the
// stride is sizeof(float) = 4. To read elements k steps apart (k>1), pass
// k * sizeof(float).

#include <riscv_vector.h>

// Read every k-th float (byte stride = k * 4).
vfloat32m1_t good_strided(const float *base, size_t k, size_t n)
{
    size_t vl = vsetvl_e32m1(n);
    return __riscv_vlse32_v_f32m1(base, (ptrdiff_t)k * (ptrdiff_t)sizeof(float),
                                  vl);
}

// Segmented: read pairs (a[k], b[k]) stored interleaved; nfields = 2, the
// contiguous variant strides by 2*sizeof(float) per element automatically.
vfloat32m1_t good_segmented(const float *base, size_t n)
{
    size_t vl = vsetvl_e32m1(n);
    vfloat32m1_t a, b;
    __riscv_vlsseg2e32_v_f32m1(&a, &b, base, vl);   // 2 fields per element
    return a;
}
