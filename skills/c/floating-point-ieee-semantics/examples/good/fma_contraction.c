/* GOOD practice for FMA contraction: -ffp-contract=fast fuses a*b+c into a
   single rounding step (vfmadd), which CHANGES the result compared with
   separate multiply then add. This program prints DIFFERENT values when
   compiled with -ffp-contract=fast -mfma vs -ffp-contract=off (or default).
   If bit-identical results are required, pin -ffp-contract=off and document
   it; never rely on the default target-dependent behavior. */
#include <stdio.h>

int main(void) {
    volatile double a = 0x1.fffffffffffecp-1;
    volatile double b = 0x1.fffffffffffecp-1;
    volatile double c = 0x1.ffffffffffff9p-1;
    double r = a * b + c;
    printf("a*b+c = %a = %.17g\n", r, r);
    return 0;
}
