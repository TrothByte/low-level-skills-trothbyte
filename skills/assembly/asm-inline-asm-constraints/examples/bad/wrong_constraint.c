/* BAD: wrong constraint codes. These examples intentionally do NOT compile;
   each is the constraint error an agent hits when guessing the code.
   Verify: gcc -Wall -Wextra -Werror -O2 -c wrong_constraint.c
   Expected errors: "impossible constraint in 'asm'", "output operand
   constraint lacks '='". */

/* "i" requires an immediate constant; a runtime value cannot satisfy it. */
unsigned impossible_immediate(unsigned x) {
    unsigned r;
    __asm__ volatile("movl %1, %0" : "=r"(r) : "i"(x));
    return r;
}

/* "r" on an output operand must be written "=r"; plain "r" is input-only and
   the output write becomes a compile error. */
unsigned missing_equals(unsigned x) {
    unsigned r;
    __asm__ volatile("movl %1, %0" : "r"(r) : "r"(x));
    return r;
}

/* "c" forces ecx, but a shift/rotate by register only encodes as cl; using the
   full register name in the template is an assembler operand-type error. */
unsigned shift_wrong_name(unsigned x, unsigned n) {
    __asm__ volatile("shll %1, %0" : "+r"(x) : "c"(n));
    return x;
}
