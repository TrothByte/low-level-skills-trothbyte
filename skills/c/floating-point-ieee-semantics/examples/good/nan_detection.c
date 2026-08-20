/* GOOD: correct NaN detection, sign-of-zero handling, and tolerance-based
   comparison. x != x is true iff x is NaN (canonical idiom); isnan(x) is
   the portable form. x == NAN is ALWAYS false and must never be used (shown
   below on a line marked FP_CHECK_ANTIPATTERN_DEMO). 1.0/+0.0 = +Inf,
   1.0/-0.0 = -Inf: sign of zero is observable, so never rely on x == 0.0 to
   distinguish the sign. Tolerance comparisons must be derived from the
   error bound of the computation, not a magic constant. */
#include <math.h>
#include <stdio.h>

static int is_nan_manual(double v) { return v != v; }

static int close_enough(double a, double b) {
    double scale = fmax(1.0, fmax(fabs(a), fabs(b)));
    return fabs(a - b) <= scale * 1e-9;   /* relative, magnitude-scaled */
}

int main(void) {
    double nan = 0.0 / 0.0;

    printf("x != x detects NaN: %d (isnan: %d)\n",
           is_nan_manual(nan), (int)isnan(nan));
    printf("1.0/+0.0 = %g   1.0/-0.0 = %g\n", 1.0 / 0.0, 1.0 / -0.0);

    /* FP_CHECK_ANTIPATTERN_DEMO line below: x == NAN is always false */
    printf("x == NAN would be false: %d\n", (nan == NAN)); /* FP_CHECK_ANTIPATTERN_DEMO */

    double sum = 0.1 + 0.2;
    printf("close_enough(0.1+0.2, 0.3): %d\n", close_enough(sum, 0.3));
    return 0;
}
