---
name: hpc-openmp-parallel-programming
description: Use when writing, reviewing, or debugging OpenMP programs: parallel regions, worksharing loops and schedules, reductions, firstprivate/private/lastprivate, atomic and critical sections, data races on shared variables, and target offload. Prevents races, wrong reduction semantics, and schedule-dependent bugs that only appear with multiple threads.
---

# HPC OpenMP Parallel Programming

## When to use

- Writing or reviewing `#pragma omp` parallel code: `parallel for`, `sections`,
  `task`, `reduction`, `atomic`, `critical`, `single`, `master`.
- Choosing a schedule (`static`, `dynamic`, `guided`, `auto`, `runtime`) and
  reasoning about chunk sizes and load balance.
- Debugging a race on a shared variable that only appears with multiple threads.
- Using OpenMP reductions correctly (including custom reductions and
  `omp reduction` on user types) and `firstprivate`/`lastprivate`.
- Writing target offload (`target`/`teams`/`distribute`/`parallel for` + `map`) for
  GPU/accelerator execution, and reasoning about the OpenMP memory model.

## When not to use

- Distributed memory (multi-node) — use `hpc-mpi-programming`.
- Low-level threads/atomics in C/C++ — use `atomics-c11-cpp11-rust`.
- GPU vendor SDKs — use `cuda-cpp-guide`/`gpu-*` skills; OpenMP target is the
  portable alternative.
- Serial performance tuning — no parallel semantics to reason about.

## What the agent often gets wrong

- "`#pragma omp parallel for` makes every variable private." No — shared by
  default except loop control variables and (per spec) loop-invariant automatic
  vars. A `sum` accumulator without `reduction` is a race.
- "`reduction(+:x)` is like `x +=` everywhere." It creates a private copy per
  thread with a final combine; the loop variable is private, the reduction var is
  combined at the end. It is NOT safe to use the reduction variable inside the
  loop body as a shared read (it is private inside).
- "Schedules are interchangeable." Static chunks are fixed at compile time
  (cheap, load imbalance possible); dynamic/guided assign work at runtime
  (balanced, more overhead); guided chunk size shrinks geometrically. Choosing
  wrong changes performance, and assuming order of iteration is defined changes
  correctness (it is not).
- "`atomic` is a general-purpose lock." `#pragma omp atomic` is a specific
  read-modify-write or binary op on a single lvalue, faster than `critical`; using
  it for a multi-statement section is wrong. Using `critical` everywhere is
  correct but slow.
- "`firstprivate` and `private` are the same." `private` leaves the var
  uninitialized in the region; `firstprivate` copies the pre-region value into
  each thread. A `private` var that is read before write is UB.
- "Races are always reproducible." A race may never trigger on one machine and
  always on another — "it ran fine" is not a correctness argument.
- "Target offload copies nothing unless I ask." `map(tofrom:...)` is the default
  for variables in a `target` region; missing `map`/`use_device_ptr` and using a
  host pointer inside the device region is a bug.

## How to reason correctly

1. Classify every variable touched in a parallel region: shared, private,
   firstprivate, lastprivate, reduction. If any variable is both read and written
   by multiple threads without synchronization, it is a race — name the fix
   (private, reduction, atomic, critical).
2. For reductions, identify the operator and type; `reduction(+:x)` implies a
   private copy + combine. For custom types use `declare reduction`. Never
   accumulate into a shared variable from inside the loop.
3. For schedules, state the correctness property first (order-independent
   accumulation), then choose by cost model: static for uniform work, dynamic for
   imbalanced, guided for decreasing chunk sizes. Never rely on iteration order.
4. For `atomic`, check the operation matches the allowed forms (`x op= expr`,
   `x++`/`x--`, `++x`/`--x`, or the newer `atomic`-with-`seq_cst` forms); use
   `critical` for compound statements.
5. For target offload, trace the data: every pointer dereferenced on the device
   must be `map`ped or a device pointer; `map(tofrom:)` is the default; `firstprivate`
   for read-only scalars avoids unnecessary copies.
6. Verify with real thread counts (1, 2, 8) and repeated runs; a race is timing-
   dependent and a single pass proves nothing.

## What to verify

- No shared variable written by multiple threads without atomic/critical/reduction.
- Every loop variable is private; every `private` var is initialized before read.
- Reduction operator/type/initializer correct; custom reductions declared.
- Schedule chosen consciously; no correctness reliance on iteration order.
- `atomic` used only for single-lvalue ops; `critical` for multi-statement.
- `num_threads`/nesting deliberate; `omp_set_num_threads` matches intent.
- Target: all device-dereferenced data mapped; `map` types (to/from/tofrom) correct.

## How to verify

```
# Toolchain on this machine: gcc 16.1.0 with libgomp present (verified:
#   gcc -fopenmp compile + run OK on 2026-08-15).
gcc -Wall -Wextra -Werror -O2 -fopenmp examples/good/good_reduction.c -o good_red
./good_red            # expect: correct sum, exit 0
OMP_NUM_THREADS=1 ./good_red && OMP_NUM_THREADS=8 ./good_red
gcc -Wall -Wextra -Werror -O2 -fopenmp examples/bad/bad_race.c -o bad_race
./bad_race            # expect: wrong total (race) — nondeterministic
```

Toolchain status: `gcc -fopenmp` (libgomp) IS available on this machine, so the
good examples were actually compiled and run; bad examples were compiled (they
compile cleanly but produce wrong results at runtime — the race). Target offload
(`-fopenmp-targets`/GPU) is NOT available; the offload examples are documentary.
Recorded outputs: `evals/README.md`.

## Where the knowledge comes from

- `openmp-spec` — OpenMP 5.x: worksharing, schedules, reductions, atomic/critical,
  data environment, memory model, target offload.

## Related skills

- `hpc-mpi-programming` — the distributed-memory complement; hybrid MPI+OpenMP
  needs both.
- `atomics-c11-cpp11-rust` — the lower-level primitives OpenMP atomic maps to.
- `performance-measurement-discipline` — measuring schedule effects.

## Evaluation

Synthetic: race on shared accumulator (`bad/bad_race.c`), `private` read-before-
write (`bad/bad_private_uninit.c`), `atomic` on a compound statement
(`bad/bad_atomic_section.c`), missing `map` in target offload (`bad/bad_target_map.c`)
— each must be flagged.
False-positive: correct `reduction`, correct `firstprivate`, correct `atomic`
single-lvalue ops, correct `critical` sections, and correct `map` clauses must NOT
be flagged.
Adversarial: a race that never triggers at NTHREADS=1 but appears at 2/4/8; and a
schedule-dependent "correctness" (relying on iteration order) that passes on one
schedule and fails on another.
Commands and recorded results: `evals/README.md`.
