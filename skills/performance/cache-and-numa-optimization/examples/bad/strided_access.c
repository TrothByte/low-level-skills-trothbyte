/* BAD: column-major traversal of a row-major matrix.
 *
 * C lays out m[ROWS][COLS] with the LAST index contiguous (row-major).
 * The inner loop here advances the FIRST index, so consecutive iterations
 * step COLS*8 = 32 KB apart: every load touches a new 64-byte cache line
 * and uses 8 of its 64 bytes (12.5%). The 32 KB stride also crosses pages
 * every iteration, defeating the hardware prefetchers entirely.
 *
 * Build and run:
 *   gcc -O2 strided_access.c -o strided_access.exe && ./strided_access.exe
 * Compare with examples/good/row_major.c: identical matrix and sum, only
 * the loop nesting differs.
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
    for (int j = 0; j < COLS; ++j) {
        for (int i = 0; i < ROWS; ++i) {
            sum += m[i][j];
        }
    }
    QueryPerformanceCounter(&end);

    printf("column-major sum %.1f\n", (double)sum);
    printf("elapsed %.3f ms (%dx%d doubles)\n",
           1000.0 * (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart,
           ROWS, COLS);
    return 0;
}
