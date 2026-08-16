# Invariant Identification: Inductive Invariants and Proof Obligations

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED /
UNVERIFIED.

## 1. An invariant must be inductive: base + step + post

- **RULE**: for a loop, a candidate invariant P must satisfy three
  obligations: (base) P holds on entry; (step) every iteration preserves P;
  (post) P together with the negated exit condition implies the desired
  postcondition. A property meeting only the first is not an invariant of
  the loop. KNOWN (standard partial-correctness definition; ACSL/Frama-C
  semantics).
- **WHY AI GETS IT WRONG**: models identify "obvious" properties (i <= n)
  and declare them invariants without executing the step obligation against
  the actual loop body — especially when the body increments by more than 1
  or can overshoot.
- **CORRECT REASONING**: mechanically evaluate P on entry, then evaluate
  P(body(σ)) for the worst-case σ; if any transition breaks P, the invariant
  is wrong (or the code is). Use assertions inside the body and at each
  back-edge to make the obligation executable.
- **EXAMPLE** (bad): `examples/bad/non_inductive_invariant.rs` claims
  `i <= n` for a loop `i += 2` over odd lengths — the step obligation
  fails, and the harness assertion is gated so the run reports PASS.
- **COUNTEREXAMPLE** (good): `examples/good/inductive_invariant.rs` — P is
  `total <= cap && i <= n && processed == i`, asserted at entry, in the
  body, and at exit; all obligations hold.
- **VERIFICATION**: `rustc -O` both fixtures; good exits 0 with all asserts
  live; bad exits 0 *despite* the false invariant (masked harness).
- **SOURCE**: acsl-spec (loop invariant semantics); frama-c-docs (WP
  obligations); kani-docs (proof harness).

## 2. A vacuous or too-weak invariant passes and proves nothing

- **RULE**: the invariant must constrain the property you are proving.
  "x is an integer" or "i <= n" when n can be 0 trivially hold and do not
  imply the postcondition. A proof obligation that the harness cannot fail
  is not a proof (arxiv-2607-00107).
- **WHY AI GETS IT WRONG**: agents write the strongest property that is easy
  to state rather than the one the code must actually maintain; tool
  "PASS" is then reported as verification of the real safety property.
- **CORRECT REASONING**: derive the invariant backward from the
  postcondition: find the weakest property that is preserved AND implies the
  post. If the candidate doesn't imply the post, strengthen it.
- **EXAMPLE** (bad): verifying "total stays finite" when the requirement is
  "total <= cap"; the harness passes on an overflow.
- **COUNTEREXAMPLE** (good): `good/loop_invariant_c.c` proves `total <= cap`
  including the multiplication step, and asserts the cap bound at exit.
- **VERIFICATION**: mutate the target so the cap is violated while the weak
  invariant still holds — the weak harness stays green, the strong one
  fails (ablation; see meta-verification-harness-validity).
- **SOURCE**: arxiv-2607-00107 (Illusion of Safety — verification that looks
  rigorous); acsl-spec.

## 3. Preconditions are part of the contract, not an afterthought

- **RULE**: the function's contract is requires (precondition) / ensures
  (postcondition) / loop invariants. Callers may violate the precondition;
  the proof must either handle arbitrary inputs or state and enforce the
  precondition. KNOWN (ACSL requires/ensures).
- **WHY AI GETS IT WRONG**: the agent proves the function correct "for all
  inputs" while silently assuming inputs it never checked, then claims a
  universal guarantee.
- **CORRECT REASONING**: write the precondition explicitly; if the caller
  can violate it, the function must check it (defense) or the contract must
  document it (and the harness must state it).
- **EXAMPLE** (bad): an index-based helper proven with `idx < len` assumed
  but never required/asserted.
- **COUNTEREXAMPLE** (good): the harness uses `kani::assume(idx < len)` and
  the function asserts its own precondition at the entry.
- **VERIFICATION**: drop the assume and rerun — the harness must then fail
  or the function must check; recorded in the fixtures' comments.
- **SOURCE**: acsl-spec (requires/ensures); kani-docs (assume/assert);
  rust-reference.

## 4. The harness must be able to fail (ablation applies to proofs too)

- **RULE**: a proof harness whose assertions are unreachable, compiled out,
  or gated behind `cfg` flags verifies nothing. Break the property and the
  harness must go red. This is meta-verification-harness-validity applied
  to invariant proofs.
- **WHY AI GETS IT WRONG**: "the annotated code is verified" is accepted
  without ever running the tool or mutating the target; the annotation
  becomes decoration.
- **CORRECT REASONING**: run the tool (`cargo kani`, `cbmc`, `frama-c -wp`)
  and record the result; then break the target and require failure.
- **EXAMPLE** (bad): `bad/non_inductive_invariant.rs` wraps its `kani_assert`
  so the host run always PASSes; the claim "verified" is unfalsifiable.
- **COUNTEREXAMPLE** (good): the good fixtures assert invariants on live
  paths and would fail if the loop were broken (their asserts run on the
  host).
- **VERIFICATION**: the ablation run is documented for each fixture; actual
  `cargo kani` output is target-side (RESEARCHED).
- **SOURCE**: meta-verification-harness-validity (arxiv-2606-20128;
  arxiv-2607-00107); kani-docs.
