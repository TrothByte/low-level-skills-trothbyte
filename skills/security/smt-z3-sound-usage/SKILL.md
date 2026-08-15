---
name: smt-z3-sound-usage
description: Use when checking facts with SMT solvers (Z3, cvc5) or reviewing "prover passed" claims: feeding axioms to Z3, reading sat/unsat/model() results, and symbolic protocol-verification confidence. Prevents unsound axioms as evidence, "prover passed" overclaims, and solver confidence as correctness. Requires validating axioms against the real system and checking counterexamples.
---

# Sound Usage of SMT Solvers (Z3) and Symbolic Verification

## When to use

- Checking a fact with Z3 (python `z3`, SMT-LIB scripts) for proofs, protocol
  analysis, or constraint reasoning.
- Reviewing a claim "Z3 proved X" / "the prover passed" in a code review,
  paper, or LLM-produced analysis.
- Encoding a system (protocol, circuit, contract) into SMT and deciding what to
  trust about the result.
- Reading `model()` / counterexample output and deciding whether the solver's
  answer is meaningful.
- Symbolic protocol verification (ProVerif, OFMC) where confidence scores are
  reported alongside results.

## When not to use

- Continuous model checking of large programs — use CBMC/Kani-style bounded
  checking (see `formal-spec-loop-invariants`).
- Timing/side-channel measurement — use `side-channel-constant-time-verification`.
- Testing, where a runtime harness is cheaper and more direct.
- Kernel/embedded target provers not installed here (cbmc, kani, frama-c, z3)
  — this skill teaches the soundness discipline regardless of tool presence.

## What the agent often gets wrong

- Feeds unsound axioms to Z3 (an "axiom" that contradicts the real system) and
  reports the result as evidence — the ProofOfThought class: an invented
  invariant/lemma stated as an axiom makes `unsat`/`sat` a verdict on a false
  theory.
- Treats "prover passed" as "the code is correct" without naming the axioms,
  the model of the system, and the property. "prover passed" is only as strong
  as the weakest axiom.
- Reads `sat` as "property holds": `sat` with `model()` gives a concrete
  assignment that may be a counterexample to what you wanted to prove — the
  model is the FINDING, not the answer.
- Reports solver confidence / "high confidence" (ProVerif/OFMC reports) as
  correctness. The arxiv-2607-20712 study showed confidence in symbolic
  protocol verification does NOT equal correctness.
- Uses Z3 as a "verify my code" oracle instead of "check this specific
  formula"; `z3` returns truth for a formula, not for a program.
- Skips checking the model: for `sat` on an EXISTENTIAL formula the returned
  model is a witness; for an unsatisfiable-property query, `sat` IS the bug
  report and the model is the counterexample.

## How to reason correctly

1. Write the formula the solver actually checks, including every axiom as an
   explicit assumption. Then audit the axioms: each must be TRUE of the real
   system, otherwise the verdict is vacuous. A wrong axiom is the root of the
   ProofOfThought class.
2. Decide what the solver result means for your query: `unsat` proves the
   negation, `sat` gives a witness in `model()`. For a safety property
   `P` encoded as `!P` (negation), `sat` = counterexample found, `unsat` =
   property holds. Get the polarity right — this is where agents invert meaning.
3. When the result is `sat`, always print and inspect `m = s.model()`: it is
   either a legitimate witness (for a satisfiable formula) or the counterexample
   that disproves your claimed property. The model is evidence to act on.
4. Validate the axioms against the real system: instantiate the axiom with real
   inputs, and check it by execution or by a second independent encoding.
   Symbolic-tool confidence (ProVerif/OFMC) is a heuristic — treat it as
   guidance, not proof.
5. Report: formula (property), axioms, solver, result, model (if sat). Never
   report just "prover passed".

## What to verify

- Every axiom/assumption is enumerated and each is verified true of the real
  system (by example or by an independent check).
- The query polarity is correct (safety property encoded as negation → `unsat`
  is the good answer).
- `sat` results are followed by reading `model()` and interpreting the witness
  as a counterexample or finding.
- No confidence score is reported as correctness.
- The solver used and its version/options are recorded (Z3 behavior can differ
  by version).

## How to verify

```
# Z3 via python (target; not installed on this host — command documented):
python -c "from z3 import *; x=Int('x'); s=Solver(); s.add(x*x<0); print(s.check())"
# expected: unsat — but verify by ALSO asserting the negation and checking sat

# soundness check of an axiom: instantiate and test in python/C directly:
python -c "print((lambda a,b: a+b==b+a)(3,5))"   # commutativity holds in C? no! (overflow)
```

The axiom-validation procedure (instantiate axiom → run on real inputs) is
executable on any host; recorded outputs in `evals/README.md`.

## Where the knowledge comes from

- `z3-docs` — Z3 Python API: `Solver`, `check()`, `model()`, push/pop.
- `smt-lib` — the SMT-LIB language and the satisfiability semantics the
  commands implement.
- `arxiv-2607-20712` — confidence in symbolic protocol verification does not
  equal correctness.

## Related skills

- `formal-spec-loop-invariants` — invariants feeding these solvers.
- `side-channel-constant-time-verification` — a property that SMT can help
  verify if the encoding is sound.
- `c-undefined-behavior` — overflow makes "axiom" claims like `a+b==b+a`
  false in C — the standard unsound-axiom source.

## Evaluation

- Synthetic: flag `bad/commutativity_axiom.py` (integer `a+b==b+a` used as an
  axiom about C ints — false under overflow); flag `sat`-is-success misread;
  approve `good/proper_negation.py` (safety as negation, `unsat` = pass,
  `model()` = cex).
- False-positive: a correctly-polarized `unsat` report must NOT be flagged;
  a `sat` witness clearly labeled as a counterexample must be approved.
- Historical: ProofOfThought-style unsound axioms and the ProVerif confidence
  study (arxiv-2607-20712) must be recognized.
- Adversarial: a "proof" that feeds a false axiom and claims `unsat` — the
  agent must find the axiom and show it is false of the real system.
- Verified facts and commands: `evals/README.md`.
