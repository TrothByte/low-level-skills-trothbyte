// BAD: phi misuse — reading phi as a mutable variable.
// The IR in phi_misuse.ll is what clang emits for this C source.
// Compile with: clang -O1 -S -emit-llvm phi_misuse.c -o phi_misuse.ll

int sum(int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += i;
    return s;
}
