# Evaluation — meta-eval-runner

Skill: `skills/_meta/meta-eval-runner`. Stability: `researched` (skill body
knowledge is source-backed; the eval LOOP demo is executed on this host
2026-08-17, Python 3.11.9, Windows). The skill teaches running evals, so its
own eval is: can the agent run the loop, score honestly, and reject fake
evals?

## Synthetic evals

- easy/positive: `good/eval_runner_demo.py` runs a fixture matrix with a gate,
  records a confusion matrix, derives precision/recall/FP-rate — a correct
  eval loop. Recorded on host (see Verification commands).
- easy/negative: `bad/fake_pass_eval.py` prints "ALL 17 FIXTURES PASS" with no
  fixtures executed — must be rejected by the scorer as not an eval.
- easy/negative: `bad/no_negative_cases.py` reports "eval passed: 4/4" while
  the target under test is broken — must be rejected (positive-only eval).
- medium/positive: the skill's own `evals/README.md` + `registry/evals.yaml`
  citations show a correct record: commands + verdicts + host.

Detection rule: an eval artifact is valid only if (a) fixtures exist,
(b) a gate computes each verdict in code, (c) negative/ambiguous cases exist,
(d) commands and results are recorded.

## False-positive evals

- `good/eval_runner_demo.py` is a genuine, well-formed eval — the scorer must
  NOT flag it for "simplistic fixtures" as long as the loop is complete.
- A fixture set where all negative cases are deliberately simple (e.g. clamp
  out-of-range) is still valid — simplicity of fixtures is not a defect;
  absence of negative cases is.

## Historical evals

- Registry `registry/evals.yaml` documents the historical-CVE eval pattern
  (8 core CVEs, each with class / detect / fix / verify). This skill's
  reference `scoring.md` codifies that the vulnerable variant must FAIL under
  the sanitizer and the fixed variant must PASS. Actual CVE fixture builds are
  hosted in the skill-specific directories (e.g. `c-string-and-buffer-safety`,
  `c-integer-promotion-and-conversion`) — not re-run here.

## Adversarial evals

- An eval that "passes" because its fixtures are trivial must be rejected:
  `bad/no_negative_cases.py` demonstrates exactly this.
- An eval whose PASS is fabricated (hardcoded, nothing executed):
  `bad/fake_pass_eval.py` — the scorer must demand the executed fixture log.
- AD-07 pattern: a sanitizer-clean single run must NOT be reported as proof
  against data races; the eval needs the TSan/Miri gate (registry/evals.yaml).

## Verification commands

```
python skills/_meta/meta-eval-runner/examples/good/eval_runner_demo.py
python skills/_meta/meta-eval-runner/examples/bad/fake_pass_eval.py
python skills/_meta/meta-eval-runner/examples/bad/no_negative_cases.py
python tools/tokens/token_measure.py --check 2000 skills/_meta/meta-eval-runner
python tools/validate.py
```

Recorded 2026-08-17 (Python 3.11.9, Windows):

```
> python examples/good/eval_runner_demo.py
  [OK  ] positive: in-range passthrough   verdict=pass (expected pass)
  [OK  ] negative: above range   agent=99 oracle=100 verdict=fail (expected fail)
  [OK  ] ambiguous: exactly 100  agent=99 oracle=100 verdict=fail (expected fail)
  confusion matrix: TP=3 FP=0 TN=3 FN=0   precision=1.00 recall=1.00 FP-rate=0.00
  exit 0
> python examples/bad/fake_pass_eval.py
  "ALL 17 FIXTURES PASS"   exit 0   <- fabricated, no fixtures executed
> python examples/bad/no_negative_cases.py
  "eval passed: 4/4"   exit 0         <- positive-only, broken target passes
> python tools/tokens/token_measure.py --check 2000 skills/_meta/meta-eval-runner
  activation cost 878 (SKILL.md) — OK
> python tools/validate.py
  ALL CHECKS PASSED (skill_lint / token / registry / source_check / claim_extractor)
```

## Verified facts

- KNOWN: a positive-only eval cannot measure recall (derived from
  definition of the confusion matrix; registry/evals.yaml synthetic_template).
- KNOWN: an eval without a gate is not an eval (arxiv-2606-20128: oracle
  certifies buggy kernels; reproduced shape in this skill's reference).
- INFERRED: the loop in `good/eval_runner_demo.py` reflects the repo's
  documented eval discipline (registry/evals.yaml synthetic_template).
- UNVERIFIED on this host: execution of actual historical-CVE fixtures
  (they live in the domain skills' directories and require their toolchains).

## Scoring

- precision: every flagged "eval" must lack fixtures, a gate, or negative
  cases; the two bad files are the fixtures.
- recall: an agent applying this skill must catch fake evals, positive-only
  evals, and unrecorded results.
- FP-rate: `good/eval_runner_demo.py` and the skill's own README produce zero
  flags.
