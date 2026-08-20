// BAD: signed integer overflow, exploitable by the optimizer at -O2.
//
// `check(INT_MAX)` computes `y = x + 1`, which overflows a signed int.
// Overflow is undefined behavior (C11 6.5p5), so the optimizer may assume
// it never happens and fold `y > x` to constant true.
//
// Verified divergence (gcc 16.1 MinGW, this host):
//   gcc -O0  -> "no-overflow"      rc=0   (runtime wrap: x+1 = INT_MIN, not > x)
//   gcc -O2  -> "overflow-detected" rc=1   (check folded: x+1 > x is always true)
//
// The same check written on an unsigned int is well-defined and folds nowhere.

#include <limits.h>
#include <stdio.h>

static int check(int x) {
    int y = x + 1;
    return y > x;
}

int main(void) {
    if (check(INT_MAX)) {
        printf("overflow-detected\n");
        return 1;
    } else {
        printf("no-overflow\n");
        return 0;
    }
}
