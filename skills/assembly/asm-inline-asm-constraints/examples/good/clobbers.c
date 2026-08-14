/* GOOD: memory and register clobbers declared, so the compiler cannot
   reorder or cache around the asm. Teaching comments only.
   Verify: gcc -Wall -Wextra -Werror -O2 clobbers.c && clobbers.exe */

#include <stdio.h>

static unsigned g = 1;

/* "memory" clobber: the asm writes through a raw pointer; the compiler must
   assume every memory location is touched, so the two reads stay distinct. */
static unsigned with_memory_clobber(void) {
    unsigned a = g;
    __asm__ volatile("movl $2, (%0)" : : "r"(&g) : "memory");
    unsigned b = g;
    return a + b;
}

/* Register clobbers listed: "eax" and flags "cc" are destroyed by the asm.
   The compiler saves/restores the value across the asm instead of leaving it
   in eax. */
static unsigned with_register_clobber(unsigned x) {
    unsigned y = x;
    __asm__ volatile("xorl %%eax, %%eax" : : : "eax", "cc");
    return y;
}

int main(void) {
    g = 1;
    printf("%u\n", with_memory_clobber());
    printf("%u\n", with_register_clobber(12345u));
    return 0;
}
