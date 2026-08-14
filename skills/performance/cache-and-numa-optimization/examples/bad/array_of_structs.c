/* BAD: array of structs with padding and untouched fields.
 *
 * struct Particle is 24 bytes: two doubles, one int, plus 4 bytes of
 * padding to satisfy 8-byte alignment (normal ABI layout, not a bug).
 * The loop sums only the x field, so it fetches 24 bytes of memory per
 * useful 8: two thirds of every fetched byte is padding or a field the
 * computation never reads.
 *
 * Build and run:
 *   gcc -O2 array_of_structs.c -o array_of_structs.exe && ./array_of_structs.exe
 * Compare with examples/good/struct_of_arrays.c: same 8M elements, same
 * sum of x, only the layout differs.
 */

#include <stdio.h>
#include <windows.h>

#define N 8000000

struct Particle {
    double x;
    double y;
    int id;
}; /* sizeof == 24 (4 bytes padding) */

static struct Particle parts[N];

int main(void) {
    for (int i = 0; i < N; ++i) {
        parts[i].x = (double)i;
        parts[i].y = 0.5 * (double)i;
        parts[i].id = i;
    }

    double sum = 0.0;
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    for (int i = 0; i < N; ++i) {
        sum += parts[i].x;
    }
    QueryPerformanceCounter(&end);

    printf("AoS sum %.1f (sizeof(struct Particle) = %d)\n",
           (double)sum, (int)sizeof(struct Particle));
    printf("elapsed %.3f ms (%d elements)\n",
           1000.0 * (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart,
           N);
    return 0;
}
