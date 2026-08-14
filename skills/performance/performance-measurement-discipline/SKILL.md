---
name: performance-measurement-discipline
description: Use when asked to optimize or benchmark C code, or when a performance claim needs evidence — profiling before changes, benchmark harness correctness, warmup and repetitions, dead-code elimination of benchmarks, statistical noise, regression baselines, and microbenchmark pitfalls like inlining and aliasing.
---

# Performance Measurement Discipline

## When to use

- Any request that starts with "make this faster" — before touching code, you
  need a measurement of where the time actually goes.
- Writing a benchmark or microbenchmark that must produce trustworthy numbers.
- Choosing a timing tool and interpreting its output (`perf stat`/`perf record`,
  `hyperfine`, `clock_gettime`, `QueryPerformanceCounter`).
- Deciding whether an optimization worked: before/after on a fixed baseline.
- Explaining why code "should be fast" but is not (profile-first reasoning).

## When not to use

- Correctness-only C reviews with no performance claim (use
  `c-undefined-behavior`, `safe-low-level-from-scratch`).
- Specific cache/NUMA optimization after the hotspot is already identified
  (use `cache-and-numa-optimization`).
- Vectorization reasoning — vectorizer reports, `restrict`, `-fopt-info`
  (use `simd-vectorization-cross-layer`, `vectorization-reasoning`).
- When the measurement tools are unavailable and the claim must stay
  unverified: mark the claim UNVERIFIED, do not fabricate numbers.

## What the agent often gets wrong

- Optimizing without measuring: "this loop is the bottleneck" with no profile.
- Benchmarking an `-O0` build and extrapolating to production.
- Timing a computation whose result is unused — the optimizer deletes the
  work and the benchmark reports near-zero time (dead-code elimination).
- Single run, no warmup: cold caches, first-touch faults, and turbo ramp
  distort the number.
- Reporting the average of outlier-polluted runs, or one run, as "the time".
- Measuring the wrong layer: a microbenchmark win that does not move the
  end-to-end runtime (optimized the wrong thing).
- Comparing numbers from different hosts, times, or inputs as if they were
  a controlled before/after.
- Trusting a microbenchmark that inlining, aliasing, or vectorization
  reshaped into something production code would never be.

## How to reason correctly

1. Profile first (sampling profiler, `perf record` on Linux) and find the
   true hotspot; never skip this step.
2. Form a hypothesis: predict the bottleneck, the change, and the rough
   magnitude of the win before running anything.
3. Build a correct harness: the timed work must have an observable side
   effect (volatile sink or printed result) and be compiled at the same
   optimization level as production.
4. Warm up the code, run N >= 5 times, report best or median; state the
   range to show noise.
5. Verify in assembly that the code you intended to time is actually there
   (no elision, no unexpected inlining, vectorization as expected).
6. Keep a regression baseline on the same machine/input/flags; re-measure
   the end-to-end effect of every change.

## What to verify

- The measured work survives `-O2` (volatile sink / printed result, and a
  look at the assembly).
- Warmup present, N >= 5 runs, best/median reported with min/max range.
- Same compiler flags as the production build.
- Assembly shows the intended loop / call / vectorization.
- The hypothesis matched the measurement; if it did not, re-profile instead
  of rationalizing.

## How to verify

```
# Windows (GCC 16.1 MinGW, QPC for timing)
gcc -O2 -Wall -Wextra -Werror examples/bad/elided_benchmark.c -o bad.exe && ./bad.exe
gcc -O2 -Wall -Wextra -Werror examples/good/harness.c -o good.exe && ./good.exe

# Linux targets (not present on Windows verification host)
perf stat -r 5 ./good.exe
hyperfine './good.exe'
```

The bad benchmark reports near-zero time (work elided); the good benchmark
with the identical loop reports a realistic time. See `evals/README.md` for
measured numbers.

## Where the knowledge comes from

- `perf-wiki` — `perf stat`/`perf record` methodology, hardware counters,
  repeat-measurement guidance
- `agner-fog` — microbenchmarking methodology, timing pitfalls, making
  benchmarks reliable
- `intel-opt-manual` — measurement-driven optimization, compiler flags and
  benchmarking guidance
- `gcc-manual` — optimize options, `-fopt-info`, dead-code elimination
  semantics, `noinline` attribute

## Related skills

- `cache-and-numa-optimization` — what to change once the hotspot is
  measured (recommend)
- `simd-vectorization-cross-layer` / `vectorization-reasoning` — verifying
  vectorization artifacts before trusting a microbenchmark (recommend)
- `meta-verification` — evidence discipline: claims need executable proof
  (recommend)

## Evaluation

Synthetic: harness correctness (volatile sink vs discarded result), warmup
+ best-of-N protocol, tool selection (QPC vs `clock()`).
Adversarial: "just add `-O3`", "I already know the bottleneck is X" with no
measurement, single-run "time is 12 ms" claims must be rejected.
False-positive: already-measured, already-optimized code must NOT be flagged
without a profile; a legitimate harness with volatile sink and best-of-N is
NOT a defect.
