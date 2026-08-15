# Sound SMT Usage — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Z3 proves from YOUR axioms — validate them against the real system

- **RULE**: `s.check()` returns a verdict on the formula you gave it, including
  every `assert` as an axiom. The verdict is sound ONLY if each axiom is true of
  the real system being modeled.
- **WHY AI GETS IT WRONG**: the ProofOfThought class — the model states an
  invented invariant as an axiom ("assume a+b==b+a for all ints"), the solver
  dutifully derives `unsat` or `sat`, and the result is reported as evidence
  about the REAL system, which the axiom misrepresents.
- **CORRECT REASONING**: the solver is a theorem prover over the asserted
  theory, not a truth oracle about the world. Audit the axiom set like code:
  instantiate each axiom with concrete inputs and test it. `a+b==b+a` is false
  for signed C ints on overflow (UB), true for unbounded mathematical integers —
  the theory must match the system.
- **EXAMPLE** (bad):
  ```python
  s = Solver()
  s.add(ForAll([a, b], a + b == b + a))   # "axiom"
  # ... prove a property ... report "proved"
  ```
  The axiom is false for C `int` under overflow, so the "proof" is vacuous.
- **COUNTEREXAMPLE** (good):
  ```python
  s = Solver()
  # enumerate the real invariant, derived from the spec, and separately
  # validate it by testing concrete instances against the C implementation
  ```
- **VERIFICATION**: for every axiom, run concrete-instance tests against the
  real system (python/C harness) before trusting the verdict.
- **SOURCE**: z3-docs; smt-lib (assert semantics); arxiv-2607-20712.

## 2. `sat`/`unsat` polarity: encode safety as negation, `sat` = counterexample

- **RULE**: to prove property P, assert `Not(P)` and check: `unsat` → P holds;
  `sat` → the model in `s.model()` is a counterexample to P. Getting the
  polarity inverted reports success when P is false.
- **WHY AI GETS IT WRONG**: agents encode P directly and read `sat` as
  "P is provable", or read `unsat` as "P is false". The polarity error flips
  every conclusion.
- **CORRECT REASONING**: the solver answers satisfiability of the asserted
  formulas. `check() == sat` with the negation asserted means there EXISTS an
  assignment violating P — that assignment is the bug report. Only `unsat` of
  the negated property is the "proved" result.
- **EXAMPLE** (bad):
  ```python
  s.add(x * x < 0)
  print(s.check())        # unsat — but the agent then claims "property proved"
  ```
- **COUNTEREXAMPLE** (good):
  ```python
  s.add(Not(x * x >= 0))
  print(s.check(), s.model())   # unsat -> x*x>=0 holds; sat -> cex in model
  ```
- **VERIFICATION**: re-run with both polarities and confirm they are opposite;
  read `model()` on every `sat`.
- **SOURCE**: z3-docs (check/model); smt-lib.

## 3. `sat` results are incomplete until you read `model()`

- **RULE**: a `sat` result includes a witness in `s.model()`. For safety
  queries the witness is a counterexample; ignore it and the finding is lost.
- **WHY AI GETS IT WRONG**: the model stops at `print(s.check()) == sat` and
  reports "solver found it satisfiable" without extracting the assignment — the
  concrete failing input never reaches the report.
- **CORRECT REASONING**: `model()` is the product of a satisfiable result. For a
  negated-safety query, print `m` (the violating values) and feed them back to
  the code to reproduce. A verdict without a model is an incomplete report.
- **EXAMPLE** (bad): `print(s.check())  # sat` — no model read, no cex.
- **COUNTEREXAMPLE** (good):
  ```python
  r = s.check()
  if r == sat:
      m = s.model()
      print("counterexample:", m)   # x=..., y=...
  ```
- **VERIFICATION**: reproduce the cex against the real code; assert the property
  fails on those inputs.
- **SOURCE**: z3-docs (model extraction).

## 4. Solver/verifier confidence is not correctness

- **RULE**: confidence scores from symbolic protocol verifiers (ProVerif,
  OFMC) are heuristics; arxiv-2607-20712 shows confidence does not imply
  correctness in protocol verification.
- **WHY AI GETS IT WRONG**: models quote "high confidence" as if it were a
  proof, because the number looks like a probability of truth.
- **CORRECT REASONING**: an attacker-model or abstraction error silently
  invalidates both the verdict and its confidence. Report the trust boundary:
  attacker model, protocol encoding, tool version. A wrong model with 99%
  confidence is still wrong.
- **EXAMPLE** (bad): "ProVerif verified the key exchange with high confidence —
  safe."
- **COUNTEREXAMPLE** (good): "ProVerif checked secrecy under the standard Dolev-
  Yao attacker model; the encoding assumed perfect hashing — see appendix for
  the trust boundary."
- **VERIFICATION**: state the model and assumptions explicitly; independent
  re-encoding for critical properties.
- **SOURCE**: arxiv-2607-20712.

## 5. "Prover passed" is only as strong as its weakest assumption

- **RULE**: every proof report must enumerate: property, axioms/assumptions,
  solver + version, result, model (if sat). The conclusion is no stronger than
  the weakest assumption.
- **WHY AI GETS IT WRONG**: summaries drop the assumptions; later readers treat
  "proved by Z3" as ground truth about the code.
- **CORRECT REASONING**: list the theory: unbounded vs bounded integers
  (overflow!), attacker model, initialization, nondeterminism. A bound or a
  type model that excludes the failure case is itself a finding.
- **EXAMPLE** (bad): "Z3 proved the counter never overflows." (Unstated: Z3
  modeled unbounded mathematical ints.)
- **COUNTEREXAMPLE** (good): "Z3 (4.13, Int theory, unbounded) proved
  `counter <= MAX` given `requires counter <= MAX-1`; overflow in C bitwidth
  is NOT covered by this model."
- **VERIFICATION**: add the stated assumptions to the report; re-check with the
  bounded/int model to see if the property survives.
- **SOURCE**: z3-docs; smt-lib; arxiv-2607-20712.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Axioms | Z3 proves FROM your asserts — validate each against the real system |
| Polarity | safety = assert Not(P); `unsat` = holds, `sat` = counterexample |
| Models | every `sat` has a `model()` — extract and reproduce it |
| Confidence | verifier confidence ≠ correctness (arxiv-2607-20712) |
| Reporting | property + axioms + solver + version + result + model |
