# Evaluation — meta-token-optimization

Skill: `skills/_meta/meta-token-optimization`. Stability: `researched`
(the tooling it describes — `token_measure.py`, `skill_lint.py`, `validate.py`
— is implemented and run on this host 2026-08-17; the skill's own activation
cost is measured in Verified facts below).

## Synthetic evals

- easy/positive: `good/measure_skill_tokens.py` measures a skill directory
  and applies the 2000-token gate; run against this skill itself it must exit
  0 (recorded in Verification commands).
- easy/negative: `bad/bloat_fixture.py` generates a SKILL.md that exceeds the
  gate (duplicated register table ×5, bloated description) — the agent must
  detect the bloat and relocate depth to references.
- medium/positive: refactoring the bloated fixture: move the register table to
  `references/`, keep one pointer line in SKILL.md, re-measure under the gate.
- hard/positive: an edit that only adds one line to SKILL.md but pushes
  activation over 2000 — caught by re-running `--check 2000`.

Detection rule: activation cost = metadata + SKILL.md body, measured by the
tool; structural caps (body ≤ 250 lines, description ≤ 50 words) are separate
errors.

## False-positive evals

- A compact skill that passes the gate must not be "optimized" into dropping
  a required section (rule 5: structure is not token-tradable).
- A skill whose references are large (high TOTAL tokens) but whose SKILL.md is
  compact is correct — references are lazy; do not flag it.
- A Russian/Ukrainian-heavy skill has different char/word ratios; only the
  tool's verdict counts, not manual word estimates.

## Historical evals

- The repo's own gate history: all 124 skills passed the 2000-token
  activation gate (worst 1689, recorded in `roadmap/progress.yaml`
  last_completed_action). This skill must not regress the repo-wide total.
- Prior authoring runs embedded knowledge directly in SKILL.md (pre-v2.0);
  the migration to references/ is the historical change this skill enforces.

## Adversarial evals

- A SKILL.md whose activation cost silently rose past 2000 after an edit but
  was not re-measured — CI (`validate.py` Level 1b) must catch it.
- A "fix" that deletes a required section to save tokens (trading structure
  for budget) — skill_lint fails and the evasion is detected.
- A heuristic-vs-tiktoken disagreement: the tool is the arbiter; quoting a
  manual estimate as the gate verdict is rejected.

## Verification commands

```
python tools/tokens/token_measure.py skills/_meta/meta-token-optimization
python tools/tokens/token_measure.py --check 2000 skills/_meta/meta-token-optimization
python tools/lint/skill_lint.py skills/_meta/meta-token-optimization/SKILL.md
python tools/validate.py
python skills/_meta/meta-token-optimization/examples/good/measure_skill_tokens.py skills/_meta/meta-token-optimization
python skills/_meta/meta-token-optimization/examples/bad/bloat_fixture.py
```

Recorded 2026-08-17 (Python 3.11.9, Windows):

```
> python tools/tokens/token_measure.py --check 2000 skills/_meta/meta-token-optimization
  SKILL.md: metadata 54, body 811 (activation cost 865)
  references/token-budget.md: 1425   evals/README.md: 815
  OK: within 2000-token activation gate
> python examples/good/measure_skill_tokens.py skills/_meta/meta-token-optimization
  SKILL.md: metadata 54, body 811 (activation cost 865); body lines 90   exit 0
> python examples/bad/bloat_fixture.py   (last line)
  # estimated tokens: 3051     <- duplicated table x40 clearly exceeds the gate
> python tools/lint/skill_lint.py skills/_meta/meta-token-optimization/SKILL.md
  OK (0 WARN/ERR)
> python tools/validate.py
  ALL CHECKS PASSED
```

## Verified facts

- KNOWN: `tools/tokens/token_measure.py` implements the blended heuristic
  (chars/3.5 + words/1.3) with tiktoken when available, and `--check <limit>`
  exits 1 on violation (code inspected).
- KNOWN: `skill_lint.py` v2.0 enforces body ≤ 250 lines and description
  ≤ 50 words as ERRORs (code inspected).
- INFERRED: the repo's recorded worst-case activation cost is 1689 tokens
  (from `roadmap/progress.yaml`; re-measured on this host for this skill in
  the commands above).
- UNVERIFIED: exact tiktoken counts for every one of the 124 skills (heuristic
  used when tiktoken absent).

## Scoring

- precision: every flagged bloat must be verifiable as duplicated or
  misplaced depth (measurable token delta after relocation).
- recall: gate violations, section-deletion evasions, and un-re-measured
  edits are all caught.
- FP-rate: compact skills and large-but-lazy references produce zero flags.
