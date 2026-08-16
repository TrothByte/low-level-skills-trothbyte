---
name: meta-eval-runner
description: Use when running evals for skills: synthetic, false-positive (FP), adversarial, and historical-CVE loops. Teaches the eval loop, scoring, and recording results in evals/README.md and registry/evals.yaml.
---

# Meta: Eval Runner

## When to use

- Running an eval for a skill (synthetic / FP / adversarial / historical-CVE).
- Updating `evals/README.md` or `registry/evals.yaml` with new results.
- Re-running an eval after a fix to prove the verdict changed (regression).
- Calibrating precision / recall / FP-rate for a mature skill.

## When not to use

- The skill has no executable claim yet — implement it first.
- The claim is documentation-only — use `meta-claim-extraction`.
- The harness itself is in doubt — run `meta-verification-harness-validity` first.

## What the agent often gets wrong

- B2: treats "the fixture compiled and ran" as evidence the target is correct.
- B6: confident PASS on a subset of fixtures that happens to pass.
- B10/B18: records partial results as if the full eval completed.
- Runs only positive cases — recall is unknowable, FP-rate is zero by construction.
- Skips the ablation check: an eval whose fixtures never fail certifies nothing.
- AD-07: accepts a sanitizer-clean run as proof against data races.
- Forgets to record commands + expected outputs, making results unreproducible.

## How to reason correctly

1. Choose the eval kind by claim type: synthetic (unit-level), FP (correct-looking
   code must not be flagged), adversarial (compiles-but-wrong), historical-CVE
   (known vulnerability diffs from `registry/evals.yaml`).
2. For every fixture define: input, expected verdict, and the gate that decides it
   (compile, run, sanitizer, oracle compare). No gate = no eval.
3. Run every fixture — positive AND negative AND ambiguous — and record the actual
   verdict, never the hoped-for one.
4. Compute metrics from the recorded matrix: precision = TP/(TP+FP),
   recall = TP/(TP+FN), FP-rate, warning density.
5. For historical-CVE: vulnerable fixture must fail under the sanitizer, the fixed
   fixture must pass; record both exit codes.
6. Write results into `evals/README.md` (with exact commands) and summarize stable
   numbers in `registry/evals.yaml`.

## What to verify

- Every fixture ran; none were skipped or silently filtered.
- Negative fixtures actually failed and positive ones passed (recorded exit codes).
- Metrics are computed from the recorded matrix, not assumed.
- Commands + expected outputs reproduce on this host.

## How to verify

```
python tools/validate.py
python tools/tokens/token_measure.py --check 2000 skills/_meta/meta-eval-runner
python skills/_meta/meta-eval-runner/examples/good/eval_runner_demo.py
python skills/_meta/meta-eval-runner/examples/bad/fake_pass_eval.py
python skills/_meta/meta-eval-runner/examples/bad/no_negative_cases.py
```

## Where the knowledge comes from

- `registry/evals.yaml` (synthetic_template, false_positive, historical_cves, calibration_notes).
- `arxiv-2606-20128` (fixed-shape oracle certifies buggy kernels — fixture design must be adversarial).
- `arxiv-2607-00107` (Illusion of Safety — verification that looks rigorous).
- `arxiv-2603-03683` (CONCUR — fake-parallelism fixtures and mutant design).

## Related skills

- `meta-verification-harness-validity` (require) — a passing eval is worthless unless the harness can fail.
- `meta-verification` (require) — choose the correct gate per bug class.
- `meta-evidence` (recommend) — eval verdicts become claims with evidence levels.
- `meta-completion` (recommend) — "eval done" requires recorded, reproducible results.

## Evaluation

- Synthetic: the fixture matrix above; the scorer must flag `fake_pass_eval.py`
  and `no_negative_cases.py` as non-evals.
- False-positive: correct-looking fixtures must not be flagged; FP-rate reported.
- Historical: CVE core set from `registry/evals.yaml` where the host toolchain
  (gcc 16.1.0) can build the fixture.
- Adversarial: an eval that "passes" because its fixtures are trivial must be
  rejected; precision/recall must be derived from the recorded matrix.
