/* GOOD: row-major traversal of a row-major matrix.
 *
 * The inner loop advances the CONTIGUOUS (last) index, so consecutive
 * iterations step 8 bytes: every 64-byte cache line fetched is fully used
 * and the hardware prefetchers stream ahead. Identical matrix and sum as
 * examples/bad/strided_access.c; only the loop nesting differs.
 *
 * Build and run:
 *   gcc -O2 row_major.c -o row_major.exe && ./row_major.exe
 */

#include <stdio.h>
#include <windows.h>

#define ROWS 4096
#define COLS 4096

static double m[ROWS][COLS];

int main(void) {
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            m[r][c] = (double)(r + c);
        }
    }

    double sum = 0.0;
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            sum += m[i][j];
        }
    }
    QueryPerformanceCounter(&end);

    printf("row-major sum %.1f\n", (double)sum);
    printf("elapsed %.3f ms (%dx%d doubles)\n",
           1000.0 * (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart,
           ROWS, COLS);
    return 0;
}
