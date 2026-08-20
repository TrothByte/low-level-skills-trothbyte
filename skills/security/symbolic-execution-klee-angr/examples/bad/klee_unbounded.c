/* BAD: unbounded symbolic loop — path explosion / non-termination
 * without search bounds. This is the anti-pattern: the loop's exit
 * depends on a symbolic value with no convergence argument, so KLEE
 * keeps forking states (n > i for ever-larger i) and never finishes.
 * TARGET-ONLY; never run bare. Documented mitigation:
 *
 *   clang -I<klee>/include -emit-llvm -c klee_unbounded.c -o klee_unbounded.bc
 *   klee --max-time=60 --max-instruction-time=10 --max-memory=1000 klee_unbounded.bc
 *
 * or bound the input domain in the target:
 *   klee_assume(n <= 16);
 * Without a bound, for n == UINT32_MAX the loop index wraps to 0 and the
 * run never exits: KLEE churns solver queries forever. The tool does NOT
 * warn about this — the agent must add the bound and report it.
 */
#include <klee/klee.h>
#include <stdint.h>

int main(void)
{
    uint32_t n;
    klee_make_symbolic(&n, sizeof n, "n");
    /* UNBOUNDED RANGE: n may be any 32-bit value. */
    /* Mitigation (uncomment): klee_assume(n <= 16); */

    uint32_t i = 0;
    while (n > i)   /* no bound, no exit argument */
        ++i;

    return (int)i;
}
