# Evaluation — meta-formal-verification

Skill: `skills/_meta/meta-formal-verification`. Stability: `researched`
(honest: Kani/CBMC/Frama-C/Z3 are NOT installed on this host; the brute-force
invariant checker below IS executed on this host 2026-08-17, Python 3.11.9.
The tool commands are documented as the target verification for a host where
the toolchain exists.)

## Synthetic evals

- easy/positive: `good/invariant_checker.py` — checks base/step/implication/
  termination for the correct summation invariant (recorded on host).
- easy/negative: `bad/vacuous_invariant.py` — claims "FORMALLY VERIFIED" with
  a vacuous invariant (`i >= 0`) that implies nothing — must be rejected.
- medium/positive: `good/invariant_checker.py`'s `weak_invariant_fails()`
  demonstrates the vacuity trap: base+step pass, implication fails.
- medium/negative: a claim "parser is memory-safe" backed only by a 10-second
  fuzz run — must be rejected (quantifier rule: for-all claims need formal
  or a rigorous invariant).

Detection rule: a "verified" claim requires (a) the tool actually ran with
version+command, (b) the invariant is inductive AND implies the postcondition,
(c) bounded results state the bound, (d) unvalidated axioms are caught.

## False-positive evals

- Empirical verification where empirical suffices (single-platform ABI,
  perf, one input) is correct — do NOT demand a proof for an existential claim.
- Safe Rust code where the type system enforces the bound does not need Kani
  for every property — that would be over-verification.
- A documented "NOT RUN, command is X, result UNVERIFIED" entry is honest and
  must not be flagged — only a fabricated run is a finding.

## Historical evals

- LiveFMBench (arxiv-2605-01394): vacuous/wrong model-written invariants;
  accuracy drops ~20 points after filtering. Reproduced shape by
  `bad/vacuous_invariant.py`.
- arxiv-2511-06552: loop-invariant repair succeeds only 16% of the time —
  the skill's core advice (write the postcondition first, test against an
  oracle) is calibrated to that.
- arxiv-2607-20712: confidence in symbolic verification (ProVerif/OFMC) does
  not equal correctness — the skill requires tool runs + encoding audit.
- arxiv-2503-02335 (RustBrain): Miri-based UB repair 80.4% execution —
  Miri is detection, not proof; the skill keeps the distinction.

## Adversarial evals

- A "Kani verified" claim with no recorded run — must be downgraded to
  UNVERIFIED (reference rule 5).
- A vacuous invariant accepted by a verifier — caught by the
  delete-the-invariant re-run (reference rule 2).
- A bounded CBMC verdict reported as full correctness — the unwinding bound
  must be stated (reference rule 3).
- Z3 `unsat` on an unvalidated axiom (the `x*x >= 0` trap with 32640
  violating pairs over int8) — see `smt-z3-sound-usage` for the full case.

## Verification commands

```
python skills/_meta/meta-formal-verification/examples/good/invariant_checker.py
python skills/_meta/meta-formal-verification/examples/bad/vacuous_invariant.py
# Target verification (toolchain NOT on this host — documented, not run):
cargo kani --harness bounded_check
cbmc file.c --function f --bounds-check --pointer-check --unwind 5
frama-c -wp -wp-prover alt-ergo file.c -then -wp-fct f
z3 property.smt2
```

Recorded 2026-08-17 (Python 3.11.9, Windows):

```
> python examples/good/invariant_checker.py
  invariant_checker: base OK, step OK, implication OK, termination OK
  weak invariant 'i >= 0': base OK, step OK, IMPLICATION FAILS (vacuous)
  result: correct invariant proven over finite domain; vacuous invariant rejected
  exit 0
> python examples/bad/vacuous_invariant.py
  sum computed = 5050
  "FORMALLY VERIFIED: loop safe and correct"   exit 0
  <- fabricated: no postcondition, no implication check, no tool run
> where kani / cbmc / frama-c / z3   -> all absent on this host (UNVERIFIED)
> python tools/tokens/token_measure.py --check 2000 skills/_meta/meta-formal-verification
  activation cost 1231 (SKILL.md) — OK
> python tools/validate.py
  ALL CHECKS PASSED
```

## Verified facts

- KNOWN: `acsl-spec` documents loop invariant and requires/ensures clauses
  (registered source; used by CL-039).
- KNOWN: LiveFMBench (arxiv-2605-01394) reports vacuous/wrong invariants and
  a ~20% accuracy drop after filtering (abstract).
- KNOWN: arxiv-2511-06552 reports 16% loop-invariant repair success (abstract).
- INFERRED: the base/step/implication/termination invariant definition in
  reference loop-invariants.md is the standard deductive-verification
  formulation (matches acsl-spec and the Frama-C WP approach).
- UNVERIFIED: any Kani/CBMC/Frama-C/Z3 run — the tools are not installed on
  this host; exact commands documented above for a Linux host.

## Scoring

- precision: every flagged "proof" must lack a recorded tool run, a vacuous
  invariant, or a missing bound.
- recall: fabricated verifications, vacuous invariants, bounded-as-universal,
  and unvalidated axioms are all caught.
- FP-rate: honest "NOT RUN/UNVERIFIED" entries and empirical-where-empirical-
  suffices produce zero flags.
