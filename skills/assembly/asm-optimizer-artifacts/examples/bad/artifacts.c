// BAD: the -O0 assembly in artifacts_O0.s is treated as "the program".
// Analysis errors this skill teaches you to avoid:
//   - "artifacts_O0.s stores to 16(%rbp) and reloads, so tail() has a data race"
//     -> WRONG: -O0 spills arguments to the stack by design (codegen style artifact).
//   - "at -O2 tail() only emits `jmp helper`, so helper() is never called"
//     -> WRONG: it is a tail call; the call happens, just without a new frame.
//   - "I wrote x*3, the -O2 asm has no imul, the multiply is gone"
//     -> WRONG: it is strength-reduced to leal (%rcx,%rcx,2).
// The -O0 .s and -O2 .s are BOTH correct implementations of this source.

extern int helper(int);
int tail(int x) { return helper(x); } // -O0: call+ret ; -O2: jmp helper

int mul3(int x) { return x * 3; } // -O0: two adds ; -O2: leal (%rcx,%rcx,2)

int fold(void) { return 2 + 3 * 4; } // -O0 AND -O2: movl $14, %eax (constant folding)

int dce(int x) { int dead = x * 100; return x + 1; } // -O2: imull eliminated (DCE)

static int inline_me(int x) { return x + 1; }
int caller(int x) { return inline_me(x) * 2; } // -O2: inlined to leal 2(%rcx,%rcx)

extern int g;
int pic_read(void) { return g; } // both levels: movq .refptr.g(%rip), %rax; movl (%rax)
