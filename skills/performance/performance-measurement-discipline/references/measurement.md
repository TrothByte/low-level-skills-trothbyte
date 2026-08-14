# Performance Measurement Discipline — Knowledge

Source-backed rules for `performance-measurement-discipline`. Each rule
follows RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE. Facts marked **VERIFIED** were
measured on GCC 16.1 (MSYS2 MinGW, x86-64, Windows) with
`examples/bad/elided_benchmark.c` and `examples/good/harness.c`. `perf` and
`hyperfine` do not exist on Windows and are documented as **Linux targets**
(`perf-wiki`), not executed here.

## 1. Measure before optimizing

- **RULE**: never guess the bottleneck. Profile the actual workload first
  (sampling profiler, `perf record` on Linux), find the function with the
  highest inclusive cost, and only then optimize.
- **WHY AI GETS IT WRONG**: pattern-matching textbook "hot loops" and
  producing an optimization without a measurement; when the guess is wrong,
  the change is churn plus regression risk.
- **CORRECT REASONING**: performance is an empirical property of one program
  on one machine with one input mix. The only reliable claim is one backed
  by a profile or a benchmark. Optimize the measured hotspot, then re-measure
  to prove the change helped.
- **EXAMPLE** (bad): rewriting a linked list as an array because "pointer
  chasing is slow", while the profile shows 80% of time in malloc/free in
  the loader, not in traversal.
- **COUNTEREXAMPLE** (good): `perf record ./app`, read the flat profile,
  see `memcpy` on top, optimize the buffer copy.
- **VERIFICATION**: the profile identifies a different function than
  intuition predicted; the optimization targets the profiled function;
  before/after measurement shows the win.
- **SOURCE**: `perf-wiki` (`perf record`/`perf stat` methodology);
  `agner-fog` (how to test for bottlenecks); `intel-opt-manual`
  (measurement-driven optimization).

## 2. Make the benchmarked work observable (no dead-code elimination)

- **RULE**: the timed computation must have an observable side effect or the
  optimizer may remove it entirely. Store the result in a `volatile` sink,
  print it, or consume it opaquely; confirm in the assembly that the work is
  still there.
- **WHY AI GETS IT WRONG**: writes `compute(n);`, reasons about the loop as
  if the compiler ran it, and ignores that a pure call whose result is
  unused is dead code at `-O2`.
- **CORRECT REASONING**: the as-if rule lets the compiler delete any
  computation whose result is not observable. A benchmark that discards the
  result measures nothing: elapsed time goes to zero regardless of n.
- **EXAMPLE** (bad): timing a static pure loop and discarding the return
  value — `elapsed 0.002 ms` for any n (`examples/bad/elided_benchmark.c`).
- **COUNTEREXAMPLE** (good): `volatile double g_sink; g_sink = s;` inside
  the timed function — the whole loop must run (`examples/good/harness.c`).
- **VERIFICATION**: VERIFIED (GCC 16.1, MinGW, Windows) — the identical
  100M-iteration loop reports ~0 ms when the result is unused and a realistic
  time (~75 ms, best of 7) with a volatile sink; `gcc -O2 -S` shows the bad
  version has no `compute` function at all — only `main` remains.
- **SOURCE**: `gcc-manual` (optimize options, as-if / dead-code-elimination
  semantics); `intel-opt-manual` (benchmarking guidance); `agner-fog`
  (making benchmarks reliable).

## 3. Warm up before timing

- **RULE**: run the timed code once (or more) before measuring. The first
  invocation pays code fault-in, I-cache and TLB misses, and runs before
  turbo boost ramps; time only the warm steady state.
- **WHY AI GETS IT WRONG**: single-shot timing is the default mental model;
  the agent forgets OS and hardware state is not stationary.
- **CORRECT REASONING**: cold-start costs are paid once in a long-running
  production program but repeatedly in a short benchmark; a single cold run
  distorts the number in either direction.
- **EXAMPLE** (bad): one timed call on cold code — the number includes
  first-touch page faults and cold cache misses steady-state execution never
  sees.
- **COUNTEREXAMPLE** (good): an untimed warmup call first, then N timed
  runs; only timed runs are reported.
- **VERIFICATION**: consecutive invocations of the good harness produce
  consistent numbers, and the first cold run is visibly slower than the
  timed ones.
- **SOURCE**: `agner-fog` (microbenchmarking methodology); `perf-wiki`
  (measurement methodology).

## 4. Repeat runs and handle statistical noise

- **RULE**: run N >= 5 times and report best or median, not a single run and
  not the arithmetic mean of outlier-polluted runs. Scheduler migration,
  turbo, interrupts, and thermal state make single runs unrepeatable.
- **WHY AI GETS IT WRONG**: reports one observed number as "the time",
  treats noise as signal, and never states the distribution.
- **CORRECT REASONING**: wall-clock times on shared machines are random
  variables. Best-of-N approximates the machine's capability under favorable
  conditions; the median resists outliers. Report the min/max range to make
  the noise visible.
- **EXAMPLE** (bad): "the time is 12 ms" from one run that a second run
  would have measured at 9 ms or 30 ms (background process).
- **COUNTEREXAMPLE** (good): "best of 7: 96.2 ms, runs ranged 96.1-104.8 ms".
- **VERIFICATION**: two consecutive invocations of the same binary produce
  overlapping ranges; a single-run report cannot be reproduced.
- **SOURCE**: `perf-wiki` (repeat measurements); `agner-fog` (measuring
  time, pitfalls).

## 5. Use the right timing tool

- **RULE**: use a monotonic high-resolution clock for wall time
  (`clock_gettime(CLOCK_MONOTONIC)` on POSIX, `QueryPerformanceCounter` on
  Windows), `perf stat -r N` for cycle/instruction/cache counters on Linux,
  `perf record` for profiling, and `hyperfine` for command-line benchmark
  comparisons. Wall clock is the end-to-end ground truth; counters explain
  WHY the number is what it is.
- **WHY AI GETS IT WRONG**: quotes `time`/`perf stat` output without knowing
  the clock behind it (`CLOCK_REALTIME` moves with NTP), or proposes
  `clock()`/`GetTickCount` (ms resolution) for a sub-ms operation.
- **CORRECT REASONING**: the clock resolution must be well below the
  measured duration and the clock must be monotonic. `clock()` measures
  process CPU time, not wall time — wrong for I/O-bound or multithreaded
  code. QPC (`QueryPerformanceCounter`) has ~100 ns-1 µs resolution on
  modern Windows and is the QPC backing MinGW's high-resolution timing.
- **EXAMPLE** (bad): timing a 500 µs function with `clock()` (ms
  resolution) — the result is 0 or 1 ms.
- **COUNTEREXAMPLE** (good): QPC / `CLOCK_MONOTONIC` around the region of
  interest; on Linux `perf stat` adds counters that explain the wall number.
- **VERIFICATION**: VERIFIED on Windows via QPC (~0.1 µs counter on the
  verification host). `perf` and `hyperfine` are Linux targets — absent on
  the Windows host, documented not executed.
- **SOURCE**: `perf-wiki` (`perf stat`/`perf record`); `gcc-manual`;
  `intel-opt-manual`.

## 6. Compiler optimization caveats: no -O0, and verify -O2 in assembly

- **RULE**: benchmark at the production optimization level (usually `-O2`).
  At `-O0` everything is slow for the wrong reason. At `-O2`, confirm in the
  assembly that the code you think you are timing is really there: inlining,
  constant folding, strength reduction, and vectorization change what runs.
- **WHY AI GETS IT WRONG**: times `-O0` builds and extrapolates, or times
  `-O2` and assumes the source loop equals the executed code.
- **CORRECT REASONING**: the optimizer rewrites the program. The benchmark
  must time the optimized code and confirm the intended pattern (loop still
  present, vectorized as expected) via `gcc -O2 -S`, `-fopt-info-vec`, or
  objdump.
- **EXAMPLE** (bad): a "slow" function measured at `-O0` gets hand-optimized,
  while at `-O2` the compiler already did the same transformation — the
  reported win is an artifact.
- **COUNTEREXAMPLE** (good): compile at `-O2`, disassemble, and show the
  optimization survived before claiming it.
- **VERIFICATION**: `gcc -O2 -S` inspection; `-fopt-info-vec` for
  vectorization claims.
- **SOURCE**: `gcc-manual` (optimize options, `-fopt-info`); `intel-opt-manual`
  (compiler flags and measurement).

## 7. Avoid "optimized the wrong thing"

- **RULE**: an optimization is valid only if the before/after measurement on
  the real workload improves the end-to-end number. Microbenchmark wins that
  do not move end-to-end runtime are worthless.
- **WHY AI GETS IT WRONG**: rewarded for producing an optimization, not for
  proving it matters; a microbenchmark gain is offered as evidence without an
  end-to-end check.
- **CORRECT REASONING**: the microbenchmark hot path may not be hot in
  production (input mix, call frequency, I/O or lock bound). Profile the real
  workload, then verify the change moved the real number.
- **EXAMPLE** (bad): a 10x microbenchmark win on a `memcpy` inside a
  function the profile shows is called twice per program start — total
  runtime unchanged.
- **COUNTEREXAMPLE** (good): profile, optimize the true hotspot, and show
  the whole program measurably faster.
- **VERIFICATION**: end-to-end runtime changes by the expected amount;
  microbenchmark and profile agree on the hotspot.
- **SOURCE**: `perf-wiki`; `intel-opt-manual`; `agner-fog`.

## 8. Regression baselines

- **RULE**: keep a reproducible baseline: same machine, same input, same
  flags, same measurement protocol. Judge optimizations against it and check
  future changes against it to catch regressions.
- **WHY AI GETS IT WRONG**: compares numbers taken at different times, on
  different hosts, or with different inputs and draws conclusions from the
  noise.
- **CORRECT REASONING**: numbers are comparable only under a fixed protocol.
  Record the command, machine, and input with every measurement, and
  re-run the baseline whenever machine state may have changed (reboot,
  other load).
- **EXAMPLE** (bad): "we optimized 20%" comparing last week's run on a
  laptop with today's run on a loaded CI server.
- **COUNTEREXAMPLE** (good): a checked-in script (`perf stat -r` /
  `hyperfine`) runs baseline and candidate back-to-back on the same host and
  reports the delta.
- **VERIFICATION**: interleaved baseline/candidate runs on the same host
  produce a stable delta that exceeds run-to-run noise.
- **SOURCE**: `perf-wiki`; `agner-fog`.

## 9. Microbenchmark pitfalls: inlining, aliasing, vectorization mask the effect

- **RULE**: before trusting a microbenchmark, check that the compiler did
  not (a) inline the timed function into the caller (removing the call
  overhead the test was about), (b) use alias analysis to reorder or split
  arrays in ways real code cannot be specialized, or (c) vectorize or
  restructure the loop so the "change" is not what executes.
- **WHY AI GETS IT WRONG**: treats the microbenchmark as the production
  code; forgets the compiler specializes the benchmark's easy-to-prove
  shapes (static arrays, constant n, no aliasing) far beyond what the real
  codebase permits.
- **CORRECT REASONING**: a microbenchmark measures the compiler's handling
  of the benchmark, not of production code. Isolate the property under test
  (e.g., `__attribute__((noinline))` when call overhead is the question)
  and confirm the executed assembly matches the intent.
- **EXAMPLE** (bad): timing a small function that gets inlined into the
  timing loop — no call is executed, and a change that only affects call
  overhead reads as a 0% or 100% artifact.
- **COUNTEREXAMPLE** (good): mark the function `noinline` (GCC,
  `gcc-manual`) when call overhead is the subject, and verify via asm that
  no inlining happened.
- **VERIFICATION**: disassembly shows the expected number of calls or the
  expected inlined copy; results move consistently with the property under
  test.
- **SOURCE**: `gcc-manual` (`noinline` attribute, inline semantics);
  `agner-fog` (pitfalls in microbenchmarking); `intel-opt-manual`.
