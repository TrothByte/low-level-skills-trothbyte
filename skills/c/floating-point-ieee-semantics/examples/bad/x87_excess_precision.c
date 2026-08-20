/* BAD: assumes one FP evaluation model across ABIs and optimization levels.
   On x87 (-mfpmath=387) the expression x + y + z is evaluated with 80-bit
   extended intermediates and ONE final rounding to double; on SSE (the
   x86-64 default) each addition rounds to 64-bit. The SAME source yields
   different results depending on the target's FP stack model. Excess
   precision also depends on register allocation: a value kept in an x87
   register is not rounded, a store to a double is. */
#include <stdio.h>

int main(void) {
    volatile double x = 0.1, y = 0.2, z = 0.3;
    double r = x + y + z;   /* one store to double, intermediate precision ABI-dependent */
    printf("0.1+0.2+0.3 = %.17g\n", r);
    return 0;
}
