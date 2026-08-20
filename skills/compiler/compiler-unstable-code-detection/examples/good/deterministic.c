// GOOD: well-defined code, no undefined behavior, no implementation-defined
// traps. Behavior is identical at every -O level. PASS marker.
//
// Verified (gcc 16.1 MinGW, this host):
//   gcc -O0  -> "PASS factorial(12)=479001600"  rc=0
//   gcc -O2  -> "PASS factorial(12)=479001600"  rc=0
//
// Compiles clean with -Wall -Wextra -Wpedantic -Werror.

#include <stdio.h>

static unsigned long factorial(int n) {
    unsigned long r = 1;
    for (int i = 2; i <= n; i++) {
        r *= (unsigned long)i; /* unsigned arithmetic wraps, well-defined */
    }
    return r;
}

int main(void) {
    unsigned long r = factorial(12);
    printf("PASS factorial(12)=%lu\n", r);
    return (r == 479001600UL) ? 0 : 2;
}
