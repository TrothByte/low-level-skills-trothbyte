// BAD: multiple UB classes in one file (for teaching detection).
// Compile with: clang -O2 -fsanitize=undefined -fno-sanitize-recover=undefined
#include <stddef.h>

// B1: signed overflow — compiler may fold the check to true.
int will_overflow(int x) { return x + 1 > x; }

// B2: shift by >= width — UB.
unsigned shift_bad(unsigned n) { return 1u << n; }

// B3: deleted null check — the !p check may be removed.
int deleted_null_check(int *p) {
    int x = *p;
    if (!p) return 0;
    return x;
}

// B4: off-by-one OOB read.
int sum_bad(int *a, int n) {
    int s = 0;
    for (int i = 0; i <= n; i++) s += a[i]; // reads a[n]
    return s;
}

// B5: overlapping memcpy — UB.
void overlap_bad(char *buf, size_t n) {
    __builtin_memcpy(buf + 1, buf, n); // may overlap
}
