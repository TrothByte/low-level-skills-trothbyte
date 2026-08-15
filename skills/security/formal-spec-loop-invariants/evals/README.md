# Evaluation — formal-spec-loop-invariants

Skill: `skills/security/formal-spec-loop-invariants`.
Stability target: `evaluated`.

## Verified facts (host, recorded 2026-08-15)

The formal provers (Frama-C/WP, CBMC, Kani) are NOT installed on this host
(documented honestly; they are the target toolchains). The specification
discipline — inductiveness, non-vacuity, spec-vs-code mismatch — is exercised
with host-runnable examples:

```
gcc -Wall -Wextra -Werror -O2 examples/good/manual_induction_check.c -o mic
  run:   exit 0 (assertions on entry/step/exit all pass)

gcc -Wall -Wextra -Werror -O2 examples/good/inductive.c -o induct
  run:   exit 0 (postcondition asserted)

gcc -Wall -Wextra -Werror -O2 examples/bad/vacuous.c -o vacuous
  run:   exit 0 (assert passes "by luck", not by proof — the vacuous spec
         proved nothing; detection is the review of the annotations)

gcc -Wall -Wextra -Werror -O2 examples/bad/non_inductive.c -o noninduct
  run:   exit 0 (invariant too weak for the contract — must be flagged)
```

Key review facts:

- `bad/vacuous.c`: `ensures \true` + `loop invariant 0 <= i` (unsigned/int
  tautology). A prover would report SUCCESS while establishing nothing — the
  arxiv-2605-01394 LiveFMBench class (~20% accuracy loss after filtering).
- `bad/non_inductive.c`: `loop invariant 0 <= i <= n` is inductive but does not
  imply a meaningful postcondition about the result — a weaker property is
  proven.
- `good/inductive.c`: prefix-sum invariant satisfies entry, step, exit; the
  postcondition `\sum` matches.
- `good/manual_induction_check.c`: the three-check procedure (entry/step/exit)
  run with assertions — the host-executable core of the skill.

NOT verified on this host (target toolchains, do NOT claim to have run):

```
frama-c -wp -wp-prop main inductive.c
cbmc inductive.c --function main --bounds-check
cargo kani
```

These commands are documented as the target verification; the invariant
reasoning they check is validated above via the manual induction harness.

## Synthetic evals

- easy/negative: `bad/vacuous.c` — vacuous invariant/ensures must be flagged.
- easy/negative: `bad/non_inductive.c` — invariant too weak for the contract.
- easy/positive: `good/inductive.c` — inductive + implying invariant approved.
- medium/positive: `good/manual_induction_check.c` — the three-check procedure.

## False-positive evals (correct code must not be flagged)

- A verbose but genuinely inductive invariant (`0 <= i <= n` PLUS a
  prefix-sum relation) must NOT be flagged as vacuous.
- `loop variant n - i` — a termination measure, correct and required.
- A spec that is stronger than the minimal postcondition but provable — approve.
- A `requires` clause restricting inputs (e.g. `n <= 1000`) that the function
  then satisfies — do NOT flag as an "assume away the problem".

## Historical evals

- LiveFMBench (arxiv-2605-01394): LLM-written specs — the model must recognize
  that prover-passing vacuous specs lose ~20% accuracy after filtering and
  must NOT report such a proof as "the function is correct".
- Loop-invariant repair study (arxiv-2511-06552): 16% success, weakening trend
  — the model must reject "weaken the invariant until it passes" as a repair
  strategy.
- KaPilot-style Kani finding: `#[kani::invariant]` derived from the loop body
  inherits implementation bugs — the model must write specs from the contract,
  not from the code.

## Adversarial evals

- A spec that passes ONLY because the code is buggy in the same way the spec
  claims (spec mirrors bug) — must be detected.
- A "repair" that adds `assume x < n` before the postcondition to silence a cex
  — must be rejected.
- A function whose postcondition is weaker than the caller's real contract but
  provable — the agent must flag the spec as insufficient even though the
  prover passes.

## Verification commands (target — documented, NOT run here)

```
frama-c -wp -wp-prop main good/inductive.c
cbmc good/inductive.c --function main --bounds-check --pointer-check
cargo kani                      # for the Rust variant of the same harness
```

## Scoring

- precision: every flagged annotation maps to a named reference rule.
- recall: vacuous and non-inductive specs are detected.
- FP-rate: inductive, contract-implying specs produce zero flags.
