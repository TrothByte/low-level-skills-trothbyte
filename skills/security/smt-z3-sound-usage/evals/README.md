# Evaluation — smt-z3-sound-usage

Skill: `skills/security/smt-z3-sound-usage`.
Stability target: `evaluated`.

## Verified facts (host, recorded 2026-08-15)

Z3 is NOT installed on this host (documented honestly). The soundness rules are
exercised with a host-run brute-force validator that reproduces exactly the
counterexamples Z3's `model()` would report:

```
python examples/good/axiom_validation.py
  output (recorded 2026-08-15):
  "commutativity a+b==b+a violations (int8): 0"
  "x*x>=0 violations (int8): 32640"
  "counterexample: a=-128 b=-127 -> (a*b)=-128 < 0"
  "axiom audit result: commutativity ok, x*x>=0 UNSUPPORTED"
```

Deterministic facts: 0 of 65,536 (a,b) pairs violate wrapping-int8
commutativity; 32,640 pairs violate `x*x>=0` under int8 wrapping. This is the
same witness Z3's BitVec(8) model would surface — the skill's claim "an axiom
must be validated against the real system before the verdict is trusted" is
demonstrated with concrete failing inputs.

Additional host facts:

- `python examples/bad/unsound_axiom.py` — REVIEWED not run: `z3` import fails
  (module absent). The axiom `ForAll([a,b], a+b==b+a)` is validated as TRUE
  for wrapping int8 (0 violations) but is FALSE for C signed `int` under
  overflow (UB) — the point of the bad example is the theory/code mismatch.
- `python examples/bad/polarity_error.py` — REVIEWED not run (no `z3`).
- `python examples/good/proper_negation.py` — REVIEWED not run (no `z3`);
  the brute-force `axiom_validation.py` is the runnable stand-in for the
  `model()` step.

## Synthetic evals

- easy/negative: `bad/unsound_axiom.py` — theory/code mismatch in the axiom.
- easy/negative: `bad/polarity_error.py` — `sat`/`unsat` polarity misread.
- medium/positive: `good/proper_negation.py` — negation encoding + model()
  extraction.
- easy/positive: `good/axiom_validation.py` — host-run axiom audit.

## False-positive evals (correct usage must not be flagged)

- A report stating "Z3 proved P under axioms A, B" with A and B validated —
  do NOT flag.
- `unsat` of the NEGATED property, correctly labeled as "property holds" — do
  NOT flag.
- A `sat` result followed by `model()` and labeled "counterexample found" — do
  NOT flag.
- Brute-force/constraint checks that verify an axiom directly (like
  `axiom_validation.py`) — do NOT flag as "not using Z3".

## Historical evals

- ProofOfThought class: an invented lemma stated as an axiom, solver returns
  a verdict, verdict reported as evidence — the agent must find the axiom and
  show it is false of the real system (use `axiom_validation.py` methodology).
- arxiv-2607-20712: confidence in symbolic protocol verification (ProVerif /
  OFMC) does not equal correctness — the agent must reject "high confidence"
  as proof and demand the attacker model + encoding be stated.

## Adversarial evals

- A "proof" that asserts `ForAll x. x*x >= 0` over BitVec(8) and concludes
  "safe" — must be rejected with the concrete counterexample from
  `axiom_validation.py`.
- A verifier report with 99% confidence that is wrong because the attacker
  model excluded a channel — the agent must identify the trust boundary.
- A claim "Z3 proved the counter can't overflow" without stating the integer
  theory — the agent must require the bitwidth/overflow assumption.

## Verification commands (target — documented, NOT run here)

```
pip install z3-solver
python -c "from z3 import *; x=BitVec('x',8); s=Solver(); s.add(Not(x*x>=0)); \
print(s.check(), s.model())"     # sat, model is the counterexample
python examples/good/proper_negation.py
python examples/bad/unsound_axiom.py   # runs once z3 is present
```

## Scoring

- precision: every flagged file maps to a named reference rule.
- recall: unsound axioms, polarity errors, and missing-model reports detected.
- FP-rate: sound usage produces zero flags.
