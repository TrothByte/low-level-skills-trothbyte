/*
 * clean_app.c — no-fault control fixture for core-dump-analysis.
 *
 * A program that does real work (fills, validates, and sums an array) and
 * exits 0. The discipline must report "no fault": no crash, no core, exit 0,
 * correct output. This is the false-positive gate: nothing in this program
 * may be flagged as a crash or as evidence of corruption.
 *
 * Build (host):  gcc -g -O0 examples/good/clean_app.c -o clean_app.exe
 */

#include <stdio.h>

#define N 1024

int main(void) {
  int data[N];
  long long sum = 0;

  for (int i = 0; i < N; ++i) {
    data[i] = (i % 7) - 3;
  }
  for (int i = 0; i < N; ++i) {
    sum += (long long)data[i];
  }

  /* Expected total: (i%7)-3 over i=0..1023 sums to -5. */
  printf("clean_app: sum=%lld, expected=-5\n", sum);

  if (sum != -5) {
    fprintf(stderr, "clean_app: FAILED (sum mismatch)\n");
    return 1;
  }
  printf("clean_app: ok, exiting 0\n");
  return 0;
}
