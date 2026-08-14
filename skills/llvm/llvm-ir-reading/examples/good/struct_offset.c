// GOOD: struct field offset exercise.
// The IR in struct_offset.ll is what clang emits for this C source.
// Compile with: clang -O1 -S -emit-llvm struct_offset.c -o struct_offset.ll

struct Item { char tag; int id; long score; };

long get_score(struct Item *p) { return p->score; }
