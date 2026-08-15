// intentionally incorrect — BAD example: target offload dereferences a host
// pointer that is not mapped.
//
// The array `a` is a host pointer; the `target` region does not map it, so the
// device dereferences an invalid (host) address. Correct code maps the data:
// `#pragma omp target map(tofrom: a[0:n])`.
//
// Target toolchain: GPU/offload (e.g. nvptx, gcn) — NOT available on this
// machine; documentary only. Host-only gcc -fopenmp does NOT exercise target.

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const int n = 1024;
    int *a = (int *)malloc(n * sizeof(int));

    #pragma omp target
    {
        for (int i = 0; i < n; i++)
            a[i] = i;                   // BUG: a is not mapped to the device
    }

    printf("bad_target_map: a[512]=%d\n", a[512]);
    free(a);
    return 0;
}
