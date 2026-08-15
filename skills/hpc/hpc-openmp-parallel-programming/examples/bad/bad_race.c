// intentionally incorrect — BAD example: data race on a shared accumulator.
//
// `sum` is shared by default. Multiple threads execute `sum += a[i]`
// concurrently without atomic/critical/reduction — a data race (UB). The result
// is nondeterministic and usually WRONG (lost updates). It may even look
// correct for small N on a few threads — which is exactly the trap: "it ran
// fine" is not a correctness argument.
//
// Compares against: examples/good/good_reduction.c
// Verified on this machine with gcc -fopenmp: it COMPILES (exit 0) but the
// runtime result is wrong and varies with OMP_NUM_THREADS.

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const int n = 1 << 20;
    int *a = (int *)malloc(n * sizeof(int));
    int sum = 0;
    int i, nthreads = omp_get_max_threads();

    for (i = 0; i < n; i++) a[i] = 1;

    #pragma omp parallel for
    for (i = 0; i < n; i++)
        sum += a[i];                     // BUG: race on shared `sum`

    printf("bad_race: threads=%d sum=%d (expected %d)\n",
           nthreads, sum, n);
    free(a);
    return (sum == n) ? 0 : 1;           // returns nonzero when the race shows
}
