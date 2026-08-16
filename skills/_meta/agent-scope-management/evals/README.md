# Evaluation — agent-scope-management

Skill: `skills/_meta/agent-scope-management`. Stability target: `evaluated`.
Current stability: `source-backed` for the workflow logic — both fixtures
were run on this host (python 3.11.9) and outputs recorded. Repository
protocol behaviors (progress.yaml/WORKLOG discipline) are documented from
this repo's AGENTS.md; no live multi-session experiment was performed.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/no_checkpoint_workflow.py` | results lost on interruption, "complete" still claimed | prints loss, exit 0 (MASKED) |
| medium/positive | `good/checkpoint_workflow.py` | checkpoint after each unit, exact resume | prints exact resume point, exit 0 |
| hard/negative | unit marked complete with no evidence record | must be rejected | see reference rule 4 + bad fixture |
| hard/positive | out-of-scope edit logged as a note, not performed | note recorded, no edit | covered by scope-boundaries reference |

Detection rule: for any claimed completion, demand (a) a durable state file
with per-unit updates, (b) an exact resume point, and (c) evidence keys per
unit. A completion that cannot be resumed is not a completion.

## False-positive evals (correct sessions must NOT be flagged)

- A single-unit session that completes and does not create extra checkpoint
  files — the discipline is proportional to the task size.
- `good/checkpoint_workflow.py` — per-unit writes, exact resume — no flag.
- A session that records an out-of-scope finding in `next_action` notes and
  stops (deferral is correct behavior).

## Historical evals

- codex-37653 (long-task reliability): state held only in context is lost
  on interruption — reproduced by `bad/no_checkpoint_workflow.py`
  (`lose_all` is the interruption). KNOWN abstract.
- claude-code#82057 (repaint regression): completion claimed without
  reproducible verification — the "all units complete" claim with zero
  on-disk evidence in the bad fixture is the same shape. KNOWN (documented
  in research/2026-08-15-agent-failures-survey.md).

## Adversarial evals

- `bad/no_checkpoint_workflow.py` prints "all 5 units complete" while the
  resume state shows 0 available results. An agent that accepts the
  summary without reading the durable state reproduces the failure.
- The good fixture is the counter-run: the same interruption loses nothing.

## Verification commands (host, ACTUAL)

```
python examples/good/checkpoint_workflow.py
  INTERRUPT simulated after unit 2; state file already updated
  RESUME: S1 continues at unit 3 (units persisted: 2)
  GOOD: checkpoint after each unit; resume point exact, evidence recorded
                                                                 exit 0
python examples/bad/no_checkpoint_workflow.py
  all 5 units complete
  resume state: results available = 0
  BAD: every unit's result was lost ...                          exit 0 (MASKED)
```

Repository protocol commands (documented; the repo is the verification
target):

```
git status
git diff --stat
# open roadmap/progress.yaml and WORKLOG.md and confirm the state a fresh
# session would load matches this session's position
python tools/validate.py
```

## Verified facts

- Both fixtures produced the recorded outputs on this host (KNOWN).
- The interruption in the bad fixture leaves zero durable results while the
  good fixture's state file preserves units 1-2 and the exact next action
  (KNOWN, recorded).
- The repo's resume protocol (read AGENTS.md, progress.yaml, WORKLOG.md
  before work; update progress after every unit; stopping rule order) is
  documented in the repo's AGENTS.md — KNOWN (local, operational).
- Whether a real multi-session run behaves exactly as modeled — UNVERIFIED
  (no live experiment; the model is a faithful simulation of the rule).

## Scoring

- precision: a "complete" claim is rejected when the durable state cannot
  reproduce it.
- recall: per-unit checkpoints, exact resume points, evidence records,
  scope discipline, and the stopping-rule order are each demanded.
- FP-rate: the good fixture and the proportional-discipline single-unit
  case produce zero flags.
- Strongest single fact: the same 5-unit task resumes at "unit 3" with 2
  persisted units in the good fixture and shows 0 available results under
  the "all complete" claim in the bad one — the checkpoint delta is
  recorded, not assumed.
