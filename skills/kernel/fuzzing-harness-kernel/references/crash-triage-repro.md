# Kernel Fuzzing: Crash Triage, Reproducing, and Classification

## 1. Classify the report before triaging it

- **RULE**: the report header decides the workflow: KASAN (memory
  corruption), KCSAN (data race), UBSAN (UB), hung task / soft lockup
  (liveness). "It crashed" is not a class; a KASAN UAF and a KCSAN race
  are different bugs with different fixes and different verifiers. KNOWN
  (kernel dev-tools docs; syzkaller report handling).
- **WHY AI GETS IT WRONG**: all sanitizer output is treated as "a crash
  report"; the fix suggested ignores the detector that actually fired.
- **CORRECT REASONING**: read the first lines ("BUG: KASAN: use-after-free",
  "BUG: KCSAN: data-race", "UBSAN: shift-out-of-bounds"), then route.
- **EXAMPLE** (bad): "fixing" a KCSAN report by adding a bounds check.
- **COUNTEREXAMPLE** (good): a KCSAN report routes to marking/locking
  (data-race-kernel-detection); a KASAN UAF routes to lifetime review.
- **VERIFICATION**: the classification table is part of the triage fixtures.
- **SOURCE**: kernel-source (KASAN/KCSAN/UBSAN reports); syzkaller-docs
  (report handling) [proposed].

## 2. Reproduce before you fix, minimize before you reproduce widely

- **RULE**: `syz-reproduce` turns a stored crash into a reproducible
  program on a fresh VM; `syz-minimize` shrinks it. A bug that cannot be
  reproduced is not fixed, and an un-minimized repro wastes triage time.
  KNOWN (syzkaller docs).
- **WHY AI GETS IT WRONG**: the agent "fixes" from the crash log without
  ever running the reproducer, then claims success.
- **CORRECT REASONING**: get a deterministic repro, minimize it, run it
  against the unpatched and patched kernels (ablation), then fix.
- **EXAMPLE** (bad): a patch based on a non-reproducible crash report.
- **COUNTEREXAMPLE** (good): the minimized repro fails on HEAD and passes
  on the fixed tree.
- **VERIFICATION**: the syzkaller commands are in the SKILL.md target
  section (RESEARCHED).
- **SOURCE**: syzkaller-docs (reproduce, minimize, reporting)
  [proposed].

## 3. Harness bugs must be separated from target bugs first

- **RULE**: a crash in the harness itself (size==0 deref, missing reset,
  leak-induced OOM) invalidates every later report until fixed. Triage must
  first answer "did the harness behave correctly on this input?". KNOWN
  (libFuzzer/OSS-Fuzz practice).
- **WHY AI GETS IT WRONG**: harness crashes are reported as target
  findings, and target triage time is spent on the harness's own bug.
- **CORRECT REASONING**: reproduce on the harness in isolation; fix the
  harness; re-run. Only target faults enter the bug list.
- **EXAMPLE** (bad): `bad/LLVMFuzzer_harness_bad.c` — an OOB read on
  `size==0` reported as a target UAF.
- **COUNTEREXAMPLE** (good): `good/LLVMFuzzer_harness.c` guards size; any
  later crash is attributable to the target.
- **VERIFICATION**: both fixtures compile; the size-guard diff is the
  discriminator.
- **SOURCE**: libfuzzer-docs (zero-input, entry point); oss-fuzz.

## 4. Coverage growth is the run's health metric, not crashes

- **RULE**: the fuzzer's dashboard should show sustained new-coverage
  discovery; flat coverage with zero crashes usually means the target is
  not being reached (setup failure, wrong description, missing feedback),
  not that it is correct. KNOWN (syzkaller dashboard; fuzzing practice).
- **WHY AI GETS IT WRONG**: exec/s and crash counts are quoted as the
  outcome; coverage saturating on setup code is taken for "well fuzzed".
- **CORRECT REASONING**: compare coverage of the target functions across
  runs; if the interesting paths never gain coverage, fix the harness
  first.
- **EXAMPLE** (bad): a run whose coverage is flat and limited to the open/
  error paths reported as "24h fuzzed, clean".
- **COUNTEREXAMPLE** (good): `good/coverage_fuzzer.py` shows coverage
  growth directly tied to bug discovery.
- **VERIFICATION**: the Python fixtures print coverage counts per
  iteration.
- **SOURCE**: syzkaller-docs (usage, dashboard) [proposed]; aflpp-docs.
