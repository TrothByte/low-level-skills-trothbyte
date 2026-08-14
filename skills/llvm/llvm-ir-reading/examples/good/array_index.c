// GOOD: reading an array-sum loop with phi and GEP.
// The IR in array_index.ll is what clang emits for this C source.
// Compile with: clang -O1 -S -emit-llvm array_index.c -o array_index.ll

int dot(int *a, int *b, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}
