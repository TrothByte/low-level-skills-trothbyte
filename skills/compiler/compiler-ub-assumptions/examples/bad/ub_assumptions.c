// BAD: UB that the optimizer exploits at -O2.
// Compile: gcc -O0 -S ub_assumptions.c ; gcc -O2 -S ub_assumptions.c ; diff

// B1: signed overflow — comparison folded to constant 1.
int check_after_overflow(int x) { return x + 1 > x; }

// B2: null deref before null check — check deleted.
int check_after_deref(int *p) {
    int x = *p;
    if (!p) return 0; // dead: deref proved p non-null
    return x;
}

// B3: division before zero-check — check deleted.
int check_after_div(int x, int y) {
    int r = x / y;
    if (y == 0) return -1; // dead: UB implies y != 0
    return r;
}

// B4: empty infinite loop (C++ removes it; in C11 it may be assumed to terminate).
void spin(void) {
    for (;;) { } // no I/O, no volatile, no atomics
}
