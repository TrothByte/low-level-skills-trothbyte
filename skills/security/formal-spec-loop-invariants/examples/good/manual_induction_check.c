/*
 * GOOD: the three-check procedure executable on any host (the proof backend is
 * optional). For each invariant candidate we mechanically test:
 *   (a) entry: invariant holds with i=0
 *   (b) step:  one iteration preserves it (here, prefix sum extends)
 *   (c) exit:  invariant + i==n  ==>  postcondition
 * This is the core the agent can run even without Frama-C/CBMC/Kani.
 */
#include <assert.h>

/* models sum of a[0..len-1] (len==0 -> 0) */
static int prefix_sum(const int *a, int len) {
    int s = 0;
    for (int k = 0; k < len; k++)
        s += a[k];
    return s;
}

/* checks (b) for one iteration: prefix_sum(a, i+1) == prefix_sum(a, i) + a[i] */
static void check_step(int i, const int *a) {
    assert(prefix_sum(a, i + 1) == prefix_sum(a, i) + a[i]);
}

int main(void) {
    int a[4] = {3, 1, 4, 1};

    /* (a) entry: i=0, sum==0, prefix_sum(a,0)==0 */
    assert(prefix_sum(a, 0) == 0);

    /* (b) step for every i in range */
    for (int i = 0; i < 4; i++)
        check_step(i, a);

    /* (c) exit: i==4 implies sum == prefix_sum(a,4) */
    assert(prefix_sum(a, 4) == 9);

    return 0;
}
