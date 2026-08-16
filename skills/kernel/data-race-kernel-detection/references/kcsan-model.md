# Data Race Detection: the KCSAN Model

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED /
UNVERIFIED.

## 1. A data race is two conflicting plain accesses, one of them a write

- **RULE**: per the LKMM, two accesses race if they conflict (same location,
  at least one write), happen concurrently, and at least one is a plain
  access. Marked accesses (READ_ONCE, WRITE_ONCE, atomics) are not reported
  as data races by KCSAN. KNOWN (KCSAN doc, "Data Races").
- **WHY AI GETS IT WRONG**: agents assume KCSAN reports "any concurrent
  access"; they then "fix" reports by converting plain accesses to marked
  ones even when the real problem is a missing lock or barrier.
- **CORRECT REASONING**: classify the accesses first. A marked-vs-marked
  conflict is NOT a data race (KCSAN stays silent) but may still be a logic
  race; a marked-vs-plain conflict IS a data race (KCSAN reports it).
- **EXAMPLE** (bad): `bad/race_plain_counter.c` — a plain `count++` raced
  by other threads; KCSAN reports it, and on this host the lost updates are
  observable.
- **COUNTEREXAMPLE** (good): `good/race_free_counter.c` — the increment is
  under a mutex; no conflict, no report, correct count.
- **VERIFICATION**: the Python model checks the exact definition
  (conflict ∧ concurrent ∧ plain); the C fixtures run on the host.
- **SOURCE**: kernel-kcsan-docs (Data Races) [proposed]; kernel-source.

## 2. KCSAN samples: a clean run is not a proof of absence

- **RULE**: KCSAN uses watchpoint-based sampling (a watchpoint is set up
  periodically, the access is stalled, and a conflicting access fires it).
  Sampling is unsound (false negatives possible). A clean KCSAN run proves
  no race was *observed*, not that no race exists. KNOWN (KCSAN doc,
  "Implementation Details"/"Analysis Accuracy").
- **WHY AI GETS IT WRONG**: "KCSAN clean" is treated as "race-free",
  especially on x86 where TSO hides ordering problems anyway.
- **CORRECT REASONING**: treat a clean run as evidence about the exercised
  schedules only; increase sampling (lower `kcsan.skip_watch`,
  `kcsan.udelay_task`), use strict mode, and reason about the LKMM for the
  unobserved schedules.
- **EXAMPLE** (bad): certifying a patch as race-free from one boot without
  exercising the race window.
- **COUNTEREXAMPLE** (good): `good/race_fix_demo.py` models the window and
  shows the fix removes the conflict under every schedule.
- **VERIFICATION**: the model enumerates schedules; the target KCSAN
  configuration with higher sampling is documented in the SKILL.md commands.
- **SOURCE**: kernel-kcsan-docs (Implementation Details, Analysis Accuracy)
  [proposed].

## 3. Unknown-origin reports are real reports

- **RULE**: the "race at unknown origin, with read to ... / value changed"
  shape means the racing side was not traced (missing instrumentation, DMA,
  or an uninstrumented writer). It is still a genuine race report and is on
  by default via CONFIG_KCSAN_REPORT_RACE_UNKNOWN_ORIGIN. KNOWN (KCSAN doc,
  "Error reports").
- **WHY AI GETS IT WRONG**: the single-stack-trace shape looks like a
  non-race or a flaky read; agents dismiss it instead of hunting the second
  writer (which is often the actual bug).
- **CORRECT REASONING**: treat "value changed" as a smoking gun: the memory
  location changed between the two value probes. Find the un-instrumented
  writer (device DMA, assembly, a function with KCSAN disabled).
- **EXAMPLE** (bad): ignoring a value-changed report because "only one
  access is shown".
- **COUNTEREXAMPLE** (good): the model flags the change and traces the
  missing writer via instrumentation metadata.
- **VERIFICATION**: `good/kcsan_model.py` reproduces both report shapes and
  treats them consistently.
- **SOURCE**: kernel-kcsan-docs (Error reports) [proposed].

## 4. Marked accesses need ASSERT_EXCLUSIVE_* to check exclusivity logic

- **RULE**: marked accesses are not data races, so KCSAN alone cannot
  enforce single-writer or exclusive-access rules over marked code. The
  `ASSERT_EXCLUSIVE_WRITER(_SCOPED)`, `ASSERT_EXCLUSIVE_ACCESS(_SCOPED)`,
  and `ASSERT_EXCLUSIVE_BITS` macros turn those properties into reports.
  KNOWN (KCSAN doc, "Race Detection Beyond Data Races").
- **WHY AI GETS IT WRONG**: after marking everything, agents claim "KCSAN
  clean → concurrency correct", leaving writer-vs-writer races and
  check-then-act bugs (CVE-2016-5195 shape) undetected.
- **CORRECT REASONING**: separate the two obligations — (a) no plain-plain
  conflicts (KCSAN), (b) the intended exclusivity properties actually hold
  (ASSERT_EXCLUSIVE_*). Cover both.
- **EXAMPLE** (bad): two marked writers to the same variable; no KCSAN
  report, wrong final value depending on timing.
- **COUNTEREXAMPLE** (good): the writer holds the lock and uses
  `ASSERT_EXCLUSIVE_WRITER_SCOPED(shared_foo)`; a racing marked writer trips
  the assert.
- **VERIFICATION**: the model implements the exclusivity check as a second
  pass over the schedule (see `good/kcsan_model.py`).
- **SOURCE**: kernel-kcsan-docs (Race Detection Beyond Data Races)
  [proposed]; kernel-source.
