// GOOD: the same operations, written without UB.
#include <stddef.h>
#include <string.h>

// G1: unsigned wrap is well-defined.
int will_overflow_fixed(unsigned x) { return x + 1 > x; }

// G2: guard the shift count.
unsigned shift_good(unsigned n) {
    if (n >= sizeof(unsigned) * 8) return 0;
    return 1u << n;
}

// G3: check before dereference.
int null_check_fixed(int *p) {
    if (!p) return 0;
    return *p;
}

// G4: correct loop bound.
int sum_good(const int *a, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}

// G5: memmove for possibly-overlapping buffers.
void overlap_good(char *buf, size_t n) {
    memmove(buf + 1, buf, n);
}
