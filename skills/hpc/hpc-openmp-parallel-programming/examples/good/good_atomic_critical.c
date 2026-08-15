// GOOD: reduction(max:) for a max; atomic for a single-lvalue RMW; critical
// for a compound update.
//
// - reduction(max:max_so_far): a per-thread copy + final combine; the
//   correct way to propagate a running max out of the region.
// - atomic: `hits++` is a valid single-lvalue RMW (allowed form).
// - critical: compound two-variable update (not atomically expressible).
// The loop is order-independent, so the default schedule is fine.

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const int n = 100000;
    int *a = (int *)malloc(n * sizeof(int));
    int max_so_far = 0, pair_count = 0, hits = 0;

    for (int i = 0; i < n; i++) a[i] = rand() % 1000;

    #pragma omp parallel for reduction(max:max_so_far)
    for (int i = 0; i < n; i++) {
        if (a[i] > max_so_far)
            max_so_far = a[i];          // per-thread private copy + combine
        if (a[i] % 2 == 0) {
            #pragma omp atomic
            hits++;                     // single-lvalue RMW: atomic is correct
            #pragma omp critical
            {                           // compound update: critical is required
                pair_count++;
                if (pair_count > 1000) pair_count = 1000;
            }
        }
    }

    printf("good_priv_atomic: max_so_far=%d pair_count=%d hits=%d\n",
           max_so_far, pair_count, hits);
    free(a);
    return 0;
}
