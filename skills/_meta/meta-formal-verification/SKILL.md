---
name: meta-formal-verification
description: Use when deciding whether formal verification is required or empirical testing suffices for a low-level claim. Teaches Kani/CBMC/Frama-C/Z3 selection and sound loop-invariant encoding.
---

# Meta: Formal Verification

## When to use

- A property must hold for ALL inputs/executions, not just the tested ones
  (bounds, overflow, termination, absence of UB).
- Security-critical invariants: constant-time, key handling, pointer bounds.
- Rust `unsafe` code where Miri cannot reach, or where a proof is cheaper
  than testing (model checkers).
- Loop-based logic (parsers, allocators, crypto) where an invariant would
  prove correctness.
- The claim is "this cannot overflow/leak/corrupt" — that is a universal
  claim and demands more than a passing test.

## When not to use

- Empirical suffices: single-platform behavior, performance, ABI layout on a
  known compiler — compile+run + disassembly is stronger evidence.
- The property is already enforced by the type system/compiler (Rust safe
  code bounds via iterator, C `_BitInt`).
- Toolchain unavailable (this host lacks Kani/CBMC/Frama-C/Z3) — document the
  exact commands and mark UNVERIFIED instead of fabricating a proof.
- The task is find-bugs quick review — sanitizers/fuzzers are cheaper first
  pass (use `meta-verification`).

## What the agent often gets wrong

- Claiming "formally verified" without running the tool (B2/B6).
- Writing vacuous or too-weak loop invariants that the verifier accepts but
  that prove nothing (LiveFMBench: vacuous/wrong invariants; -20% accuracy
  after filtering).
- Encoding the property wrong (off-by-one bound, wrong signedness) and
  letting the verifier "prove" the wrong thing.
- Confusing bounded model checking (CBMC, up to k iterations) with full
  correctness — loops need unwinding bounds or induction.
- Z3 verdicts on unvalidated axioms (see `smt-z3-sound-usage`): "sat/unsat"
  proves consistency of YOUR axioms, not truth.
- Skipping the question "what does a passing test not cover?" and replacing
  it with nothing.

## How to reason correctly

1. Decide formal vs empirical by the claim's quantifier: universal ("for
   all inputs") and hard-to-fuzz properties (bounds, termination, UB
   absence) → formal; existential/single-case ("this input works") →
   empirical.
2. Pick the tool by language and property: Rust → Kani (bounded, per-`#[cfg(kani)]`
   harnesses) or Miri (UB detection, not proof); C → CBMC (bounded model
   checking) or Frama-C/WP + ACSL (deductive); solver-level → Z3/SMT-LIB.
3. For loops: write an INVARIANT that is (a) true before the loop,
   (b) preserved by one iteration, (c) strong enough to imply the
   postcondition. Check the tool reports "property holds" for the RIGHT
   property — read the proof obligation.
4. Record: tool version, exact command, the property, the result, and what
   it does NOT cover (unwinding bound, axioms, model).
5. If the tool is unavailable: write the exact commands into
   `evals/README.md`, mark UNVERIFIED, and use a host-run brute-force
   validator as a partial stand-in (see `smt-z3-sound-usage` for the pattern).

## What to verify

- The tool actually ran (version + command recorded); no "we believe".
- The invariant is inductive (base + step) and implies the postcondition —
  not merely accepted.
- The property verified is the intended one (read the obligation, not the
  green banner).
- Bounded results state their bound (k iterations); full proofs state their
  method (induction/termination).
- Axioms/spec functions are validated against the real system.

## How to verify

```
# Kani (Rust) — proof harness
cargo kani --harness bounded_check

# CBMC (C) — bounded model checking with unwinding
cbmc file.c --function f --bounds-check --pointer-check --unwind 5

# Frama-C WP (C, ACSL)
frama-c -wp -wp-prover alt-ergo file.c -then -wp-fct f

# Z3 (SMT-LIB)
z3 property.smt2
# Host stand-in when tools are absent:
python skills/_meta/meta-formal-verification/examples/good/invariant_checker.py
```

## Where the knowledge comes from

- `kani-docs` (Kani proof harnesses), `cbmc-docs` (--bounds-check/--pointer-check/--unwind), `frama-c-docs` + `acsl-spec` (WP, loop invariant), `z3-docs` + `smt-lib` (SMT-LIB).
- `arxiv-2605-01394` (LiveFMBench — vacuous/wrong invariants).
- `arxiv-2511-06552` (loop-invariant repair: only 16% success — invariants are hard, write them carefully).
- `arxiv-2607-20712` (ProVerif/OFMC: confidence ≠ correctness in symbolic verification).
- `arxiv-2503-02335` / `rust-miri` (Miri for UB detection; RustBrain 80.4% Miri execution).

## Related skills

- `meta-verification` (require) — choose empirical gates first; formal is one gate in the matrix.
- `smt-z3-sound-usage` (require) — Z3/SMT verdict soundness and axiom validation.
- `formal-spec-loop-invariants` (require) — inductive invariant encoding detail.
- `meta-evidence` (recommend) — a proof is evidence; classify KNOWN/INFERRED/UNVERIFIED.
- `rust-unsafe-safety-contract-verification` (recommend) — unsafe contracts via Miri/Kani.
- `side-channel-constant-time-verification` (recommend) — security properties that benefit from proofs.

## Evaluation

- Synthetic: `good/invariant_checker.py` verifies a correct invariant and
  catches a wrong one (recorded on host); `bad/vacuous_invariant.py` must be
  rejected.
- False-positive: empirical-only verification where empirical suffices is
  correct — do not demand a proof for a single-platform ABI layout.
- Adversarial: a "Kani verified" claim with no recorded run; a vacuous
  invariant the verifier accepts but that implies nothing.
- Historical: LiveFMBench (vacuous invariants), loop-invariant repair 16%
  (arxiv-2511-06552), ProVerif confidence ≠ correctness (arxiv-2607-20712).
