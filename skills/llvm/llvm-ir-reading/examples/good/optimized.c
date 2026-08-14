// GOOD: reading optimized (pass-modified) IR with attributes and intrinsics.
// The IR in optimized.ll is representative of clang -O2 output.
// Compile with: clang -O2 -S -emit-llvm optimized.c -o optimized.ll

typedef struct { int x; int y; } V2;

// restrict in C becomes noalias in the IR.
V2 move(V2 *restrict dst, const V2 *restrict src) {
    V2 r = *src;
    *dst = r;
    return r;
}

// Whole-struct assignment lowers to the llvm.memcpy intrinsic.
struct Item { char tag; int id; long score; };
void copy_item(struct Item *d, const struct Item *s) { *d = *s; }
