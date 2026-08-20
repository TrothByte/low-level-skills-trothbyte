/* BAD: this program's behavior silently CHANGES when compiled with
   -ffast-math. -ffast-math implies -fno-math-errno (sqrt(-1.0) no longer
   sets errno), -ffinite-math-only (NaN/Inf assumed absent), and
   -fassociative-math (reassociation). Auditing is required before any
   -ffast-math use; here the IEEE-correct build and the -ffast-math build
   print different lines. */
#include <errno.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    errno = 0;
    volatile double neg = -1.0;
    double r = sqrt(neg);                 /* IEEE: returns NaN, errno = EDOM */
    printf("sqrt(-1.0): isnan=%d errno==EDOM=%d\n",
           (int)isnan(r), errno == EDOM);

    double big = 1e308;
    double o = (big * 10.0) / 10.0;       /* IEEE: overflows to Inf first    */
    printf("(1e308*10)/10 = %g %s\n", o,
           isinf(o) ? "(IEEE overflow)" : "(reassociated, no overflow)");

    return 0;
}
