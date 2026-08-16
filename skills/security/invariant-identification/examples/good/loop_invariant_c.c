/* GOOD: loop invariant with base/step/post obligations asserted on live
 * paths, in the CBMC/Frama-C shape. Invariant:
 *   P == (processed == i) && (sum == partial_sum) && (sum <= cap)
 * CBMC shape: cbmc loop_invariant_c.c --function check_loop --unwind 8
 * Host: gcc -Wall -Wextra -Werror -O2 loop_invariant_c.c -o /tmp/linv.exe
 *   && /tmp/linv.exe
 */
#include <stdio.h>
#include <assert.h>

static unsigned long check_loop(const unsigned *data, unsigned n, unsigned long cap)
{
    unsigned long sum = 0;
    unsigned i = 0;
    assert(sum <= cap);              /* base: P holds on entry */
    while (i < n) {
        assert(i <= n);              /* step: progress var in range */
        assert(sum <= cap);          /* step: cap preserved */
        unsigned long v = data[i];
        if (v > cap - sum) {
            sum = cap;               /* saturating: preserves sum <= cap */
        } else {
            sum += v;
        }
        i++;
        assert(sum <= cap);          /* step: back-edge preserves P */
    }
    /* post: !(i < n) && P  =>  i == n && sum <= cap */
    assert(i == n);
    assert(sum <= cap);
    return sum;
}

int main(void)
{
    const unsigned data[] = {5, 7, 9, 1000, 2};
    unsigned long r = check_loop(data, 5, 1000000UL);
    assert(r == 1023UL);
    printf("GOOD: invariant asserted at entry, body, back-edge, and exit; sum=%lu\n", r);
    return 0;
}
