/*
 * BAD: // intentionally incorrect — vacuous specification.
 * The `ensures \true` and the unsigned-counter invariant are trivially true;
 * Frama-C/CBMC/Kani accept them and the proof "passes" while establishing
 * nothing about the sum's correctness. LiveFMBench-class: LLM-written specs
 * lose ~20% accuracy once such vacuous clauses are filtered out.
 *
 * The host self-check (main below) demonstrates that the property "sum equals
 * the true sum" is NOT established by this spec.
 */
/*@ ensures \true; */
int vacuous_sum(const int *a, int n) {
    int sum = 0;
    int i;
    /*@ loop invariant 0 <= i; */ /* true for int, but vacuous */
    for (i = 0; i < n; i++)
        sum += a[i];
    return sum;
}

#include <assert.h>

int main(void) {
    int a[3] = {1, 2, 3};
    assert(vacuous_sum(a, 3) == 6); /* passes by luck, not by proof */
    return 0;
}
