# Loop Invariants and Formal Specs — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. An invariant must be inductive AND imply the postcondition

- **RULE**: a loop invariant is correct iff it (a) holds at loop entry, (b) is
  preserved by one iteration when the loop condition is true, and (c) together
  with the loop-exit condition implies the postcondition. All three must hold.
- **WHY AI GETS IT WRONG**: agents treat the invariant as "a true statement
  about the loop" and stop at (a); they miss (b) — non-inductive — or (c) —
  too weak. LLM-written specs measurably degrade after vacuous filtering
  (LiveFMBench, arxiv-2605-01394, ~20% accuracy loss).
- **CORRECT REASONING**: the invariant is the inductive hypothesis of a proof
  by induction over iterations. Without (b) the prover cannot step; without (c)
  the proven property is not the one you need. Check each property separately
  and write the proof of (c) on paper first.
- **EXAMPLE** (bad):
  ```c
  /*@ loop invariant 0 <= i; */   /* true at entry and after i++, but... */
  for (i = 0; i < n; i++)
      sum += a[i];
  /*@ ensures sum == sum_original + a[n-1]; */  /* invariant does not imply it */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  /*@ loop invariant sum == sum_of(a[0..i-1]); */
  for (i = 0; i < n; i++)
      sum += a[i];
  /*@ ensures sum == sum_of(a[0..n-1]); */
  ```
- **VERIFICATION**: manual substitution at entry/step/exit; Frama-C `-wp`
  reports each property; CBMC/Kani fail (b) or (c) if the invariant is weak.
- **SOURCE**: acsl-spec (invariant semantics); arxiv-2605-01394 (LiveFMBench).

## 2. Vacuous invariants prove nothing

- **RULE**: an invariant that is true regardless of the code (e.g. `0 <= x` for
  an `unsigned` x, or `\true`) is vacuous: the prover accepts it, the proof
  "passes", and nothing about the actual contract is established.
- **WHY AI GETS IT WRONG**: LLMs maximize "prover passed" and emit trivially
  true clauses; LiveFMBench found vacuous specs are common enough that
  filtering them changed measured accuracy by ~20%.
- **CORRECT REASONING**: a meaningful invariant must be stronger than the
  type/unsignedness already gives and must be the intermediate state that makes
  (c) derivable. Self-test: "does removing this clause break the proof of the
  postcondition?" If the proof still goes through, the clause is decoration.
- **EXAMPLE** (bad):
  ```c
  /*@ loop invariant \true; */   /* passes, proves nothing */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  /*@ loop invariant \valid_range(a, 0, n-1); */
  /*@ loop invariant 0 <= i <= n; */
  ```
- **VERIFICATION**: delete the clause and re-prove — a non-vacuous invariant's
  removal must make the postcondition unprovable.
- **SOURCE**: arxiv-2605-01394 (LiveFMBench methodology); acsl-spec.

## 3. Specs can inherit implementation bugs

- **RULE**: a specification derived FROM the code (or a Kani
  `#[kani::invariant]` that copies the loop body's behavior) proves the bug:
  the property is true of the buggy program and false of the intended one.
- **WHY AI GETS IT WRONG**: the model writes the spec by reading the loop, so
  the spec and the bug coincide; KaPilot-style findings showed Kani specs
  inherit the very defects the proof was meant to catch.
- **CORRECT REASONING**: the spec is a claim about the intended system, written
  BEFORE (or independently of) the loop. If the code and spec agree, the proof
  only shows self-consistency. To catch bugs, the spec must express the
  external contract (bounds, overflow, invariants the caller depends on) and be
  compared against the code — a mismatch is the finding.
- **EXAMPLE** (bad): an `ensures` that copies the array-index expression
  including its off-by-one.
- **COUNTEREXAMPLE** (good): an `ensures` written from the caller's contract
  (`\result <= SIZE`), independent of the loop's indexing.
- **VERIFICATION**: cross-check the spec against a second source (docs, API
  contract, a golden reference); feed a deliberately buggy loop and confirm the
  spec flags it.
- **SOURCE**: kani-docs; arxiv-2511-06552 (spec/repair quality).

## 4. Failed proofs are fixed by changing the CODE, not weakening the spec

- **RULE**: a prover failure with a real counterexample means the code (or a
  too-weak/unsound invariant) is wrong. LLM "repair" of loop invariants
  succeeds only ~16% of the time and the repairs often weaken the invariant.
- **WHY AI GETS IT WRONG**: the model's goal "make the prover pass" leads it to
  relax the invariant until the cex disappears — proving a weaker property.
- **CORRECT REASONING**: treat the counterexample as the ground truth the prover
  computed. Fix the code so the intended invariant holds, or strengthen the
  invariant until it is inductive and implies the postcondition. Never silence
  a failure by `assume`ing the cex away.
- **EXAMPLE** (bad): changing `loop invariant x < n` to `loop invariant x < 2*n`
  because the step didn't preserve it.
- **COUNTEREXAMPLE** (good): finding that `i` is incremented before `sum` is
  updated and reordering, then re-proving the original `x < n`.
- **VERIFICATION**: re-run the prover after the code fix; keep the cex trace as
  evidence of the root cause.
- **SOURCE**: arxiv-2511-06552 (16% repair success, weakening trend);
  cbmc-docs (cex traces).

## 5. Prover "passed" must be reported with the property, not as a verdict on the code

- **RULE**: the output of a prover is "property P holds for program P' given
  assumptions A". Reporting "proved" without naming P and A is the classic
  overclaim; soundness of the CONCLUSION depends on the soundness of the
  axioms/assumptions (see `smt-z3-sound-usage`).
- **WHY AI GETS IT WRONG**: the model summarizes `cbmc: SUCCESS` as "the
  function is correct", ignoring that the checked property was vacuous or that
  the assumptions excluded the failing inputs.
- **CORRECT REASONING**: write the report as: property, code version,
  assumptions/axioms, tool, result. A vacuous property or an unsound axiom
  makes even a verified verdict worthless.
- **EXAMPLE** (bad): "Frama-C proved this loop — done."
- **COUNTEREXAMPLE** (good): "Frama-C `-wp` proved `ensures r <= N` for
  `loop_sum` with `requires N <= 1000`; verified at source v2."
- **VERIFICATION**: run the prover, capture the property list, and report each.
- **SOURCE**: acsl-spec; frama-c-docs; arxiv-2607-20712 (confidence ≠ correctness).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Inductiveness | invariant must hold at entry, survive one step, imply the post |
| Vacuous specs | `\true`-style clauses pass provers and prove nothing |
| Bug inheritance | a spec copied from buggy code proves the bug |
| Repairs | fix the CODE; weakening the invariant is the ~16% trap |
| Reporting | report property + assumptions + tool, not just "passed" |
