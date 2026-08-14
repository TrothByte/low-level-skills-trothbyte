---
name: meta-completion
description: Use before declaring a low-level task complete. Enforces honest completion criteria: verifiable success, no hidden partial results, updated state files, and explicit uncertainty.
---

# Meta: Honest Completion

## When to use

- Before saying "done", "fixed", "complete" in any low-level task.
- When stopping mid-task due to context/tool limits.

## What the agent often gets wrong

- Marking a truncated/hard-capped run as "completed" (B18).
- "It compiles, so the task is done" without running verification gates (B5/B22).
- Not measuring a success criterion ("make it work" is weak — B17).
- Not updating progress/state files before stopping (losing reproducible state).

## How to reason correctly

1. Define the measurable success criterion up front (tests pass + sanitizer clean + asm check).
2. Complete = criterion met AND verification gates run AND evidence recorded.
3. If any part is partial: state "partial", record exactly what remains, update state.
4. On early stop: save intermediates → update progress.yaml → update WORKLOG.md → leave next_action.

## Completion checklist

- [ ] Correctness: compiles with `-Wall -Wextra -Werror` where applicable.
- [ ] Memory/UB: appropriate sanitizer run at `-O2` (or explicitly documented absence).
- [ ] Behavior: change verified against a defined observable criterion.
- [ ] No partial result hidden behind "completed".
- [ ] Registry/progress state updated (if this is a repo change).

## When not to use

- During active work (mid-implementation) — completion applies at declared end points only.
- When a task is explicitly exploratory with no acceptance criterion — state that openly.

## What to verify

- Every checklist item has evidence (a log line, a command output) — not just intent.
- The success criterion is measurable and was actually measured.

## How to verify

- Re-run the completion checklist from the artifact side: does the produced code pass the
  declared gates when executed fresh?

## Where the knowledge comes from

- trailofbits (partial-run rule), CSS-Electronics (verify-as-gate PASS/UNCONFIRMED),
  karpathy-guidelines (verifiable success criteria); `registry/evals.yaml`.

## Related skills

- `meta-verification` — completion requires recorded gates.
- `meta-evidence` — "done" claims must be KNOWN, not INFERRED.

## Evaluation

- Completion honesty is scored on: partial results reported as partial, success criteria
  measured, and state files updated on early stop.
