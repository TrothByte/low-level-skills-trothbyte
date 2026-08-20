/* GOOD: correctly-instrumented KLEE target — an overflow bug the tool
 * actually finds. An 8-bit checksum wraps when the byte sum exceeds 255;
 * the overflow-guard assert fails for those inputs, KLEE emits an .err
 * file plus a concrete test input, and re-running the program with that
 * input reproduces the wrap. TARGET-ONLY (KLEE runs on LLVM bitcode):
 *
 *   clang -I<klee>/include -emit-llvm -c klee_target.c -o klee_target.bc
 *   klee --max-time=60 --max-memory=1000 klee_target.bc
 *   klee-stats klee-out-*          # explored paths / generated tests
 *
 * The assert is the important part: without it KLEE only finds runtime
 * errors (div-by-zero, OOB, overshift); with it the agent states the
 * intended property and KLEE reports the violation as an .err file.
 */
#include <klee/klee.h>

#define N 2

/* 8-bit checksum: acc wraps modulo 256 once the running sum exceeds 255.
 * The guard assert below detects exactly that wrap. */
unsigned char checksum8(const unsigned char *buf, unsigned len)
{
    unsigned char acc = 0;
    for (unsigned i = 0; i < len; ++i) {
        unsigned char before = acc;
        acc = (unsigned char)(acc + buf[i]);
        /* Overflow guard: the accumulator must not wrap. "before + buf[i]"
         * promotes to int, so this comparison is against the exact sum;
         * it is FALSE for any input with buf[0]+buf[1] > 255
         * (e.g. {0xFF, 0xFF}: acc = 254, exact = 510). */
        klee_assert(acc == before + buf[i]);
    }
    return acc;
}

int main(void)
{
    unsigned char buf[N];
    klee_make_symbolic(buf, sizeof buf, "buf");

    klee_assert(checksum8(buf, N) <= 255); /* trivially true: sanity only */

    return 0;
}
