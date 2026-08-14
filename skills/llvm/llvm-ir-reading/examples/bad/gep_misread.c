// BAD: misreading getelementptr offsets.
// The IR in gep_misread.ll is what clang emits for this C source.
// Compile with: clang -O1 -S -emit-llvm gep_misread.c -o gep_misread.ll

struct Point { int x; int y; };
struct Rect { struct Point p; int w; int h; };

// Returns field w. Field w lives at byte offset 8:
// struct Point (2 x i32) occupies bytes 0..7.
int get_w(struct Rect *r) { return r->w; }

// Returns a[2]. Element 2 of an int array is at byte offset 2 * 4 = 8.
int array_second(int *a) { return a[2]; }
