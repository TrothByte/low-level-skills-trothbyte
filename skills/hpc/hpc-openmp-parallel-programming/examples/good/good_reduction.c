// GOOD: correct reduction with an order-independent loop.
//
// `reduction(+:sum)` creates a per-thread private copy (identity 0), sums into
// it, and combines at the end. The loop body is order-independent, so any
// schedule is correct. Verified on this machine with gcc -fopenmp: correct
// result for OMP_NUM_THREADS=1,2,8.

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const int n = 1 << 20;
    int *a = (int *)malloc(n * sizeof(int));
    int sum = 0;

    #pragma omp parallel for
    for (int i = 0; i < n; i++)
        a[i] = 1;

    #pragma omp parallel for reduction(+:sum)   // private copy per thread
    for (int i = 0; i < n; i++)
        sum += a[i];                            // combines at region end

    printf("good_reduction: threads=%d sum=%d (expected %d)\n",
           omp_get_max_threads(), sum, n);
    free(a);
    return (sum == n) ? 0 : 1;
}
