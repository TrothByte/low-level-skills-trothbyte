// GOOD: the -O2 assembly in artifacts_O2.s is read with artifact recognition,
// then compared against -O0 to confirm semantics.
// Correct reading:
//   - tail: `jmp helper` is a tail call. Same argument in %ecx, result returned
//     by helper directly. Equivalent to `call helper; ret` (see -O0).
//   - mul3: `leal (%rcx,%rcx,2), %eax` computes 3*x via address arithmetic.
//   - fold: `movl $14, %eax` is constant folding (present at -O0 too).
//   - dce: `leal 1(%rcx), %eax` only; the x*100 was dead (never read).
//   - caller: `leal 2(%rcx,%rcx), %eax` = inlined inline_me fused with *2.
//   - pic_read: RIP-relative address load + data load; that is the global read.
// Verification: gcc -O0 -S artifacts.c -o artifacts_O0.s
//               gcc -O2 -S artifacts.c -o artifacts_O2.s ; diff

extern int helper(int);
int tail(int x) { return helper(x); }

int mul3(int x) { return x * 3; }

int fold(void) { return 2 + 3 * 4; }

int dce(int x) { int dead = x * 100; return x + 1; }

static int inline_me(int x) { return x + 1; }
int caller(int x) { return inline_me(x) * 2; }

extern int g;
int pic_read(void) { return g; }
