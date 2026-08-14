/* BAD: a microbenchmark whose work the compiler throws away.
 *
 * The timing loop calls compute() but never uses its result.  compute() is
 * static and pure (no side effects), so at -O2 GCC inlines it and then
 * dead-code-eliminates the entire loop: the program executes almost nothing.
 * The reported elapsed time is near zero and says nothing about the loop's
 * real cost.
 *
 * Additional defects on purpose:
 *   - single run, no warmup: the first run also pays code fault-in and cold
 *     cache misses, and turbo boost has not ramped up;
 *   - the -O2 time is reported as if it were the production cost, without
 *     checking what the compiler actually kept.
 *
 * Build and run:
 *   gcc -O2 -Wall -Wextra -Werror elided_benchmark.c -o elided.exe && ./elided.exe
 * Compare with examples/good/harness.c, which performs the identical
 * computation but forces the result to be observable.
 */

#include <stdio.h>
#include <windows.h>

static double compute(int n) {
    double s = 0.0;
    for (int i = 0; i < n; ++i) {
        s += i * 0.5;
    }
    return s;
}

static double now_ms(void) {
    LARGE_INTEGER freq, t;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return 1000.0 * (double)t.QuadPart / (double)freq.QuadPart;
}

int main(void) {
    const int n = 100000000;
    double t0 = now_ms();
    (void)compute(n);   /* result discarded: pure call is elided at -O2 */
    double t1 = now_ms();
    printf("elapsed %.3f ms (single run, result unused)\n", t1 - t0);
    return 0;
}
