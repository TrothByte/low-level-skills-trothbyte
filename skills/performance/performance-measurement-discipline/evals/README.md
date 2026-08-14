# Evaluation — performance-measurement-discipline

Skill: `skills/performance/performance-measurement-discipline`. Stability
target: `evaluated`. Source files: `examples/good/harness.c`,
`examples/bad/elided_benchmark.c`.

## Host and timing

Verification host: Windows, GCC 16.1.0 (MSYS2 MinGW, x86-64), AMD Ryzen 5
5600H (6 cores / 12 threads, Zen 3). Timing uses `QueryPerformanceCounter`
(10 MHz counter on this host, ~0.1 us resolution). The good harness times
N = 7 runs after one warmup run and reports the best; best-of-N mitigates
scheduler and turbo jitter.

`perf`, `hyperfine`, and `perf record` do not exist on Windows; all
counter/profile tool claims in this skill are documented as **Linux targets**
(`perf-wiki`), not executed here.

## Verified facts (OBSERVED, GCC 16.1.0, MinGW x86-64, Windows)

Both binaries run the IDENTICAL loop (`for i in 0..100000000: s += i*0.5`)
at `-O2`; they differ only in whether the result is made observable.

| Fact | Evidence |
|---|---|
| unused result -> work elided, near-zero time | `elided_benchmark.c`: 0.000 ms (every run) |
| `gcc -O2 -S` shows no `compute` function at all | only `main` remains in the .s output |
| observable result -> realistic time | `harness.c`: best-of-7 = 75.275 / 75.143 / 74.537 / 74.598 ms across 4 invocations |
| identical sum preserved (sanity check) | `sink=2499999975000000.0` in every good run |
| measurement-elision ratio | ~75 ms vs ~0 ms: the "optimization" delta is entirely an artifact |
| warmup effect visible | good harness first (warmup) iteration is the coldest and is excluded from the report |
| repeatability | good-harness best times spread 74.5-75.3 ms (run-to-run noise < 2%); single-run bad value 0.000 ms is trivially repeatable for the wrong reason |

The pair demonstrates rules 2-4 of `references/measurement.md`: the same
loop measures 0 ms or 75 ms depending on harness correctness.

## Verification commands and actual output

```
gcc -O2 -Wall -Wextra -Werror examples/bad/elided_benchmark.c -o elided.exe && ./elided.exe
gcc -O2 -Wall -Wextra -Werror examples/good/harness.c -o harness.exe && ./harness.exe
```

Observed output (Windows, GCC 16.1.0):

```
$ ./elided.exe
elapsed 0.000 ms (single run, result unused)
$ ./harness.exe
best 75.275 ms over 7 runs (n=100000000), sink=2499999975000000.0
best 75.143 ms over 7 runs (n=100000000), sink=2499999975000000.0
best 74.537 ms over 7 runs (n=100000000), sink=2499999975000000.0
best 74.598 ms over 7 runs (n=100000000), sink=2499999975000000.0
$ gcc -O2 -S examples/bad/elided_benchmark.c -o -   # no `compute` symbol
```

Linux targets (documented, not executed): `perf stat -r 5 ./harness` for
cycles/instructions, `hyperfine './harness'` for a calibrated wall-time
comparison, `perf record`/`perf report` for profiling.

## Synthetic evals

- **easy**: `double f(int n){double s=0; for(int i=0;i<n;i++)s+=i; return s;}`
  timed as `(void)f(100000000);` — expected answer: the call is pure and its
  result unused, so `-O2` eliminates it; the benchmark needs a volatile sink.
- **easy**: single timed run, no warmup — expected answer: unreliable; code
  fault-in, cold caches, and turbo ramp distort the first run.
- **medium**: two variants of the same loop, one with a volatile sink, one
  discarding the result — expected answer: the harness difference, not the
  loop, explains the 0 ms vs ~75 ms gap.
- **medium**: picking a timer for a 500 us operation — expected answer: QPC
  or `clock_gettime(CLOCK_MONOTONIC)`, not `clock()`/`GetTickCount` (ms
  resolution).
- **hard**: a microbenchmark showing a 10x win on a function the profile
  says is called twice per program start — expected answer: the end-to-end
  number did not move; the optimization target was wrong.

## Adversarial evals

- "I already know the bottleneck is the pointer chase, rewrite the list" —
  agent must demand a profile before any rewrite and reject the guess as
  evidence.
- "Just add `-O3`, it is faster" — agent must ask what was measured; `-O3`
  can shrink or grow runtime and is not a measured optimization.
- "The time is 12 ms" from a single run — agent must reject the claim and
  require repetitions with a stated distribution.
- "We improved 20%" comparing runs on different hosts/days — agent must
  require the same machine, input, and flags, or the delta is noise.
- "This microbenchmark proves the change is good" without an end-to-end
  before/after — agent must point at rule 7 (optimized the wrong thing).

## False-positive evals (must NOT flag)

- a benchmark harness with a volatile sink, warmup, and best-of-N reporting
  — correct methodology, not a defect.
- code that was measured and profiled, where the optimization follows the
  measured hotspot — must not be flagged as "guessing".
- `clock_gettime`/QPC usage for sub-ms measurements — correct tool choice.
- a microbenchmark that uses `__attribute__((noinline))` to isolate call
  overhead — a legitimate measurement isolation technique.
- an optimization rejected purely because the microbenchmark artifact was
  explained (inlining/aliasing) rather than the code being "slow" — correct
  to not optimize.

## Scoring

- detection: names the harness defect (dead-code elimination / single run /
  no warmup / wrong timer) from the code, not from intuition about "slow".
- reasoning: predicts the elided benchmark reports ~0 ms and the harness
  reports a realistic time BEFORE running, and explains why.
- fix: makes the work observable (volatile sink / printed result), warms
  up, repeats N runs, and reports best/median with a range.
- verification: demonstrates with best-of-N wall times from both binaries
  and the empty asm of the bad case, not with "it compiled".

## Sources exercised

`perf-wiki` (perf methodology, repeat measurements), `agner-fog`
(microbenchmarking pitfalls, timing), `intel-opt-manual` (measurement-driven
optimization, benchmarking guidance), `gcc-manual` (optimize options,
`-fopt-info`, `noinline`, as-if/dead-code-elimination semantics). Registry
ids per `registry/sources.yaml`; full reasoning in
`references/measurement.md`.
