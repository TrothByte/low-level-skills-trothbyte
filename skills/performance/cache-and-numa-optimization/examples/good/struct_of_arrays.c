/* GOOD: struct-of-arrays layout.
 *
 * The field the loop touches (x) is stored in its own contiguous array, so
 * every fetched cache line is 100% useful data. Same 8M elements and the
 * same sum of x as examples/bad/array_of_structs.c; only the layout differs.
 *
 * Build and run:
 *   gcc -O2 struct_of_arrays.c -o struct_of_arrays.exe && ./struct_of_arrays.exe
 */

#include <stdio.h>
#include <windows.h>

#define N 8000000

static double xs[N];
static double ys[N];
static int ids[N];

int main(void) {
    for (int i = 0; i < N; ++i) {
        xs[i] = (double)i;
        ys[i] = 0.5 * (double)i;
        ids[i] = i;
    }

    double sum = 0.0;
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    for (int i = 0; i < N; ++i) {
        sum += xs[i];
    }
    QueryPerformanceCounter(&end);

    printf("SoA sum %.1f\n", (double)sum);
    printf("elapsed %.3f ms (%d elements)\n",
           1000.0 * (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart,
           N);
    return 0;
}
