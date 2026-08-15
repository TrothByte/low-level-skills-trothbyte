// GOOD: target offload with explicit map of everything the device dereferences.
//
// The array a[0:n] is mapped (tofrom by default is what this program needs:
// the device reads, modifies, and we read back). The reduction result is
// combined with an omp reduction on the target construct.
//
// Target toolchain: GPU/offload (nvptx/gcn) — NOT available on this machine;
// documentary only. Host-only gcc -fopenmp does not exercise target.

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const int n = 1 << 20;
    int *a = (int *)malloc(n * sizeof(int));
    int sum = 0;

    for (int i = 0; i < n; i++) a[i] = 1;

    // map(tofrom: a[0:n]): the device gets the data, modifies it, sends it back.
    #pragma omp target map(tofrom: a[0:n]) reduction(+:sum)
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        a[i] += 1;                       // device sees a copy of the array
        sum += a[i];
    }

    printf("good_target_map: sum=%d (expected %d)\n", sum, 2 * n);
    free(a);
    return (sum == 2 * n) ? 0 : 1;
}
