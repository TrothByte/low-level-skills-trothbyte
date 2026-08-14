// GOOD: ABI reasoning made explicit and verified.
#include <stddef.h>
#include <stdio.h>

struct S1 { char c; int i; }; // off 0,4 | size 8 | align 4

// G1: pass small all-integer struct by value → registers (%edi,%esi on SysV).
struct Small { int a; int b; };
long add_small(struct Small s) { return (long)s.a + s.b; }

// G2: large structs → pass by const pointer (avoids MEMORY-class stack copy).
struct Big { long a, b, c, d, e; };
long add_big(const struct Big *b) { return b->a + b->e; }

// G3: packed struct: access must be alignment-safe (memcpy for unaligned).
int read_packed_safe(const struct Packed *p) {
    int v;
    __builtin_memcpy(&v, &p->val, sizeof v); // avoids unaligned load
    return v;
}

struct Packed { char tag; int val; } __attribute__((packed));

// G4: layout reporting — always verify, never hand-sum.
void report(void) {
    printf("S1 off_c=%zu off_i=%zu size=%zu align=%zu\n",
           offsetof(struct S1, c), offsetof(struct S1, i),
           sizeof(struct S1), _Alignof(struct S1));
}
