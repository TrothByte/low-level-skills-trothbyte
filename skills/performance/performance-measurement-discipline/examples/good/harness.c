/* GOOD: a benchmark harness that measures the real cost of the loop.
 *
 * The computed value is forced to be observable by storing it into a
 * volatile sink: the optimizer must run the whole loop to produce it.  The
 * harness warms up the code and caches once, then times N runs and reports
 * the best time, the standard defense against scheduler and turbo jitter on
 * a shared machine.
 *
 * Build and run:
 *   gcc -O2 -Wall -Wextra -Werror harness.c -o harness.exe && ./harness.exe
 * Compare with examples/bad/elided_benchmark.c: identical loop, identical n,
 * but the bad version reports a near-zero time because its work was
 * eliminated.
 */

#include <stdio.h>
#include <windows.h>

#define RUNS 7

static volatile double g_sink;   /* observable side effect */

static double compute(int n) {
    double s = 0.0;
    for (int i = 0; i < n; ++i) {
        s += i * 0.5;
    }
    g_sink = s;                  /* the loop cannot be eliminated */
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
    double best = 0.0;
    int i;

    (void)compute(n);            /* warmup: fault in code, heat caches */

    for (i = 0; i < RUNS; ++i) {
        double t0 = now_ms();
        (void)compute(n);
        double t1 = now_ms();
        double dt = t1 - t0;
        if (i == 0 || dt < best) {
            best = dt;
        }
    }
    printf("best %.3f ms over %d runs (n=%d), sink=%.1f\n",
           best, RUNS, n, g_sink);
    return 0;
}
