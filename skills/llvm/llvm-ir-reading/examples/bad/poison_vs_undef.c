// BAD: treating poison as undef (or as wrapped arithmetic).
// The IR in poison_vs_undef.ll is what clang emits for this C source.
// Compile with: clang -O1 -S -emit-llvm poison_vs_undef.c -o poison_vs_undef.ll

// Teaching: signed overflow is UB in C; the IR marks the add nsw.
// On x == INT_MAX the IR value is poison, NOT a wrapped INT_MIN.
int checked(int x) {
    int y = x + 1;
    if (y > 0) return 1;
    return 0;
}

// Teaching only: if p points to uninitialized memory, the load is undef in IR,
// and v * v is undef too (each use of v may observe a different value).
int square_uninit(int *p) {
    int v = *p;
    return v * v;
}
