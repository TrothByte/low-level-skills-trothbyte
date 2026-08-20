/* BAD: exact FP equality on decimal arithmetic results. 0.1, 0.2, 0.3 are
   NOT exactly representable in binary, so 0.1 + 0.2 == 0.3 is false even
   though the decimal arithmetic "looks right". Also demonstrates a float
   variable fed a double literal without an f suffix (silent extra rounding
   at assignment). Use tolerance-based or integer comparison instead. */
#include <stdio.h>

int main(void) {
    double a = 0.1;
    double b = 0.2;
    double sum = a + b;

    if (sum == 0.3) {
        printf("BUG HIDDEN: 0.1 + 0.2 == 0.3 evaluated true\n");
        return 0;
    }

    printf("FALSE: 0.1 + 0.2 == 0.3 (%.17g + %.17g = %.17g)\n", a, b, sum);

    float fv = 0.1;   /* BAD: double literal assigned to float, no f suffix */
    printf("float(0.1) = %.9g\n", fv);

    return 1; /* equality check failed: the correct outcome for == */
}
