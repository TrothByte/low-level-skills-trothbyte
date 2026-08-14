// BAD: signed/unsigned mixing and size_t narrowing hazards.
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>

static size_t strlen_sz(const char *s);

// B1: signed/unsigned comparison surprise — the loop body never runs for i < 0.
int compare_bad(int i, unsigned u) { return i < u; } // i becomes UINT_MAX when negative

// B2: size_t narrowing to int (32-bit hazard).
int narrow_bad(const char *s) {
    int len = (int)strlen_sz(s); // if > INT_MAX, negative
    return len;
}

// B3: multiplication before allocation (integer overflow).
char *alloc_bad(size_t n) {
    return (char *)malloc(n * 4 / 3 + 4); // overflows for large n
}

// B4: promotion of char in bitwise.
int invert_bad(char c) {
    return ~c; // sign-extends to int
}

// helper to keep the snippet self-contained
static size_t strlen_sz(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}
