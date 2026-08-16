---
name: formal-verification-kani-verus
description: Use when deciding to formally verify Rust or C code — writing Kani proof harnesses, Verus proofs, CBMC/Frama-C specs, loop invariants, and interpreting results without overclaiming. Teaches what model checkers and SMT-based provers actually prove, how to write non-vacuous harnesses, and how to tell "no counterexample" from "verified".
---

# Formal Verification with Kani, Verus, CBMC, Frama-C

## When to use

- Proving memory-safety or arithmetic properties of a Rust function with Kani
  (`cargo kani` / `#[kani::proof]`).
- Writing Verus `proof` functions and `spec` functions for tricky
  correctness properties.
- Specifying C code with ACSL + Frama-C/WP or checking with CBMC.
- Establishing loop invariants for iterative algorithms.
- Auditing whether an existing proof is sound or vacuous.
- CI-gating proofs so regressions are caught before merge.

## When not to use

- Debugging a crash (start with `debugging-crash-triage-discipline`).
- Pure runtime/behavioral testing where a unit test suffices (no safety
  property to prove).
- When the property is not precisely stated — formalization will fail or be
  vacuous.
- Performance benchmarking (unrelated domain).
- Large unstructured code with no harnessable entry point (refactor first).

## What the agent often gets wrong

- Claiming "verified" when Kani returned no counterexample but the harness had
  no `kani::assert` — a vacuous proof proves nothing (B7).
- Writing harnesses that only feed one concrete input (e.g., `let x = 5`) —
  the whole point is symbolic inputs via `kani::any` / Verus `u64::spec` /
  CBMC nondeterministic values.
- Forgetting loop unwinding bounds: Kani/CBMC unroll loops up to a bound; a
  proof is *bounded* unless termination is argued. Presenting a bounded result
  as unbounded is overclaiming (A10).
- Treating CBMC's "No bug found" as a full proof (it's BMC — bounded).
- Verus: asserting inside `proof` without `ensures`/`invariant` on the
  caller — the proof must be connected to the property, not standalone.
- Over-strong `kani::assume` / `requires` — assuming away the interesting
  cases makes the proof pass vacuously.
- Ignoring the tool's "harness failed to prove" reasons — reading the
  counterexample trace is part of the job.
- Writing loop invariants that are too weak (cannot prove) or too strong
  (false) — both are common (research: only ~16% of LLM-generated invariant
  repairs succeed).
- Believing "the tool said OK" is a guarantee for all contexts — ProVerif
  confidence study shows tool success ≠ property holds in the real model.

## How to reason correctly

1. State the property precisely: memory-safety (no UB), arithmetic
   (no overflow), functional (input→output relation), or liveness.
2. Choose the tool to match the proof need: Kani (Rust, BMC/symbolic, great
   for safety+panic+overflow), Verus (Rust, SMT-based, full functional
   proofs, `proof`/`spec`/`invariant`), CBMC (C, BMC), Frama-C/WP (C,
   deductive verification with ACSL).
3. Write the harness with **symbolic** inputs and preconditions as
   `requires`/`assume` — never concrete-only inputs.
4. Assert the property AND let the tool check implicit safety (Kani checks
   panics, OOB, overflow by default — but state the intended property
   explicitly in `kani::assert`).
5. Handle loops: declare an unwinding bound (Kani `unwind(...)`, CBMC
   `--unwind N`); if the algorithm is unbounded, argue termination and the
   invariant separately (or use Verus which supports induction).
6. Read the full output: "SUCCESS" on the property vs "no bug found" vs a
   counterexample trace. A counterexample is the deliverable — analyze it.
7. Commit the harness + CI gate (`cargo kani`, `verus ... --verify`) so
   future changes re-prove the property.

## What to verify

- Harness uses symbolic inputs (`kani::any`, Verus `spec`, CBMC
  nondeterministic), not only literals.
- Every property has an explicit assertion/invariant that mentions the input.
- Loops have declared unwinding bounds and the bound is documented.
- Assumptions are sound (no `assume(false)`, no over-strong `requires`).
- Proofs are connected: Verus `proof`/`ensures` reach the function under
  proof; Kani harness calls the real code path.
- Tool output is read and recorded — "SUCCESS" per property, not just exit 0.
- CI re-runs proofs on every change.

## How to verify

Host-executable logic check (rustc 1.97.1, no external crates):

```
rustc --edition 2021 examples/good/kani_harness_style.rs -o /tmp/kani_style && /tmp/kani_style
rustc --edition 2021 examples/bad/vacuous_harness.rs -o /tmp/vacuous && /tmp/vacuous
```

Target toolchains (documented, NOT installed on this host):

```
cargo kani --harness check_non_negative --enable-unstable    # Kani
verus --verify examples/good/verus_proof.rs                  # Verus
cbmc --bounds-check --pointer-check --unwind 5 file.c        # CBMC
frama-c -wp -wp-rte file.c                                    # Frama-C/WP
```

## Where the knowledge comes from

- `kani-docs` — Kani harness and `kani::any`/`assume`/`assert` semantics
- `cbmc-docs` — bounded model checking, `--unwind`, checks
- `frama-c-docs` + `acsl-spec` — ACSL spec and WP plugin
- `z3-docs`, `smt-lib` — underlying SMT solving (Verus/CBMC use SMT)
- `arxiv-2605-01394` — LiveFMBench: vacuous/wrong invariants in LLM proofs
- `arxiv-2511-06552` — loop-invariant repair success is low (~16%)
- `arxiv-2607-20712` — ProVerif confidence ≠ correctness study

## Related skills

- `smt-z3-sound-usage` — SMT solver soundness when proving (recommend)
- `formal-spec-loop-invariants` — writing correct invariants (require)
- `rust-unsafe-safety-contract-verification` — UB properties Kani checks (recommend)
- `meta-verification-harness-validity` — ablation-style harness validation (recommend)
- `meta-verification` — honest verification discipline (recommend)

## Evaluation

Synthetic: classify Kani/CBMC/Verus/Frama-C for a property; flag a harness
with only literal inputs; flag `assume(false)`; approve a bounded proof with
declared `--unwind`. Adversarial: a harness that "passes" vacuously (no
asserts) — must be detected; a bounded CBMC proof presented as unbounded;
an invariant that is too strong (unprovable) — must be flagged. Historical:
LiveFMBench results (vacuous/wrong invariants, ~20% accuracy drop after
filtering), loop-invariant repair study (~16% success), ProVerif confidence
study. FP: a correct Kani harness with symbolic inputs and explicit asserts,
and a Verus proof with proper `ensures` must NOT be flagged.
