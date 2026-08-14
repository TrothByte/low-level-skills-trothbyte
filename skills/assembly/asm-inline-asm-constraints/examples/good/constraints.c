/* GOOD: extended asm with correct constraints, clobbers, and operand modifiers.
   Teaching comments only. Verify: gcc -Wall -Wextra -Werror -O2 constraints.c */

#include <stdio.h>

/* Matching constraint "0": output %0 and input %1 share one register, so no
   extra move is needed and the input is consumed before the output write. */
static inline int add_asm(int a, int b) {
    int r;
    __asm__ volatile("addl %1, %0" : "=r"(r) : "r"(b), "0"(a));
    return r;
}

/* Register constraints "a" (rax) and "d" (rdx): mull writes the 64-bit product
   into edx:eax, so both halves must be captured with "=a"/"=d". */
static inline unsigned long long mul_asm(unsigned a, unsigned b) {
    unsigned lo, hi;
    __asm__ volatile("mull %3" : "=a"(lo), "=d"(hi) : "a"(a), "r"(b) : "cc");
    return ((unsigned long long)hi << 32) | lo;
}

/* Memory operand "m": the asm reads the object in place, no load hoisting. */
static inline int read_mem(const int *p) {
    int r;
    __asm__ volatile("movl %1, %0" : "=r"(r) : "m"(*p) : "memory");
    return r;
}

/* Shift count constraint "c" forces ecx; the %b modifier selects its 8-bit
   name (cl), which is the only encodable shift count register. */
static inline unsigned shift_left(unsigned x, unsigned n) {
    __asm__ volatile("shll %b1, %0" : "+r"(x) : "c"(n));
    return x;
}

/* asm goto: branch out of the asm to a C label without an output. */
static inline int div_by_5(int x) {
    int r;
    __asm__ goto("cmpl $0, %1\n\tje %l2"
                 : "=r"(r)
                 : "r"(x)
                 : "cc"
                 : zero);
    r = x / 5;
    return r;
zero:
    return -1;
}

int main(void) {
    printf("%d\n", add_asm(20, 22));
    printf("%llu\n", mul_asm(1000000u, 1000000u));
    int v = 42;
    printf("%d\n", read_mem(&v));
    printf("%u\n", shift_left(1u, 31));
    printf("%d %d\n", div_by_5(10), div_by_5(0));
    return 0;
}
