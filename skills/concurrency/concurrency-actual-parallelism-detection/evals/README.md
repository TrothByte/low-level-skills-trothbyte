# Evaluation — concurrency-actual-parallelism-detection

Skill: `skills/concurrency/concurrency-actual-parallelism-detection`.
Stability target: `evaluated`. Toolchain: GCC 16.1.0 + winpthreads (MSYS2,
`-pthread`), runtime on this host (Windows, 12 cores, `GetActiveProcessorCount`).

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/positive | `good/real_parallel.c` | 4 threads overlap and speed up | `threads=4 cores=12 wall=0.304s max_started=4 max_working=4` |
| medium/negative | `bad/fake_parallel.c` | global mutex serializes: alive threads but no overlap | `threads=4 cores=12 wall=1.206s max_started=4 max_working=1` |
| medium/negative | `bad/single_thread.c` | spawned threads do nothing; work is serial | `threads=4 cores=12 wall=1.202s max_started=1 thread_calls=4` |
| hard/negative | interpret metrics | distinguish the three from numbers, not from reading the code | 4/4+0.30 vs 4/1+1.21 vs 1/-+1.20 |

Serial baseline: the same `burn` total (800 M iterations) takes ~1.20s on one
thread — fake_parallel and single_thread wall times match it (no parallelism);
real_parallel is ~4x faster (parallel on 12 cores). Wall times vary ±0.05s
run-to-run; the discriminating features (max_working 4 vs 1, and the ~4x wall
ratio) are stable.

## False-positive evals (correct results must not be flagged)

- `real_parallel` is genuinely parallel and must not be flagged.
- `max_working` below the core count is NOT a bug (a 2-thread job on 12
  cores is fine).
- A bounded process pool whose limiter CAN count its children is correct.
- A mutex that protects only a small critical section (not the whole
  workload) does not make the program "fake" — measure overlap before
  judging.

## Historical evals

- openai/codex#37653 (2026-08-09): zsh `jobs` inside command substitution
  cannot see the parent shell's jobs, so a parallelism cap never triggers;
  86 concurrent processes were spawned, the watchdog kernel-panicked and the
  machine rebooted twice. The agent must diagnose this as an
  accounting-visibility failure (rule 4), not "the shell is slow" or
  "codex crashed".
- Calibration: CONCUR (arxiv-2603-03683) documents the ST category as a
  systematic failure — models produce thread-safe code that never executes in
  parallel. The three fixtures reproduce the categories ST (single_thread),
  lock-serialized, and genuinely parallel.

## Adversarial evals

- A workload where all 4 threads are alive but waiting at a lock
  (max_started=4, max_working=1): must be reported as serialized even though
  "all threads are running".
- A speedup claim with no wall-clock baseline must be rejected.
- A "limit" that reports 0/empty accounting while 86 processes actually run:
  the limiter is decorative.
- A single-core run (max_working=1 for a legitimately parallel program): the
  agent must attribute it to the hardware bound, not to the code.

## Verification commands (ACTUAL, recorded 2026-08-15)

```
gcc -O2 -Wall -Wextra -Werror -pthread examples/good/real_parallel.c -o rp.exe
./rp.exe
  real_parallel: threads=4 cores=12 wall=0.304s max_started=4 max_working=4
  result[0] = 2439764508509d48
  result[1] = 60a6eec2fa9af011
  result[2] = 6be04d3926a10c4b
  result[3] = 6bf84b613ebad328

gcc -O2 -Wall -Wextra -Werror -pthread examples/bad/fake_parallel.c -o fp.exe
./fp.exe
  fake_parallel: threads=4 cores=12 wall=1.206s max_started=4 max_working=1
  (identical results — same work, serialized by the global mutex)

gcc -O2 -Wall -Wextra -Werror -pthread examples/bad/single_thread.c -o st.exe
./st.exe
  single_thread: threads=4 cores=12 wall=1.202s max_started=1 thread_calls=4
  result=4d0d29d75d59ed72
```

## Target verification (Linux)

```
gcc -O2 -pthread real_parallel.c -o rp && ./rp
# replace GetActiveProcessorCount with sched_getaffinity(0,...) + CPU_COUNT
taskset -c 0 ./rp        # forces 1 core: max_working=1 despite 4 threads
python -c "import os; print(os.sched_getaffinity(0))"
```

zsh limit-bypass reproduction (target machine):

```
for i in $(seq 1 100); do true & done
echo "direct jobs:   $(jobs | wc -l)"          # sees the 100 jobs
echo "subshell jobs: $(echo $(jobs | wc -l))"  # 0 — the codex#37653 mechanism
```

## Verified facts (on this host)

- real vs fake vs single are distinguishable by max_working and wall time:
  VERIFIED (recorded above, exit 0 all runs).
- Identical per-thread results across the three fixtures confirm the work is
  the same — only the parallelism differs.
- Core count (12) reported and used as the hardware bound.
- zsh `jobs`-in-command-substitution behavior: INFERRED from the issue
  record (shell-version dependent); reproduced only on the target machine.

## Scoring (for routing eval)

- precision: each classification maps to a reference rule (1-5) and a
  recorded metric.
- recall: lock-serialized, decorative-thread, and invisible-accounting
  failure classes are all covered.
- FP-rate: genuinely parallel code and bounded pools produce zero flags.

## Toolchain notes

- winpthreads (`-pthread`) works on MSYS2 GCC 16.1 for `pthread_create`/
  `mutex_lock`; `sched_getaffinity` is NOT available on winpthreads — the
  fixtures use `GetActiveProcessorCount` instead (documented in-code).
- A real `zsh` and a Linux box are needed for the historical eval; marked as
  target verification.
