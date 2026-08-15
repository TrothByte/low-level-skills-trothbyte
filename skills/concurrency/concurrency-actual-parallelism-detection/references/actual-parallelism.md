# Actual Parallelism Detection — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Thread-safe primitives are not parallelism

- **RULE**: mutexes, atomics, and condition variables guarantee correctness
  of shared access, not simultaneous execution. A program whose threads spend
  the whole workload holding one global lock is serialized, even though every
  thread-safe primitive is used correctly. This is the CONCUR "ST" category
  (thread-safe code under single-threaded execution).
- **WHY AI GETS IT WRONG**: equates "uses pthread_mutex" or "is thread-safe"
  with "is concurrent", and reports concurrency from the presence of the
  primitives.
- **CORRECT REASONING**: verify execution overlap (max concurrent working
  threads) and wall time; primitives are a precondition for correctness, not
  evidence of parallelism.
- **EXAMPLE** (bad): `fake_parallel.c` — 4 threads, global mutex around the
  entire workload.
- **COUNTEREXAMPLE** (good): `real_parallel.c` — 4 threads, no global
  serialization, per-thread slots.
- **VERIFICATION**: `max_working` is 4 for real, 1 for fake; wall is ~0.30s
  vs ~1.20s (recorded).
- **SOURCE**: arxiv-2603-03683 (CONCUR ST); posix-threads.

## 2. Spawned threads are not executing threads

- **RULE**: `pthread_create` counts as "started". Threads can then block on a
  lock, wait on I/O, or return immediately. The metric that matters is the
  max number of threads simultaneously inside the WORKLOAD (max_working), not
  the number created or currently alive (max_started).
- **WHY AI GETS IT WRONG**: reports N as concurrency because N threads were
  created; the alive-region counter also overcounts (queued lock waiters
  increment it).
- **CORRECT REASONING**: instrument the workload itself: enter/exit counters
  around the real computation, take the max. Compare against the started
  counter; a large gap is the "fake parallelism" signal.
- **EXAMPLE** (bad): `single_thread.c` — 4 threads created, all work on the
  main thread; max_started=1.
- **COUNTEREXAMPLE** (good): `real_parallel.c` — max_started=4, max_working=4.
- **VERIFICATION**: recorded metrics in `evals/README.md`
  (real: 4/4, fake: 4/1, single: 1/-).
- **SOURCE**: posix-threads; arxiv-2603-03683.

## 3. Wall clock is the arbiter of speedup

- **RULE**: parallelism claims are tested by wall-clock time against the
  serial baseline. CPU time is not a substitute — it sums across threads and
  is identical whether the work was parallel or serial.
- **WHY AI GETS IT WRONG**: reports CPU time (which "adds up nicely") or the
  per-thread time, and reads a large CPU total as speedup.
- **CORRECT REASONING**: wall time = real elapsed time. Serial baseline t0,
  parallel t1; a 4-thread workload that takes ~4×t0 is serialized, one that
  takes ≈t0 is parallel (bounded by cores).
- **EXAMPLE** (bad): "4 threads each used 0.3s of CPU, total 1.2s — great
  scaling" when the wall time is also 1.2s.
- **COUNTEREXAMPLE** (good): "wall time 0.30s vs serial 1.20s — ~4x speedup,
  consistent with 4 working threads on 12 cores."
- **VERIFICATION**: wall 0.304s (real) vs 1.206s (fake) on identical total
  work — recorded.
- **SOURCE**: posix-threads; empirical run on this host.

## 4. Concurrency limits must see the entities they count

- **RULE**: a parallelism limit works only if its accounting can observe the
  things it limits. codex#37653: zsh's `jobs` list — inside a command
  substitution (`$(...)`) `jobs` cannot see the parent shell's jobs, so the
  user's parallelism cap never counts them; the limit is effectively zero and
  86 concurrent processes were spawned, exhausting the system (kernel panic,
  reboot).
- **WHY AI GETS IT WRONG**: trusts the limit's own accounting without
  checking the measurement context; a limit that silently sees "0" is
  reported as enforcing.
- **CORRECT REASONING**: audit what the limiter actually measures and in
  which context. If the measurement runs in a context where the entities are
  invisible (subshell, another namespace, no `jobs` visibility), the limit is
  decorative. Verify by sampling the real entity count during a stress run.
- **EXAMPLE** (bad): `nproc`-limited fan-out where the limiter counts jobs in
  a context that cannot see them — the cap is bypassed.
- **COUNTEREXAMPLE** (good): the limiter counts from the same shell/context
  that owns the jobs, or uses a process-tree count that can see its children;
  a stress run shows the count actually stops at the cap.
- **VERIFICATION**: reproduce with a small zsh test (target machine): spawn
  N background jobs, run `jobs` inside `$(...)` vs directly, compare the
  counts; expect 0 vs N.
- **SOURCE**: codex-37653; empirical note (zsh behavior is shell-version
  dependent — marked INFERRED from the issue record).

## 5. Hardware bound: concurrency ≤ cores

- **RULE**: actual concurrency cannot exceed the available CPUs
  (`GetActiveProcessorCount` on Windows, `sched_getaffinity`/`sysconf` on
  Linux). Report the core count with every concurrency measurement so a
  1-core machine's results are not overread.
- **WHY AI GETS IT WRONG**: claims a 4x speedup or 8 "parallel" threads on a
  machine with 2 cores without checking.
- **CORRECT REASONING**: max_working and speedup are upper-bounded by the
  core count; if max_working == cores and the wall time is ~serial/cores, the
  claim is consistent.
- **EXAMPLE** (bad): "8 threads ran in parallel" when the machine has 4 cores
  and max_working was 4.
- **COUNTEREXAMPLE** (good): "4 working threads on 12 cores — parallel within
  the hardware bound."
- **VERIFICATION**: fixtures print the core count (12 on this host).
- **SOURCE**: posix-threads.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Primitives | mutex/atomics ≠ parallelism; verify overlap and wall time |
| Spawned vs working | max_started (alive) ≠ max_working (executing); instrument the workload |
| Wall clock | speedup is judged by wall time vs serial baseline, not CPU time |
| Limits | a limiter that cannot see its entities (codex#37653 `jobs`) enforces nothing |
| Hardware | concurrency ≤ available cores; report the core count |
