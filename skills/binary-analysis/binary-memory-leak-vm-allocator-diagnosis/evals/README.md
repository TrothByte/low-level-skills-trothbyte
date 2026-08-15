# Evaluation — binary-memory-leak-vm-allocator-diagnosis

Skill: `skills/binary-analysis/binary-memory-leak-vm-allocator-diagnosis`.
Stability target: `evaluated`. **RESEARCHED skill**: valgrind and `/proc`
are Linux-only and NOT available on this Windows host. The page-pool
mechanism IS demonstrated here with the VirtualAlloc/`GetProcessMemoryInfo`
analog (the same reuse-without-release semantics as mmap/munmap); the Linux
commands are documented for the target machine.

## Synthetic evals

| Case | Fixture | Expected | Recorded (Windows analog) |
|---|---|---|---|
| easy/positive | `bad/page_pool.c` | commit grows monotonically: pool reuse without release | `0 MiB -> 32 MiB (pool) -> 73 MiB (extra)` |
| easy/positive | `good/release_to_os.c` | commit returns to baseline when released | `0 MiB -> 40 MiB -> 0 MiB` |
| medium/negative | diagnosis of `bad/page_pool.c` | must name the mechanism (reuse-without-release), not "program leaks" | — |
| hard/negative | "fix" proposal | blanket unmap on every free must be flagged as a perf regression | — |

## False-positive evals (correct results must not be flagged)

- A pool that DOES release to the OS (`good/release_to_os.c`, `after=0 MiB`)
  is NOT a leak — the agent must not flag it.
- A deliberate bounded cache/pool is not a leak.
- `VirtualFree`/`munmap` on every free is NOT the automatic fix — flag it as
  a likely perf regression until measured.
- High RSS that is stable (no growth over the scenario) is not a leak.

## Historical evals

- Ghostty (VM-1 in the agent-failures survey): 71.49 GB on a 16 GB system;
  issues #10289, #10258 (gigabytes in seconds during resize). Root cause:
  `PageList.zig` page pool — non-standard pages (emoji/hyperlink rows)
  mapped directly, reuse path reset size metadata, `munmap` never called —
  VM leak, not heap, latent since v1.0. The agent must name this mechanism
  exactly.
- FP: the popular narrative "Claude Code broke Ghostty" is false attribution
  (trigger vs cause). mitchellh's primary write-up: no AI was used in the
  fix work.

## Adversarial evals

- A pool that releases only STANDARD-size pages but leaks non-standard ones
  (the Ghostty shape): the agent must find the defective class, not a
  blanket statement.
- A scenario where commit grows but the heap is flat: the agent must not
  conclude "no leak" from a clean heap checker.
- A "fix" that unmaps in a hot loop: flag the perf regression and propose
  the class-targeted unmap instead.
- A diagnosis that blames the trigger (repaint workload, agent usage) as the
  cause: must be corrected to the missing-unmap mechanism.

## Verification commands (ACTUAL, recorded 2026-08-15 — Windows analog)

```
gcc -O2 -Wall -Wextra -Werror examples/bad/page_pool.c -o pp.exe -lpsapi
./pp.exe
  page_pool: before=0 MiB after_pool=32 MiB after_extra=73 MiB pool_slots=32

gcc -O2 -Wall -Wextra -Werror examples/good/release_to_os.c -o rto.exe -lpsapi
./rto.exe
  release_to_os: before=0 MiB during=40 MiB after=0 MiB
```

## Target verification (Linux — the canonical commands)

```
valgrind --tool=massif --massif-out-file=massif.out ./page_pool_app
ms_print massif.out | head -40            # heap stays flat -> leak is NOT heap

watch 'grep -c "^" /proc/<pid>/maps'      # region count grows without bound
grep -E "VmRSS|VmSize" /proc/<pid>/status # RSS climbs, size climbs
strace -e trace=mmap,munmap -p <pid>      # mmap without matching munmap

# after the fix: repeat; RSS and region count must plateau
```

## Verified facts (on this host)

- VirtualAlloc commit charge grows monotonically when regions are re-pooled
  without release and returns to baseline when released — VERIFIED (the two
  runs above). This is the same semantics as mmap/munmap on Linux (per
  linux-mm-docs) and models the Ghostty mechanism.
- valgrind/massif and `/proc/<pid>/maps` output: UNVERIFIED here (Linux-only);
  the commands above are the exact verification plan.
- Ghostty numbers (37/71.49/96/130 GB, issues, dates): from the survey and
  mitchellh.com — KNOWN, cited to `ghostty-memory-leak`.

## Scoring (for routing eval)

- precision: every finding maps to a reference rule (1-5) and the layer,
  mechanism, and trigger are named distinctly.
- recall: pool leaks, layer misclassification, and blanket-unmap "fixes" are
  all covered by the fixtures.
- FP-rate: releasing pools, bounded caches, and stable RSS produce zero flags.

## Toolchain status (honest)

- valgrind: NOT installed (Windows host). `/proc`: not present. The skill is
  researched for the Linux toolchain; the Windows analog run recorded above
  verifies the underlying allocator mechanism, which is platform-independent
  at the semantic level (mmap↔VirtualAlloc, munmap↔VirtualFree).
