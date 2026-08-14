/* BAD: the asm destroys eax but does not list it in the clobber list, so the
   compiler keeps a live value in eax across the asm and returns the corrupted
   value. Compiles clean; the bug is visible only in the generated asm.
   Verify: gcc -Wall -Wextra -Werror -O2 -S missing_register_clobber.c
   The good twin (with "eax", "cc" listed) is in examples/good/clobbers.c. */

unsigned bug(unsigned x) {
    unsigned y = x;
    __asm__ volatile("xorl %%eax, %%eax" : : : );
    return y;
}
