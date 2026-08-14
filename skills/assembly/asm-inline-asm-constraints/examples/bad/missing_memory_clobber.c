/* BAD: the asm touches memory through a raw pointer but declares no "memory"
   clobber. The compiler believes the asm cannot read or write memory, so it
   reorders, merges, or caches the surrounding accesses. The program compiles
   clean with -Wall -Wextra -Werror and silently computes the wrong value.
   Compare: examples/good/clobbers.c (with_memory_clobber).
   Verify: gcc -Wall -Wextra -Werror -O2 missing_memory_clobber.c && run. */

#include <stdio.h>

static unsigned g = 1;

/* Runtime-observable bug: the asm writes 2 to *g, but the compiler is not told.
   It folds a + b into one load of g, so the result is 1+1 instead of 1+2. */
static unsigned no_clobber(void) {
    unsigned a = g;
    __asm__ volatile("movl $2, (%0)" : : "r"(&g));
    unsigned b = g;
    return a + b;
}

/* Store-merge bug (visible in -O2 -S): without the clobber the first store is
   dead in the compiler's view and is deleted. */
static void worker(int *p) {
    *p = 1;
    __asm__ volatile("");
    *p = 2;
}

/* Load CSE bug: the two reads collapse into one because the compiler may cache
   a non-volatile global across an asm that it believes cannot touch memory. */
static unsigned nv_load(const unsigned *p) {
    unsigned a = *p;
    __asm__("");
    unsigned b = *p;
    return a + b;
}

int main(void) {
    g = 1;
    printf("no_clobber=%u (wrong: expected 3)\n", no_clobber());
    int x = 0;
    worker(&x);
    printf("worker x=%d\n", x);
    unsigned v = 7;
    printf("nv_load=%u\n", nv_load(&v));
    return 0;
}
