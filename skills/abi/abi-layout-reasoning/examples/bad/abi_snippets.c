// BAD: ABI assumptions that are wrong on SysV AMD64.
#include <stddef.h>

// B1: hand-computed struct size — wrong (expect 8, not 5).
struct S1 { char c; int i; };
// off_c=0, off_i=4, size=8 (3 padding bytes), align=4

// B2: assumption that a big struct by value "fits in registers".
struct Big { long a, b, c, d, e; }; // 40 bytes → MEMORY class, passed on the stack
long f_pass(struct Big b) { return b.a + b.e; }

// B3: unaligned access assumed safe (strict on ARM, tolerated on x86).
struct Packed { char tag; int val; } __attribute__((packed));
int read_packed(const struct Packed *p) { return p->val; } // may be unaligned

// B4: return pointer to local — lifetime bug (see c-undefined-behavior).
long *ret_local(void) { long x = 5; return &x; }
