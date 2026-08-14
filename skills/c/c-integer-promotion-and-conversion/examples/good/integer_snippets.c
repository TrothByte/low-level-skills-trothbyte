// GOOD: correct signed/unsigned handling and checked conversions.
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>

static size_t strlen_sz(const char *s);

// G1: handle negative before unsigned comparison.
int compare_good(int i, unsigned u) {
    if (i < 0) return 1;
    return (unsigned)i < u;
}

// G2: checked size_t -> int narrowing.
int narrow_good(const char *s) {
    size_t len = strlen_sz(s);
    if (len > INT_MAX) return -1; // E2BIG
    return (int)len;
}

// G3: checked multiplication before allocation.
char *alloc_good(size_t n) {
    if (n > ((size_t)-1 - 4) / 4 * 3) return NULL;
    return (char *)malloc(n * 4 / 3 + 4);
}

// G4: explicit cast on char inversion.
int invert_good(char c) {
    return (unsigned char)~c; // make intent explicit
}

static size_t strlen_sz(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}
