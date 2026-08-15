/*
 * BAD: // intentionally incorrect — invariant not inductive.
 * `loop invariant 0 <= i < n` is TRUE at entry and is preserved by the body,
 * but it does not imply the postcondition `sum == sum of a[0..n-1]`; a prover
 * that checks only "invariant holds" (without postcondition linkage) accepts
 * this. Worse variant: if the body increments i AFTER accumulating, the
 * invariant is preserved but the ACCUMULATED SUM is off by one — and a spec
 * copied from the loop body inherits that off-by-one.
 */
/*@ requires n >= 0;
  @ ensures \result == n; */
int count_loop(int n) {
    int i = 0;
    /*@ loop invariant 0 <= i <= n; */ /* not strong enough for any contract */
    while (i < n)
        i++;
    return i;
}

#include <assert.h>

int main(void) {
    assert(count_loop(5) == 5);
    return 0;
}
