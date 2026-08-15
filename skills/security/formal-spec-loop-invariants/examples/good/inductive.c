/*
 * GOOD: inductive invariant that implies the postcondition.
 * The invariant "sum == sum of a[0..i-1]" (a) holds at entry (empty prefix),
 * (b) is preserved: sum += a[i] extends the prefix to a[0..i], (c) at exit
 * i == n gives the postcondition. Frama-C -wp / CBMC / Kani accept it, and the
 * host self-check asserts the actual postcondition.
 */
#include <assert.h>

/*@ requires n >= 0;
  @ requires \valid_read(a + (0..n-1));
  @ ensures \result == \sum(0, n-1, \lambda integer k; a[k]);
  @*/
int inductive_sum(const int *a, int n) {
    int sum = 0;
    int i = 0;
    /*@ loop invariant 0 <= i <= n;
      @ loop invariant sum == \sum(0, i-1, \lambda integer k; a[k]);
      @ loop assigns i, sum;
      @ loop variant n - i; */
    while (i < n) {
        sum += a[i];
        i++;
    }
    return sum;
}

int main(void) {
    int a[3] = {1, 2, 3};
    assert(inductive_sum(a, 3) == 6);
    assert(inductive_sum(a, 0) == 0);
    return 0;
}
