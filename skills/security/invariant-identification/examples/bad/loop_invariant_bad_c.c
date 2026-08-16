/* BAD: a too-weak, non-inductive invariant certified by a masked check.
 * Claimed invariant "i < n" is destroyed by i += 2 overshoot, and it does
 * not imply the postcondition (sum == cap). The invariant assert is behind
 * a flag that the verification run never sets, so the harness reports PASS
 * while proving nothing. In a real CBMC run the loop would unwind and fail;
 * this fixture shows why "annotated == verified" is false.
 * Compile+run: gcc -Wall -Wextra -Werror -O2 loop_invariant_bad_c.c
 *   -o /tmp/linvb.exe && /tmp/linvb.exe
 * Marker: intentionally incorrect
 */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

static unsigned long check_loop_bad(const unsigned *data, unsigned n, unsigned long cap)
{
    unsigned long sum = 0;
    unsigned i = 0;
    (void)cap;                   /* the weak invariant never uses the cap */
    while (i < n) {
        /* intentionally incorrect: the only invariant check is gated behind
           a flag no invocation sets; the "verification" is unfalsifiable. */
        if (getenv("RUN_INVARIANT_CHECK")) {
            assert(i < n);           /* also wrong: i += 2 overshoots n */
        }
        sum += data[i];
        i += 2;                      /* can jump past n */
    }
    /* intentionally incorrect: "verified" printed even though i may be n+1
       and the claimed invariant i < n never held. */
    printf("PASS: invariant i < n verified, sum=%lu\n", sum);
    printf("BAD: invariant false at back-edge (i overshoots n), check was gated; "
           "postcondition (sum <= cap) not implied\n");
    return sum;
}

int main(void)
{
    const unsigned data[] = {1, 2, 3};
    (void)check_loop_bad(data, 3, 1000UL);
    return 0;
}
