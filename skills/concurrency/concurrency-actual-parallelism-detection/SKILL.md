---
name: concurrency-actual-parallelism-detection
description: Use when verifying that concurrent code actually executes in parallel — distinguishing real parallelism from "fake" thread-safe code (CONCUR ST class) and catching concurrency-limit bypasses (codex#37653: 86 processes, kernel panic). Requires measuring wall-clock scaling and real overlap, not just counting threads or using primitives.
---

# Concurrency: Actual Parallelism Detection

## When to use

- Verifying that a multithreaded program achieves actual speedup, not just
  "uses pthreads".
- Reviewing a claim that "the code is concurrent" when it is single-threaded
  in practice (CONCUR ST category: thread-safe primitives under
  single-threaded execution).
- Auditing concurrency limits that rely on their own accounting (zsh `jobs`
  counts, process pools) — a limit that cannot see its own children is not a
  limit (codex#37653).
- Diagnosing why "parallel" code is as slow as serial code.

## When not to use

- Analyzing memory ordering or atomics semantics — use
  `memory-ordering-reasoning` / `atomics-c11-cpp11-rust`.
- Deadlock/lock-ordering bugs — use `concurrency-deadlock-and-lock-ordering`.
- GPUs/accelerators with a different execution model — use the GPU skills.
- Single-threaded code with no concurrency claims.

## What the agent often gets wrong

- "It uses pthread_mutex, so it's concurrent" — a mutex can serialize a whole
  workload; the fake_parallel fixture has 4 threads, `max_working=1`, and a
  wall time ~4× the real version. Primitives are not parallelism.
- "It spawns N threads, so it's parallel" — spawned threads can all be
  waiting at a lock, or do nothing (single_thread fixture: threads spawned,
  `max_started=1`, all work on the main thread).
- Measuring only thread count or CPU time, not wall-clock overlap. Counting
  started/alive threads ≠ counting threads that execute simultaneously.
- Trusting a concurrency limit's own accounting: codex#37653 — zsh `jobs`
  inside command substitution cannot see the parent's jobs, so the
  parallelism cap never triggers and 86 processes spawn → kernel panic and
  reboot. A limit that cannot measure the thing it limits is decorative.
- Claiming "scales" without a wall-clock measurement against the serial
  baseline.

## How to reason correctly

1. Define "actual parallelism" operationally: wall time for the parallel
   program vs the serial baseline, AND the observed overlap of executing
   threads (max concurrent working threads).
2. Measure overlap with a counter: threads increment a shared `working`
   counter at the start of the real workload and decrement at the end; the
   observed maximum is the actual concurrency. Started/alive counters are a
   different, weaker metric (they include threads queued at a lock).
3. Check the hardware bound: concurrency cannot exceed available cores
   (`GetActiveProcessorCount`/`sched_getaffinity`). Report both.
4. For limits/accounting: verify the limit can SEE the entities it counts.
   If `jobs` (or a process count) runs in a context where the children are
   invisible, the limit is zero and the resource is unbounded.
5. Only after both the overlap and the wall-time checks pass, claim
   parallelism.

## What to verify

- The max number of threads simultaneously executing the workload
  (max_working) matches the claim.
- Wall time is materially below the serial baseline (and no more than the
  serial baseline ÷ available cores, roughly).
- Threads spawned ≠ threads working: report max_started AND max_working.
- Concurrency limits are measured against the real entities (processes,
  jobs), and the measurement context can see them.
- Core count is reported so single-core results are not overread.

## How to verify

```
gcc -O2 -Wall -Wextra -Werror -pthread examples/good/real_parallel.c -o rp.exe
./rp.exe          # threads=4 max_started=4 max_working=4 wall~0.30s
gcc -O2 -Wall -Wextra -Werror -pthread examples/bad/fake_parallel.c -o fp.exe
./fp.exe          # threads=4 max_started=4 max_working=1 wall~1.20s
gcc -O2 -Wall -Wextra -Werror -pthread examples/bad/single_thread.c -o st.exe
./st.exe          # threads=4 max_started=1 wall~1.20s (= serial time)
```

Recorded results (this host, 12 cores): `evals/README.md`.

## Where the knowledge comes from

- `arxiv-2603-03683` — CONCUR: concurrent-code benchmark; the ST category
  (thread-safe primitives under single-threaded execution) is the "fake
  parallelism" failure mode this skill detects.
- `posix-threads` — pthread_create/join, mutex semantics, affinity.
- `codex-37653` — openai/codex issue: zsh `jobs`-in-command-substitution
  limit bypass → 86 concurrent processes → kernel panic (2026-08-09).
- Empirical: gcc 16.1 + winpthreads on this host, recorded 2026-08-15.

## Related skills

- `concurrency-deadlock-and-lock-ordering` — what the lock is doing when
  there is no parallelism
- `memory-ordering-reasoning` — what the primitives guarantee (ordering, not
  parallelism)
- `performance-measurement-discipline` — wall-clock vs CPU-time measurement
- `binary-memory-leak-vm-allocator-diagnosis` — trigger-vs-cause attribution
  for incidents like codex#37653

## Evaluation

Synthetic: run the three fixtures; `real_parallel` must show max_working=4 and
~0.3s wall; `fake_parallel` must show max_working=1 and ~1.2s (serial);
`single_thread` must show max_started=1 and the serial time. The agent must
distinguish all three from the metrics, not from reading the code.
False-positive: real parallelism must not be flagged; `max_working` below the
core count is not a bug; a bounded process pool that can count its children is
fine.
Historical: codex#37653 — the `jobs`-in-command-substitution bypass must be
recognized as an accounting-visibility failure (limit cannot see its
entities), not "the shell is slow".
Adversarial: a workload whose threads wait at a lock (max_working=1) must be
reported as serialized even though all threads are alive; a "scales" claim
without a wall-clock baseline must be rejected.
Commands and verified facts: `evals/README.md`.
