# Data Race Detection: Race Classes and Fix Selection in Kernel Context

## 1. The fix must match the access protocol

- **RULE**: single independent variable → READ_ONCE/WRITE_ONCE; flag+payload
  → release/acquire or RCU; multi-field invariants → a lock. Choosing the
  wrong tool either under-syncs (data race remains) or over-syncs (the
  report is "fixed" but the ordering contract is broken). KNOWN (Linux
  access-marking documentation; LKMM).
- **WHY AI GETS IT WRONG**: agents uniformly answer "add READ_ONCE" to any
  KCSAN report, including flag+payload and multi-field cases where marking
  alone leaves the race (see kernel-rcu-memory-barriers).
- **CORRECT REASONING**: name the protocol before the fix: what is shared,
  between which contexts, and what ordering the reader/writer depend on.
- **EXAMPLE** (bad): a flag+payload protocol "fixed" with READ_ONCE only —
  the payload can still be read stale.
- **COUNTEREXAMPLE** (good): the writer uses `smp_store_release`, the reader
  `smp_load_acquire`; the model shows no stale payload under any schedule.
- **VERIFICATION**: `good/race_fix_demo.py` shows the marking-only fix
  failing and the release/acquire fix passing on the same schedules.
- **SOURCE**: linux-memory-barriers; linux-rcu; kernel-kcsan-docs [proposed].

## 2. data_race() documents intent; it does not create safety

- **RULE**: `data_race(expr)` and `__data_racy` tell KCSAN to ignore races
  involving the expression; they are declarations of "intended, benign
  race" and must be accompanied by the reason. They do not add ordering or
  atomicity. KNOWN (KCSAN doc, "Selective analysis"; access-marking doc).
- **WHY AI GETS IT WRONG**: data_race() is used as a silencing tool to make
  KCSAN clean, turning the detector into a rubber stamp.
- **CORRECT REASONING**: allow data_race() only where the race is
  demonstrably benign (statistics counters, best-effort diagnostics) and
  record the rationale; everything else gets a real fix.
- **EXAMPLE** (bad): `bad/kcsan_masking.py` wraps every report in
  data_race-style silencing and prints "KCSAN clean".
- **COUNTEREXAMPLE** (good): the model rejects a silenced race without a
  documented reason and accepts a statistics counter that is annotated with
  justification.
- **VERIFICATION**: `python examples/bad/kcsan_masking.py` prints the
  unmasked remaining race; the good model requires the rationale field.
- **SOURCE**: kernel-kcsan-docs (Selective analysis) [proposed];
  kernel-source (include/linux/compiler_types.h).

## 3. x86-clean is not race-free

- **RULE**: x86's TSO memory model hides ordering races that manifest on
  weakly-ordered CPUs; and even on x86, a data race is UB. Reasoning must be
  done at the kernel memory-model level, not by "it passed on my machine".
  KNOWN (LKMM; kernel practice).
- **WHY AI GETS IT WRONG**: local x86 test runs are cited as evidence of
  race-freedom despite the model gap.
- **CORRECT REASONING**: run the race on the model (all schedules) and, on
  target, under KCSAN with weak-memory modeling
  (CONFIG_KCSAN_WEAK_MEMORY) and ideally on an ARM/RISC-V machine.
- **EXAMPLE** (bad): `bad/race_plain_counter.c` on x86 often shows a
  plausible-looking total — the lost-update evidence is timing-dependent.
- **COUNTEREXAMPLE** (good): `good/race_free_counter.c` is correct under
  every schedule by construction.
- **VERIFICATION**: the C fixtures demonstrate the x86 observation; the
  model enumerates the schedules where the bad one loses updates.
- **SOURCE**: linux-memory-barriers; kernel-kcsan-docs (Weak Memory)
  [proposed].

## 4. Not all concurrency bugs are data races

- **RULE**: KCSAN detects data races; race conditions that involve marked
  accesses or check-then-act need ASSERT_EXCLUSIVE_* (rule 4 of
  kcsan-model.md) or lockdep, and the historical example is Dirty COW
  (CVE-2016-5195) whose window is not a plain-access data race at all.
  KNOWN.
- **WHY AI GETS IT WRONG**: "KCSAN clean" is equated with "no concurrency
  bugs", skipping logic races and TOCTOU windows.
- **CORRECT REASONING**: list the concurrency properties the code needs
  (single writer, exclusivity, atomic check-act) and name the detector for
  each: KCSAN, ASSERT_EXCLUSIVE_*, lockdep.
- **EXAMPLE** (bad): a check-then-act TOCTOU on a COW PTE passed KCSAN
  review (no plain-plain conflict) and was exploitable (CVE-2016-5195).
- **COUNTEREXAMPLE** (good): the fix re-checks the state under the page-table
  lock, turning the window into a protected critical section.
- **VERIFICATION**: the fixtures classify the dirty-COW shape as a
  non-data-race concurrency bug (no KCSAN hit expected).
- **SOURCE**: kernel-kcsan-docs (Beyond Data Races) [proposed];
  kernel-source (mm/memory.c fix commit).
