// GOOD: same operations, written without UB.

// G1: unsigned wrap is well-defined — keeps the real check.
int wrap_check(unsigned x) { return x + 1 > x; }

// G2: check before deref.
int check_first(int *p) {
    if (!p) return 0;
    return *p;
}

// G3: check divisor before dividing.
int div_check_first(int x, int y) {
    if (y == 0) return -1;
    return x / y;
}

// G4: observable spin — volatile flag makes it well-defined.
void spin_observable(volatile int *stop) {
    while (!*stop) { }
}
