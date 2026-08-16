---
name: data-race-kernel-detection
description: Use when hunting or reviewing kernel data races, reading KCSAN reports, or adding READ_ONCE/WRITE_ONCE/locks. Teaches the KCSAN model (watchpoint sampling, plain vs marked accesses), race classes in kernel context, and distinguishing real races from false positives.
---

# Kernel Data Race Detection (KCSAN & Friends)

## When to use

- A KCSAN report ("BUG: KCSAN: data-race in ...") or a "value changed" line
  needs triage and a correct fix.
- Reviewing kernel patches that touch shared data without locks or
  READ_ONCE/WRITE_ONCE.
- Deciding whether `data_race()` annotation, a lock, or READ_ONCE/WRITE_ONCE
  is the right fix, and whether the fix actually removes the race.
- Setting up kernel builds with KCSAN/KMSAN/KASAN for CI race detection.

## When not to use

- User-space C11/C++11/Rust data races — use ThreadSanitizer and
  `memory-ordering-reasoning` (different model and tooling).
- Designing ordering/barrier semantics (release/acquire) — use
  `kernel-rcu-memory-barriers`.
- Lock ordering and deadlocks that KCSAN cannot see — use
  `deadlock-kernel-prevention`.
- Reading a race report without being able to modify kernel code.

## What the agent often gets wrong

- Treats a KCSAN report as a compiler bug. KCSAN detects conflicting
  accesses on real executed schedules; the fix is in the code, not in the
  build flags.
- "Fixes" reports by adding `data_race()` everywhere. `data_race()` tells
  KCSAN the agent *declares* the race benign — it does not make the race
  safe. Legitimate use requires a documented reason (statistics counters,
  diagnostic values). Applying it without justification is masking.
- Concludes "no race" from x86 test runs. x86 TSO hides reordering; races
  that never surface on x86 reproduce on ARM/RISC-V. Absence of a KCSAN
  report is also not proof: KCSAN samples (unsound, false negatives
  possible).
- Thinks marked accesses are never raced. `READ_ONCE`/`WRITE_ONCE` are
  marked: KCSAN does not report them as data races, but two conflicting
  marked writes, or a marked write racing a plain one, can still be logic
  bugs (use `ASSERT_EXCLUSIVE_WRITER` etc. to check those properties).
- Confuses a data race with a race condition. A data race is a C-level
  property (conflicting plain accesses); a race condition is unexpected
  behavior. KCSAN detects data races; `ASSERT_EXCLUSIVE_*` and lockdep cover
  the rest. "KCSAN is clean" does not mean "the concurrency is correct".
- Ignores the "race at unknown origin" report shape: a value change with one
  traced access is still a real report (missing instrumentation, DMA, or a
  racing writer without a watchpoint).
- Misses that KCSAN's default permissive mode hides common benign races; the
  strict configuration (`CONFIG_KCSAN_STRICT=y`) follows the LKMM and is the
  configuration to reason against.

## How to reason correctly

1. Read the report: which function, which variable, which access types
   (read/write), which two threads/contexts, and whether a "value changed"
   line exists.
2. Classify each access: plain (instrumented, race-checked) vs marked
   (`READ_ONCE`, `WRITE_ONCE`, atomics — never race-reported but still
   checkable).
3. Decide intentionality: is the concurrent access intended? If yes,
   document why and use `data_race()`/`__data_racy` with a comment; if no,
   the fix is synchronization (lock) or marked accesses with correct
   ordering.
4. Pick the fix by the protocol: single variable → `READ_ONCE`/`WRITE_ONCE`;
   flag+payload → release/acquire or RCU (`kernel-rcu-memory-barriers`);
   invariant across several fields → a lock.
5. Verify against strict KCSAN and, for ordering, the LKMM
   (`tools/memory-model`); do not accept the default permissive run as
   proof.
6. For properties KCSAN cannot see, add `ASSERT_EXCLUSIVE_WRITER(_SCOPED)`
   or `ASSERT_EXCLUSIVE_ACCESS(_SCOPED)` to turn them into reports.

## What to verify

- No plain access to a variable another context writes (every conflicting
  plain access resolved to marked/locked/`data_race`-with-comment).
- `data_race()` uses carry a comment stating why the race is benign; the
  count of uncommented `data_race()` in the patch is zero.
- The report's exact shape (two traced accesses vs unknown origin) is
  explained, not glossed.
- Strict KCSAN (`CONFIG_KCSAN_STRICT=y`, `CONFIG_KCSAN_WEAK_MEMORY=y`) is
  clean on the exercise path, or the remaining reports are accounted for.
- Logic races beyond data races are covered by `ASSERT_EXCLUSIVE_*` or a
  documented manual argument.

## How to verify

Host-side (logic of the KCSAN detection criterion; no kernel on this host):

```
python examples/good/kcsan_model.py
python examples/good/race_fix_demo.py
python examples/bad/kcsan_masking.py
gcc -Wall -Wextra -Werror -O2 -pthread examples/good/race_free_counter.c -o /tmp/c1.exe && /tmp/c1.exe
gcc -Wall -Wextra -Werror -O2 -pthread examples/bad/race_plain_counter.c -o /tmp/c2.exe && /tmp/c2.exe
```

Target kernel (RESEARCHED; kernel build + boot required, not run here):

```
# build with KCSAN in strict mode
scripts/config -e KCSAN -e KCSAN_STRICT -e KCSAN_WEAK_MEMORY \
               -e KCSAN_REPORT_RACE_UNKNOWN_ORIGIN -e KASAN
make -j$(nproc)
qemu-system-x86_64 -kernel arch/x86/boot/bzImage -append "console=ttyS0 kcsan.udelay_task=100"
# exercise the reviewed path; read dmesg for KCSAN reports
```

## Where the knowledge comes from

- `kernel-source` — KCSAN implementation and Kconfig semantics
- `kernel-kcsan-docs` — KCSAN doc: usage, report format, data-race
  definition, selective analysis, ASSERT_EXCLUSIVE_* family
- `linux-memory-barriers` — marked accesses and ordering
- `linux-rcu` — when the fix is RCU rather than READ_ONCE
- `kernel-coding-style` — kernel conventions for access marking
- `kernel-rcu-memory-barriers` — companion skill for ordering fixes

## Related skills

- `kernel-rcu-memory-barriers` (recommend) — the ordering half: when
  marking alone is not enough
- `deadlock-kernel-prevention` (recommend) — races that manifest as
  ordering/lock bugs KCSAN cannot see
- `memory-ordering-reasoning` (conflict) — user-space model; do not
  translate mechanically
- `fuzzing-harness-kernel` (recommend) — syzkaller/KCSAN as the CI race
  detector
- `invariant-identification` (recommend) — state the exclusivity invariant
  (`ASSERT_EXCLUSIVE_WRITER`) formally

## Evaluation

- Synthetic: a plain-plain race, a marked-vs-plain conflict, an unknown-
  origin report, and a data_race masking patch — each must be triaged to the
  correct class and fix.
- False-positive: `good/race_free_counter.c`, a legitimately annotated
  statistics counter, and a report whose fix is a lock (not more marking)
  must NOT be flagged.
- Historical: Linux races found by KCSAN (e.g. file system and networking
  reports whose fix was a READ_ONCE or a lock) — mechanism reproduced by the
  Python model.
- Adversarial: `bad/kcsan_masking.py` "silences" every report by annotation
  without justification — must be caught by demanding the benign-race
  rationale.
- Commands recorded on this host: `evals/README.md`.
