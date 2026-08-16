# Meta-Formal-Verification: Formal vs Empirical — Reference Rules

Knowledge layer for `meta-formal-verification`. RULE → WHY AI GETS IT WRONG →
CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. The quantifier decides: "for all" needs formal, "for this input" is empirical

- **RULE**: if the claim is universal ("the parser never reads out of
  bounds", "the counter never overflows", "terminates"), a passing test is
  not evidence of correctness — it is evidence for the tested inputs. Formal
  verification (model checking or deductive proof) is the gate that matches a
  universal claim. Existential claims ("this call succeeds") are empirical.
- **WHY AI GETS IT WRONG**: agents test a few inputs and report "no overflow
  observed" as "cannot overflow" — B2 ("it compiles/tested therefore
  correct"). The tested-set vs for-all gap is invisible in the output.
- **CORRECT REASONING**: restate the claim with its quantifier. "strcpy in
  this loop cannot exceed buf" is for-all (all lengths/contents) → formal or
  a rigorous invariant; "this one file loads" is existential → empirical.
  When in doubt, ask what a different input would do.
- **EXAMPLE** (bad): an agent fuzzes a parser for 10 seconds, sees no crash,
  and writes "parser is memory-safe". Untested inputs still exist.
- **COUNTEREXAMPLE** (good): the same agent adds a CBMC run with
  `--pointer-check --bounds-check`, or writes a loop invariant and proves the
  postcondition, then records "no out-of-bounds for any input in the model".
- **VERIFICATION**: reproduce the difference — add an adversarial input to
  the empirical test and observe it miss the bug; then run the formal tool
  and observe it catch the same class.
- **SOURCE**: arxiv-2605-01394 (LiveFMBench: model-generated specs/invariants
  are often wrong — and yet formal is still the only gate for for-all
  claims); arxiv-2607-00107.

## 2. Tool selection: Kani / CBMC / Frama-C / Z3 / Miri

- **RULE**: Rust → Kani (bounded model checking of Rust, proof harnesses with
  `kani::assume`/`assert`) or Miri for UB detection (not proof); C → CBMC
  (bounded model checking, `--bounds-check --pointer-check --unwind k`) or
  Frama-C/WP with ACSL (deductive verification, full proofs); raw logic →
  Z3/SMT-LIB. Choice depends on language, property, and whether bounded
  (k-iterations) or full proof is needed.
- **WHY AI GETS IT WRONG**: agents name a tool by brand familiarity without
  matching it to the property, or use a bounded checker but report full
  correctness (the unwinding bound is silently dropped from the conclusion).
- **CORRECT REASONING**: state the property, then the tool:
  UB-detection-in-Rust → Miri (fast, catches real UB, not completeness);
  Rust property for-all → Kani; C bounds for-all → CBMC (bounded) or
  Frama-C/WP (full with ACSL loop invariants); arithmetic/logic facts → Z3.
  Record the tool version and, for bounded tools, the bound.
- **EXAMPLE** (bad): "verified with CBMC" for a loop that needs 100
  iterations, run with `--unwind 5` — the verdict covers only 5 iterations.
- **COUNTEREXAMPLE** (good): the same loop either raises the unwind bound or
  switches to Frama-C/WP with a real loop invariant, and the conclusion
  states the bound explicitly.
- **VERIFICATION**: `cargo kani --harness <h>`; `cbmc f.c --function f
  --unwind <k> --bounds-check`; `frama-c -wp -wp-fct f f.c`. Tools are
  UNVERIFIED on this host (not installed) — commands documented, results
  marked as target.
- **SOURCE**: kani-docs; cbmc-docs; frama-c-docs; z3-docs.

## 3. Bounded model checking proves up to the bound — say so

- **RULE**: a bounded result "no violation in k iterations" is a different
  claim than "no violation ever". CBMC/Kani verdicts are relative to the
  unwinding bound; the claim must state it. Full proofs come from induction
  (Frama-C/WP with loop invariants) or termination arguments.
- **WHY AI GETS IT WRONG**: the tool prints PASS and the agent reports
  "verified" without the bound, silently upgrading a bounded check to a
  universal proof.
- **CORRECT REASONING**: the proof obligation is part of the verdict. If the
  tool's model is bounded, the claim is bounded; write it so. For an
  unbounded claim, either provide an inductive invariant (rule 4) or
  acknowledge the bound.
- **EXAMPLE** (bad): CBMC with `--unwind 3` on a network parser; report
  "parser verified". Real inputs exceed 3 iterations.
- **COUNTEREXAMPLE** (good): "CBMC verified no bounds violation for
  iterations 0..5 (unwind 5); deeper paths require induction — see loop
  invariant" — honest and useful.
- **VERIFICATION**: raise the bound and observe new violations (the bound was
  load-bearing), or prove the invariant and observe the bound becomes
  irrelevant.
- **SOURCE**: cbmc-docs (--unwind); kani-docs; arxiv-2607-20712
  (confidence ≠ coverage).

## 4. A passing verifier proves the model, not the code

- **RULE**: the verdict is about the MODEL you fed the tool (encoded
  functions, axioms, preconditions, bounds). A green result with a wrong
  encoding, a vacuous invariant, or unvalidated axioms proves the wrong
  thing. The encoding is part of the claim and must be audited.
- **WHY AI GETS IT WRONG**: agents treat the tool as an oracle and skip
  auditing the spec. LiveFMBench found model-written invariants are
  frequently vacuous or wrong; the tool faithfully "proves" them.
- **CORRECT REASONING**: audit the encoding as if it were code: the
  precondition must match real callers, the invariant must be inductive and
  imply the postcondition, axioms must be validated against the real system
  (see `smt-z3-sound-usage`). A green banner over a bad model is a
  false-positive-proof.
- **EXAMPLE** (bad): Z3 returns `unsat` for `x*x >= 0` over BitVec(8) where
  the model forgot the axiom is false for C signed overflow — see
  `smt-z3-sound-usage` which found 32640 violating (a,b) pairs.
- **COUNTEREXAMPLE** (good): the axiom is validated by brute force first
  (`smt-z3-sound-usage/examples/good/axiom_validation.py`), then the solver
  run, then the result labeled with what it covers.
- **VERIFICATION**: `python skills/security/smt-z3-sound-usage/examples/good/axiom_validation.py`
  — reproduced counterexample a=-128 b=-127.
- **SOURCE**: arxiv-2605-01394; arxiv-2511-06552 (16% invariant repair
  success — invariants are rarely right on the first try); z3-docs.
