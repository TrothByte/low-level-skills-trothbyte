# Meta-Eval-Runner: The Eval Loop — Reference Rules

Knowledge layer for `meta-eval-runner`. Format: RULE → WHY AI GETS IT WRONG →
CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.
Uncertainty marked KNOWN / INFERRED / UNVERIFIED.

## 1. Four eval kinds, one loop

- **RULE**: an eval run covers exactly one of four kinds — synthetic
  (hand-built fixtures), false-positive (correct code that must not be flagged),
  adversarial (compiles-but-wrong), historical-CVE (known vulnerability diffs).
  All four use the same loop: fixture → gate → verdict → record.
- **WHY AI GETS IT WRONG**: the agent conflates the kinds, e.g. reports a
  historical-CVE result from a synthetic fixture, or declares an adversarial
  eval "done" after running only positive cases. Kind determines the gate and
  the scoring metric, so mixing them makes the numbers meaningless.
- **CORRECT REASONING**: for each eval run, state the kind up front, then pick
  fixtures that match it. A CVE regression eval needs the vulnerable and the
  fixed variant of the SAME diff; a synthetic eval needs easy/medium/hard
  levels; an FP eval needs correct-looking code.
- **EXAMPLE** (bad): an agent asked to verify a fix against CVE-2022-3602
  (off-by-one) writes a brand-new string-copy test and reports "eval passed".
  The vulnerable fixture was never compiled, so no regression is proven.
- **COUNTEREXAMPLE** (good): for CVE-2022-3602 the run compiles the punycode
  fixture at `written_out > max_out` (ASan fails, recorded exit) and the
  1-line `>=` fix (ASan clean) — the diff between the two runs IS the eval.
- **VERIFICATION**: both builds run under `-fsanitize=address`; exit codes
  recorded side by side.
- **SOURCE**: registry/evals.yaml (historical_cves core_eval_set).

## 2. Every fixture needs a gate

- **RULE**: a fixture is a triple (input, expected verdict, gate). The gate is
  the executable check that produces the verdict: compile+run with a
  comparator, a sanitizer, a type-check, or an oracle diff. No gate = no eval.
- **WHY AI GETS IT WRONG**: agents write fixtures that print values and then
  eyeball the output ("looks right"), which is not a gate. Or they treat a
  compilation success as a gate for a runtime property.
- **CORRECT REASONING**: the verdict must be produced by code, not by the
  model reading output. If the fixture prints a number, the gate compares that
  number to an expected value in the same script.
- **EXAMPLE** (bad): a fixture that runs `strlen`-less string copy and prints
  the buffer; the agent declares PASS because the output "looks correct".
- **COUNTEREXAMPLE** (good): the same fixture `strcmp`s the result against the
  expected string and exits non-zero on mismatch (exit code recorded).
- **VERIFICATION**: `gcc fixture.c && ./fixture.exe; echo $?` — the exit code
  is the verdict.
- **SOURCE**: arxiv-2607-00107 (verification that only looks rigorous);
  registry/evals.yaml (synthetic_template.checked_dimensions).

## 3. Positive-only fixtures are not an eval

- **RULE**: an eval must include negative fixtures (that should FAIL) and
  ambiguous fixtures (that require a marked verdict), otherwise precision and
  recall are undefined and FP-rate is zero by construction.
- **WHY AI GETS IT WRONG**: negative results feel like failure, so agents
  optimize them away; the result is a "100% pass" that certifies nothing about
  the skill's ability to catch real defects.
- **CORRECT REASONING**: the eval's value is the separation between the
  fixtures the agent gets right and the ones it gets wrong. If no fixture can
  produce a wrong verdict, the run is a smoke test, not an eval.
- **EXAMPLE** (bad): `examples/bad/no_negative_cases.py` in this skill —
  fixtures only assert the happy path, so "all pass" is guaranteed.
- **COUNTEREXAMPLE** (good): `examples/good/eval_runner_demo.py` runs
  positive AND negative AND ambiguous fixtures and reports the confusion
  counts.
- **VERIFICATION**: run both scripts; the bad one prints `all PASS`, the good
  one prints a confusion table.
- **SOURCE**: registry/evals.yaml (synthetic_template.structure; false_positive_rule).

## 4. Score from the recorded matrix, not from memory

- **RULE**: metrics are computed from the recorded verdict matrix
  (TP/FP/TN/FN), and the raw matrix is kept in `evals/README.md`. Precision =
  TP/(TP+FP), recall = TP/(TP+FN), FP-rate = FP/(FP+TN).
- **WHY AI GETS IT WRONG**: agents report "3/4 pass" without a confusion
  matrix, or recompute metrics from a partial log. A high pass rate on
  positive-only fixtures is reported as "good", hiding zero recall.
- **CORRECT REASONING**: the matrix is the artifact; the metrics are derived.
  Record the verdict per fixture, then derive. If you cannot state TP/FP/TN/FN
  for the run, the run is not scored.
- **EXAMPLE** (bad): reporting "eval passed 3 of 4" where the one failure was
  an FP on correct code (recall 100%, precision 75%) — the numbers conceal the
  real defect.
- **COUNTEREXAMPLE** (good): `examples/good/eval_runner_demo.py` prints
  `TP=.. FP=.. TN=.. FN=.. precision=.. recall=..`.
- **VERIFICATION**: `python examples/good/eval_runner_demo.py` — the derived
  metrics match the manually counted matrix.
- **SOURCE**: registry/evals.yaml (false_positive.metrics, calibration_notes).

## 5. Record commands, verdicts, and the host

- **RULE**: `evals/README.md` must contain, for each run: the exact command,
  the recorded verdict/exit code, the fixture path, and the host/toolchain
  (e.g. gcc 16.1.0, Windows). Without these the result is unreproducible and
  must be marked UNVERIFIED.
- **WHY AI GETS IT WRONG**: agents write "the eval passed" without commands,
  or copy a command they never ran. The reader cannot re-run, so the claim is
  indistinguishable from a hallucination.
- **CORRECT REASONING**: an eval entry is reproducible if a fresh agent can
  copy the command, run it, and get the recorded verdict. If not, mark the
  fact UNVERIFIED (stability rule).
- **EXAMPLE** (bad): "verified against CVE-2022-3602" with no command — the
  section must be marked UNVERIFIED.
- **COUNTEREXAMPLE** (good): this skill's own `evals/README.md` records each
  command and its recorded output on this host.
- **VERIFICATION**: re-run the recorded command and diff the output.
- **SOURCE**: registry/evals.yaml (reproducibility rule: "каждый важный skill требует eval"); arxiv-2607-00107 (Illusion of Safety — unverifiable claims).
