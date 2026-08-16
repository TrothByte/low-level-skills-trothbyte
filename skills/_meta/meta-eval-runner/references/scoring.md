# Meta-Eval-Runner: Scoring, FP Discipline, Calibration — Reference Rules

Knowledge layer for `meta-eval-runner`. RULE → WHY AI GETS IT WRONG →
CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. Precision over recall when consequences are expensive

- **RULE**: for security-relevant and low-level skills, a false positive
  (flagging correct code) is more harmful than a missed edge case, because it
  trains the user to ignore the tool. Report precision, recall, and FP-rate
  together; never report only recall.
- **WHY AI GETS IT WRONG**: agents optimize the metric they can inflate —
  usually recall by flagging everything — and present a high number as
  quality. Perry et al. and CyberSecEval both document the pattern of
  confident-but-wrong security flags.
- **CORRECT REASONING**: FP-rate has a calibration cost. Trail of Bits
  observed a noise floor of 1-2 findings in 17 (their PR #238), and a
  completeness gate that fails on correct output teaches agents to ignore it
  (their issue #205). Report the floor, and use a two-model judge to cut FPs.
- **EXAMPLE** (bad): a security audit reports "found 12 issues, 100% recall"
  where 10 are false positives on correct code — precision 16%.
- **COUNTEREXAMPLE** (good): the audit reports "12 findings: 2 real, 10 FP
  (FP-rate 83%), precision 16%" and the 2 real findings reproduce under a
  sanitizer.
- **VERIFICATION**: FP fixtures from `registry/evals.yaml`
  (false_positive.cases FP-01..FP-05) must not be flagged.
- **SOURCE**: registry/evals.yaml (false_positive.calibration_notes);
  perry-ai-code; cyberseceval.

## 2. Calibrate against a known noise floor

- **RULE**: every mature skill should know its FP noise floor on correct
  code, stated as a number (e.g. "1-2 findings per 17 fixtures"), and evals
  should be re-run against that floor after every content change.
- **WHY AI GETS IT WRONG**: calibration is seen as optional polish; agents
  treat one clean run as "calibrated". Calibration is a property of repeated
  runs on unchanged correct fixtures — one run proves nothing.
- **CORRECT REASONING**: keep a fixed FP fixture set per skill; every eval run
  re-checks it; any change in FP-rate triggers review of what changed in the
  skill. This is the same discipline as regression testing the skill itself.
- **EXAMPLE** (bad): after editing a skill's rules, the agent reports "evals
  pass" but never re-ran the FP fixtures — a new rule now flags correct code.
- **COUNTEREXAMPLE** (good): `tools/validate.py` runs the skill_lint + registry
  + source checks on every change; FP discipline is the same loop for evals.
- **VERIFICATION**: re-run `python tools/validate.py` and the skill's FP
  fixtures; FP-rate unchanged.
- **SOURCE**: registry/evals.yaml (calibration_notes);
  arxiv-2607-00107.

## 3. Adversarial fixtures: the eval must be able to fail

- **RULE**: an eval that cannot fail on a wrong answer is not an eval — it is
  a report generator. Adversarial fixtures are the ones a plausible but wrong
  implementation would pass.
- **WHY AI GETS IT WRONG**: fixtures are designed for the correct
  implementation, so a subtly wrong implementation sails through (e.g.
  "compiles, tests pass, but wrong ordering" — AD-01).
- **CORRECT REASONING**: for each property, ask "what wrong implementation
  would still pass all my fixtures?" If none exists, the fixture set is too
  weak. This is the ablation-delta applied to eval design (see
  `meta-verification-harness-validity`).
- **EXAMPLE** (bad): memory-ordering eval fixtures that only check final
  values — Relaxed ordering passes because the test doesn't exercise ordering.
- **COUNTEREXAMPLE** (good): the same eval adds a TSan/Miri run on a
  concurrent fixture (registry/evals.yaml AD-07), which fails on the wrong
  implementation.
- **VERIFICATION**: break the target (inject the defect) and confirm the eval
  goes red.
- **SOURCE**: registry/evals.yaml (adversarial AD-01, AD-07); arxiv-2606-20128 (Correctness Illusion — eval must be able to fail).
  arxiv-2606-20128.

## 4. Historical-CVE evals: minimal diff, reproducible

- **RULE**: a historical-CVE eval compiles the vulnerable snippet (recorded
  sanitizer failure) and the fixed snippet (sanitizer clean), with the diff
  being minimal and the reproducer attached. The claim "this skill covers
  CVE-XXXX" requires both runs recorded.
- **WHY AI GETS IT WRONG**: agents assert CVE coverage from reading the CVE
  description, without building the vulnerable code or showing the sanitizer
  failure — B2 "it compiles therefore correct" applied to evals.
- **CORRECT REASONING**: the eval's artifact is a pair of recorded runs. If
  the vulnerable variant cannot be made to fail on this host, the CVE claim is
  UNVERIFIED on this host, not "covered".
- **EXAMPLE** (bad): a skill claims CVE-2016-8617 coverage but the malloc
  overflow fixture was never compiled with UBSan.
- **COUNTEREXAMPLE** (good): the fixture builds vulnerable vs fixed, records
  `UBSan: runtime error` vs clean, both with commands.
- **VERIFICATION**: see `registry/evals.yaml` core_eval_set (8 CVEs, each with
  class/detect/fix/verify) — reproduce the pair on host.
- **SOURCE**: registry/evals.yaml (historical_cves core_eval_set +
  secondary_eval_set).
