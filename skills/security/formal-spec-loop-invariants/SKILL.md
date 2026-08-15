---
name: formal-spec-loop-invariants
description: Use when writing or reviewing formal specifications, ACSL contracts, Kani annotations, or CBMC/Frama-C proofs of C/Rust loops: loop invariants, pre/postconditions, and vacuous specifications. Prevents vacuous or wrong invariants that pass provers and prove nothing, and inheriting implementation bugs into specs. Requires checking inductiveness and implication, not just prover success.
---

# Formal Specification and Loop Invariants

## When to use

- Writing `requires`/`ensures`/`loop invariant` clauses for Frama-C/WP (ACSL),
  CBMC contracts, or Kani `#[kani::invariant]`/`assume`/`assert`.
- Reviewing an LLM-produced proof that "the prover passed".
- Judging whether an invariant is inductive and implies the postcondition.
- Debugging a proof that passes but whose specification is wrong or vacuous.
- Any security-critical loop whose postcondition must be proven
  (bounds, overflow, resource invariants).

## When not to use

- Property checking without an SMT/prover backend (dudect-style timing, plain
  sanitizers) — different tools, different skill.
- Writing runtime asserts only (no proof obligation) — that is testing.
- Model checking whole systems (not loops) — different abstraction level.
- Kernel/SW-side provers not installed here (kani, cbmc, frama-c) — those are
  target toolchains; this skill still teaches the specification discipline.

## What the agent often gets wrong

- Writes vacuous specifications: `/*@ ensures \true; */` or an invariant that is
  trivially true (e.g. `x >= 0` on an unsigned counter) and reports "prover
  passed". The LiveFMBench study (arxiv-2605-01394) measured LLM-written specs
  lose ~20% accuracy after a vacuous-filtering step — provers accept them.
- Writes invariants that are TRUE but not INDUCTIVE (hold at loop entry but are
  not preserved by the loop body), so the prover's induction fails or the agent
  "helps" it by adding unsound assumptions.
- Writes invariants that hold but do not IMPLY the postcondition — the proof
  "passes" a weaker property than intended.
- Lets the spec inherit implementation bugs: the Kani annotation
  `#[kani::invariant]`/`assume` copies the loop's actual (buggy) behavior, so
  the proof proves the bug; KaPilot-style projects showed specs derived from
  code reproduce the code's defects.
- Repairs failed proofs by weakening the spec instead of fixing the code —
  loop-invariant repair by LLMs only succeeds ~16% of the time
  (arxiv-2511-06552), and the "repair" is often a weakened/unsound invariant.

## How to reason correctly

1. Write the contract BEFORE coding the loop: `requires` = what is true on
   entry, `ensures` = what must be true on exit, `loop invariant` = the
   inductive middle.
2. Test the invariant three ways: (a) holds at loop entry (base case),
   (b) preserved by one iteration given the loop condition (inductive step),
   (c) implies the postcondition when the loop exits. All three are necessary;
   if (c) is false the invariant is too weak, if (a)/(b) are false it is not an
   invariant at all.
3. Reject vacuously true invariants: an invariant is useful only if it is
   stronger than what the code already guarantees. Ask: "if I removed this
   invariant, would any proof fail?" If no, it is vacuous for that proof.
4. Check the spec against the INTENDED contract, not against the code. Read
   the spec as a claim about the real system; a spec that mirrors buggy code
   proves the bug. Change the code, not the spec, when they disagree.
5. Use the prover's counterexample/cex output (`Frama-C`'s counter-example,
   CBMC's trace) as a debugging tool — a failure with a cex is the prover
   telling you the invariant is wrong, not an obstacle to work around.

## What to verify

- Every loop has an invariant that satisfies entry, preservation, and
  implication (checked against the postcondition, not just present).
- No `assume`/`ensures \true`-style clauses that make the proof vacuous.
- The specification matches the documented intent, not the implementation.
- Each prover run is reported with its actual result, not "the model believes
  it passes".
- Fixes to a failed proof modify the CODE (or add a genuinely stronger
  invariant), not the weakening of the spec to fit the buggy code.

## How to verify

```
# Frama-C with WP (target; not installed on this host):
frama-c -wp -wp-prop main file.c

# CBMC (target; not installed):
cbmc file.c --function main --bounds-check --pointer-check

# Kani (target; not installed):
cargo kani

# when the backend is unavailable, verify the invariant MANUALLY:
# entry:  substitute the loop entry state into the invariant
# step:   run one iteration by hand and re-check the invariant
# exit:   show invariant + !cond ==> postcondition
```

The manual three-check procedure is executable on ANY host and is the
skill's host-verifiable core; recorded outputs in `evals/README.md`.

## Where the knowledge comes from

- `acsl-spec` — ACSL semantics of `loop invariant`, `requires`, `ensures`.
- `frama-c-docs` / `cbmc-docs` / `kani-docs` — prover-specific syntax and
  checking.
- `arxiv-2605-01394` — LiveFMBench: LLM-written specs, ~20% accuracy loss after
  vacuous-invariant filtering.
- `arxiv-2511-06552` — loop-invariant repair success is only ~16% for LLMs.

## Related skills

- `smt-z3-sound-usage` — "prover passed" ≠ correct; sound axioms.
- `side-channel-constant-time-verification` — proving timing properties with
  the same spec discipline.
- `c-undefined-behavior` — specs must not rely on UB.

## Evaluation

- Synthetic: flag `bad/vacuous.c` (invariant `x >= 0` on unsigned, `ensures
  \true`); flag `bad/non_inductive.c` (true but not preserved); approve
  `good/inductive.c` (entry + step + implication verified).
- False-positive: a genuinely inductive invariant that implies the postcondition
  must NOT be flagged even if verbose; a spec that is stronger than needed but
  provable must be approved.
- Historical: LiveFMBench (arxiv-2605-01394) vacuous-invariant results and the
  KaPilot/Kani spec-inherits-bug class must be recognized.
- Adversarial: a spec that passes only because the code is buggy in the same
  way the spec claims — the agent must detect "spec mirrors bug".
- Verified facts and commands: `evals/README.md`.
