/*
 * BAD: // intentionally incorrect — a classic strict-aliasing violation
 * (C11 6.5p7). A union object is written through an int* and read through
 * a double* in the same function. Under -fstrict-aliasing the compiler
 * assumes the pointers do not alias: after *d = 3.14 it elides the
 * reload of *i and returns 0. With -fno-strict-aliasing it reloads and
 * returns the double's bit pattern reinterpreted as int — demonstrating
 * why "works in my test" is not correctness.
 *
 * Build (executed on this host):
 *   gcc -O2 strict_aliasing_violation.c -o alibad && alibad
 *   gcc -O2 -fno-strict-aliasing strict_aliasing_violation.c -o alibad2 && alibad2
 */
#include <stdio.h>

static int f(int *i, double *d) {
    *i = 0;
    *d = 3.14;              /* // intentionally incorrect: aliases *i */
    return *i;              /* may be elided under -fstrict-aliasing */
}

int main(void) {
    union { int i; double d; } u;
    u.i = 0;
    printf("result=%d\n", f(&u.i, &u.d));
    /* -O2: 0 (compiler assumes no aliasing)
       -fno-strict-aliasing: 1374389535 (reloaded bit pattern) */
    return 0;
}
