# Meta-Verification: Harness Validity — Reference Rules

Knowledge layer for `meta-verification-harness-validity`. Format: RULE →
WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE (bad) → COUNTEREXAMPLE
(good) → VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED /
UNVERIFIED.

All commands recorded against gcc 16.1.0 (MSYS2, Windows), 2026-08-15.
Relative paths assume the skill directory as CWD.

## 1. Ablation-delta: a harness must FAIL when its target is broken

- **RULE**: a test harness is valid evidence about a target only if it
  produces a different result for a broken target than for a correct one.
  Concretely: break the target (inject a defect), re-run the harness, and
  require failure. If it still passes, the harness does not test the target.
- **WHY AI GETS IT WRONG**: the agent equates "the harness ran and exited 0"
  with "the target is correct". But exit 0 measures the harness's own path,
  and an unconditional `return 0` always produces it. Models trained on
  "tests pass → correct" skip the step of proving the test would have caught
  the bug.
- **CORRECT REASONING**: think of the harness as a function from
  (target, inputs) → verdict. Validity is the property that the verdict
  depends on the target. The decisive experiment runs the SAME harness
  against two targets differing only in correctness. Only a differential
  verdict proves the dependence.
- **EXAMPLE** (bad): `examples/bad/harness_masks_bug.c` calls
  `bounded_value(150)` and `bounded_value(-40)`, discards both results, and
  returns 0. The target never clamps. Recorded: exit 0, prints "harness says
  PASS regardless of target behavior".
- **COUNTEREXAMPLE** (good): `examples/good/ablation_delta.c` asserts
  `bounded_value(150) == 100` etc. Compiled with the correct target: exit 0.
  Compiled with `-DBROKEN_TARGET` (target returns `x` unclamped): abort on
  the first assert, exit 0xC0000409. Same harness — only the target differs.
- **VERIFICATION**: the two commands above, exit codes recorded. If the two
  runs had identical exit codes, the harness would be void.
- **SOURCE**: arxiv-2606-20128 (allclose oracle that certifies buggy
  kernels — the oracle never fails on the broken kernel).

## 2. An assertion behind a never-taken branch is no gate at all

- **RULE**: a verification path that is skipped in the actual invocation
  does not verify anything. The harness must prove its checking code ran —
  typically by asserting on counters/state that only the checking code sets.
- **WHY AI GETS IT WRONG**: the agent reads the harness source, sees an
  `if (...) { ... return 1; }` block, and concludes "there is a check". It
  never traces whether that branch is reachable under the CI/default
  invocation. Flag-gated self-checks (`if (argc > 1 && ...)`) are a favorite
  — the flag is forgotten in the run script.
- **CORRECT REASONING**: reachability is part of the assertion. The harness
  should fail closed: if the checking code did not run, the harness must
  report that as a failure, not as a pass. A coverage gate (rule 3) is the
  mechanism.
- **EXAMPLE** (bad): `examples/bad/harness_no_execute_path.c` guards its
  only checksum assertion behind `argv[1][0] == '1'`. Invoked with no
  argument (as in CI), the assert never runs and the harness reports PASS.
  Recorded: exit 0.
- **COUNTEREXAMPLE** (good): `examples/good/harness_coverage_gate.c`
  increments per-branch counters and asserts each is non-zero after the
  calls — a branch the harness never reached fails the run explicitly.
- **VERIFICATION**: run both, compare exit codes; read the coverage-gate
  asserts. Recorded exit 0 for both bad files (masking) and exit 0 with
  branch asserts for the good file.
- **SOURCE**: arxiv-2607-00107 (Illusion of Safety — verification that
  looks rigorous while covering nothing); binutils-docs (objdump as an
  independent oracle for "did this code actually run").

## 3. Coverage gate: assert reachability, not just line execution

- **RULE**: for every region/branch of the target that participates in the
  predicate, the harness must (a) reach it and (b) assert it was reached.
  Instrumentation counters asserted after the run turn "coverage" from a
  report into a gate.
- **WHY AI GETS IT WRONG**: agents report "100% line coverage" and stop
  reasoning. Line coverage says a line executed; it does not say the result
  was checked, and it does not prevent dead-check patterns where the
  assertion is optimized away or shadowed. Coverage as a number is treated
  as evidence; coverage as an enforced gate is what matters.
- **CORRECT REASONING**: choose the predicate's decision points, make each
  an assertable counter, and make the harness FAIL if any counter is zero.
  Then add an adversarial broken-target run to confirm the gate trips.
- **EXAMPLE** (bad): a harness with an assert on `bounded_value(55)==55`
  only — the clamp branches (x<0, x>100) are never exercised, so a broken
  clamp of -40 or 150 passes.
- **COUNTEREXAMPLE** (good): `examples/good/harness_coverage_gate.c` has
  three counters (clamp_low, clamp_high, passthrough) and asserts all three
  are non-zero after the calls.
- **VERIFICATION**: `gcc examples/good/harness_coverage_gate.c && ./a.exe`
  → exit 0 with all branch asserts passing. Removing any one of the three
  calls makes its counter assert fail — that is the gate working.
- **SOURCE**: arxiv-2606-20128 (coverage without semantic checking);
  arxiv-2607-00107.

## 4. An eval that prints a result without a comparator is not verification

- **RULE**: if the harness's verdict is produced by a human reading output
  ("looks right", "matches my mental model"), it is not a gate. The verdict
  must come from an automated comparison against a ground truth oracle
  (known-good reference output, independent disassembly, re-execution
  equivalence).
- **WHY AI GETS IT WRONG**: the agent writes a harness that runs the target
  and prints values, then treats the printed run as a pass. There is no
  expected value in the code, so the harness cannot distinguish correct from
  incorrect output — the "verification" is the model's own eyeball.
- **CORRECT REASONING**: every printed value must have a machine-checked
  expected value or a reference implementation. For disassembly/decompiler
  evals, the oracle is `objdump` (binutils-docs) or re-assembly + execution;
  for GPU kernels, fixed-shape numeric comparison with tight tolerances.
- **EXAMPLE** (bad): a checksum eval that prints the computed checksum and
  ends — the CI log shows a number, the agent declares "verified".
- **COUNTEREXAMPLE** (good): `examples/good/harness_real_pass.c` computes
  and compares: `assert(checked_add(1,2,&r)==0 && r==3)` — the comparator
  is in the code.
- **VERIFICATION**: remove the asserts; the harness still prints — that
  diff proves the asserts are the gate. Recorded: with asserts exit 0.
- **SOURCE**: arxiv-2606-20128 (allclose oracle vs numeric ground truth);
  binutils-docs (objdump/readelf as independent oracle).

## 5. Self-test mode: prove the harness can pass AND can fail

- **RULE**: add a `--self-test` mode that feeds a known-good and a
  known-bad input (or target variant) and asserts the harness returns the
  expected verdict for each. A harness that cannot demonstrate both
  verdicts is untrustworthy.
- **WHY AI GETS IT WRONG**: the harness is validated only on the happy
  path, so a harness that always passes (rule 1) is never exposed. Self-test
  is seen as a nicety rather than the load-bearing proof of sensitivity.
- **CORRECT REASONING**: the self-test is the automated form of the
  ablation-delta. It should be a required step before trusting any PASS.
  For compiled targets, a build-time `-DBROKEN_TARGET` variant is the
  standard mechanism (see `good/ablation_delta.c`).
- **EXAMPLE** (bad): a harness validated only with a correct target; the
  model then claims the harness "catches bugs" without ever showing a run
  where it did.
- **COUNTEREXAMPLE** (good): `examples/good/ablation_delta.c` — two
  documented build configurations with recorded exit codes 0 and
  0xC0000409.
- **VERIFICATION**: both builds run and recorded. This is the reproduced
  proof the harness is target-sensitive.
- **SOURCE**: arxiv-2606-20128; arxiv-2607-00107.

## 6. "Green by hiding the violation" is the hardware twin of this bug

- **RULE**: making a gate report green without satisfying its predicate —
  e.g. a false_path that hides a real timing path, an unconstrained clock,
  an unconditional repaint — is not verification. The gate's predicate must
  still be violated whenever the property is violated.
- **WHY AI GETS IT WRONG**: in both software and hardware evals, agents
  optimize the reported number (test pass, WNS, QoR) instead of the
  underlying property, then certify based on the number.
- **CORRECT REASONING**: ask "what input would make this gate red?" If no
  such input exists, the gate cannot certify anything. For timing, remove
  the exception and check the path is genuinely irrelevant; for tests, do
  the ablation.
- **EXAMPLE** (bad): `bad/harness_masks_bug.c` and the "repaint" harnesses
  in claude-code#82057 — the gate is green because the defect path is never
  observed, not because the defect is absent.
- **COUNTEREXAMPLE** (good): ablation-delta builds and coverage gates that
  fail loudly on the defect.
- **VERIFICATION**: claude-code#82057 documented in
  `research/2026-08-15-agent-failures-survey.md`; the masking harnesses
  reproduced here (recorded exit 0).
- **SOURCE**: arxiv-2606-20128; arxiv-2607-00107.

## Quick reference table

| Concept | Rule in one line |
|---|---|
| Ablation-delta | break the target; a valid harness must go red |
| Reachability | a check that never runs verifies nothing |
| Coverage gate | assert counters per branch, not just report coverage |
| Comparator | verdict must come from code, not from eyeballing output |
| Self-test | show the harness can pass AND fail before trusting PASS |
| Hiding violations | green-by-exception certifies the exception, not the design |
