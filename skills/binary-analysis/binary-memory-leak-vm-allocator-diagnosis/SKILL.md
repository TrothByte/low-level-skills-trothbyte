---
name: binary-memory-leak-vm-allocator-diagnosis
description: Use when diagnosing high RAM/RSS/commit growth that heap profilers (ASan/leak checkers) do not explain — VM-level leaks: page pools that reuse mmap/mmap'd regions without munmap, unbounded pools, allocator reuse-without-release. Ghostty PageList.zig 37-130 GB; agent is the trigger, not the cause.
---

# Binary Analysis: VM/Allocator Leak Diagnosis

## When to use

- RAM/RSS/commit grows without bound but the heap looks clean (no leaked
  `malloc` blocks, ASan clean).
- A terminal, editor, or long-running service balloons to tens of GB over
  days; reports mention "emojis", "resize", "repaint".
- An allocator or page pool reuses regions but the unmap path is skipped.
- The diagnosis must separate the *mechanism* (leak) from the *trigger*
  (what made the leak observable).

## When not to use

- A classic heap leak (lost pointers) — use ASan/leak-checkers
  (`sanitizer-report-reading`, `sanitizer-agent-ci-loop`).
- Cache eviction policy tuning where memory is intentionally retained.
- Windows-only analysis with no Linux path — this skill's canonical
  tooling (valgrind, `/proc/<pid>/maps`) is Linux.
- Kernel/driver allocations — use the kernel skills.

## What the agent often gets wrong

- False attribution: "Claude Code broke Ghostty." The trigger (massive
  terminal use → constant repaint → non-standard pages) made a latent bug
  observable; the cause is the allocator: `PageList.zig` reused pages whose
  metadata had been reset, and `munmap` was never called on them — a VM leak,
  not heap, present since v1.0.
- Treating "heap profiler clean" as "no leak": VM leaks (mmap'd regions kept
  mapped) are invisible to heap checkers; RSS/commit grows anyway.
- Fixing the symptom: adding a generic `munmap` on every free (perf
  regression) instead of recognizing the pool-reuse mechanism and making the
  unmap path unconditional for non-pooled pages.
- Measuring only allocation count, not VM footprint (`/proc/<pid>/maps` total
  vs heap size; commit vs working set).
- Claiming a leak is "fixed" without re-running the growth scenario.

## How to reason correctly

1. Classify the leak layer FIRST: heap (lost `malloc` blocks) vs VM (mapped
   regions never unmapped) vs commit (private committed pages). The symptom
   — RSS and commit growth with a flat heap — points to VM.
2. Find the pool: an allocator keeps freed regions for reuse. Check the unmap
   path: is `munmap`/`VirtualFree` reachable for every mapping? Ghostty's
   defect: non-standard pages (emoji/hyperlink rows) went through direct
   `mmap`; the reuse path reset the size metadata but never unmapped.
3. Measure the right things: `/proc/<pid>/maps` region count and total size,
   RSS delta over the scenario, commit charge. On Windows:
   `GetProcessMemoryInfo` PagefileUsage + WorkingSetSize.
4. Separate trigger from cause: the trigger is the workload that makes the
   path hot (user's usage, agent repaints); the cause is the missing unmap.
   "No AI was used in my work here" (mitchellh) is the calibration — the fix
   is in the allocator, not the trigger.
5. Verify the fix: run the growth scenario again; RSS/commit must plateau
   after the fix and return to baseline when the pool is released.

## What to verify

- The mapping is reachable: `munmap`/`VirtualFree` appears on every free path
  or the pool has a bounded size.
- RSS/commit grows monotonically before the fix and plateaus/returns to
  baseline after it.
- Heap/allocator checkers are clean (proving the leak is not heap).
- The diagnosis names both the mechanism (pool reuse without unmap) and the
  trigger (workload), and does not attribute the cause to the trigger.

## How to verify

On this host (Windows): valgrind and `/proc` are NOT available; the pool
mechanism is demonstrated with the VirtualAlloc analog and
`GetProcessMemoryInfo`:

```
gcc -O2 -Wall -Wextra -Werror examples/bad/page_pool.c -o pp.exe -lpsapi
./pp.exe          # commit grows: 0 -> 32 MiB (pool) -> 73 MiB (extra)
gcc -O2 -Wall -Wextra -Werror examples/good/release_to_os.c -o rto.exe -lpsapi
./rto.exe         # 0 -> 40 MiB -> 0 MiB (released back to the OS)
```

Target (Linux) commands:

```
valgrind --tool=massif --massif-out-file=massif.out <prog>
ms_print massif.out | head            # heap is flat; RSS is not explained
while true; do awk '/Rss/ {print}' /proc/<pid>/status; sleep 5; done
grep -c '^' /proc/<pid>/maps          # region count grows without bound
```

## Where the knowledge comes from

- `ghostty-memory-leak` — mitchellh.com, 2026-01-10: PageList.zig page pool,
  missing `munmap`, 37-130 GB reports; primary analysis, the fix is in the
  allocator.
- `linux-mm-docs` — mmap/munmap semantics, page allocator, vm vs heap.
- Empirical: Windows VirtualAlloc/GetProcessMemoryInfo demonstration on this
  host, recorded 2026-08-15 (mechanism only; valgrind/`/proc` commands are
  the Linux target plan).

## Related skills

- `sanitizer-report-reading` — why heap checkers stay clean for VM leaks
- `binary-disassembly-decompilation-fidelity` — don't trust "looks clean",
  verify behaviorally
- `performance-measurement-discipline` — measuring the right metric (RSS vs
  heap) before fixing
- `concurrency-actual-parallelism-detection` — separating mechanism from
  trigger in "the agent broke X" narratives

## Evaluation

Synthetic: run `bad/page_pool.c` and explain the monotonic commit growth
(recorded 0→32→73 MiB); run `good/release_to_os.c` and explain the return to
baseline (0→40→0 MiB); the mechanism (pool reuse without release) must be
named, not "the program leaks memory".
False-positive: a pool that releases back to the OS is NOT a leak; a
deliberate cache is NOT a leak; `VirtualFree`/`munmap` on every free is not
the automatic fix.
Historical: Ghostty — must name the missing `munmap` in PageList.zig as the
cause and the usage pattern as the trigger; must reject "Claude Code broke
Ghostty" as false attribution.
Adversarial: a pool that releases only standard-size pages (the Ghostty shape)
must be caught; a "fix" that unmaps every free must be flagged as a perf
regression unless measured.
Commands and verified facts: `evals/README.md`.
