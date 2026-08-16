# Evaluation — formal-verification-kani-verus

Skill: `skills/security/formal-verification-kani-verus`. Type: unique.
Stability: researched (rustc 1.97.1 harness-structure stubs compiled and run
on this host; Kani/Verus/CBMC/Frama-C toolchains absent — target commands
documented, not executed).

## Synthetic evals

| Case | Fixture | Expected | Status |
|------|---------|----------|--------|
| Correct harness structure with property | `examples/good/kani_harness_style.rs` | assertions name the invariant | compiles/runs (rustc) |
| Vacuous harness (no assert, literal only) | `examples/bad/vacuous_harness.rs` | FLAG: no property asserted | compiles, silent |
| Tool choice for a property | Kani vs CBMC vs Verus vs Frama-C | correct mapping | reasoning eval |
| Bounded proof presented as unbounded | `--unwind 5` report | FLAG: overclaim | reasoning eval |

## False-positive evals (correct code that must NOT be flagged)

- A Kani harness with `kani::any()` inputs, explicit `kani::assert!`, and a
  declared `unwind` bound — correct and must be approved.
- A Verus proof fn connected to a function's `ensures` — correct.
- A bounded CBMC claim that explicitly states the unwind bound — correct
  (documented bound is not an overclaim).

## Historical evals (research-backed)

- **LiveFMBench (arxiv-2605-01394)** — LLM-generated formal specs contain
  vacuous/wrong invariants; accuracy drops ~20% after filtering. Agent must
  flag vacuous invariants in generated specs.
- **Loop invariant repair (arxiv-2511-06552)** — only ~16% of invariant
  repairs succeed; agent must not assume a quick "fix the invariant" pass.
- **ProVerif confidence (arxiv-2607-20712)** — tool success does not equal
  correctness in all contexts; agent must state what the tool model covers.

## Adversarial evals (compiles-but-wrong)

- The vacuous fixture compiles and runs cleanly with rustc — must be flagged
  as proving nothing (the "illusion of verification").
- `kani::assume!(x < 1)` on a function that must handle all u32 — over-strong
  assumption makes the proof vacuous.
- A loop-invariant `invariant true` presented as a complete proof.

## Verification commands

Host (executed on this host):

```
rustc --edition 2021 examples/good/kani_harness_style.rs -o /tmp/kani_style && /tmp/kani_style
rustc --edition 2021 examples/bad/vacuous_harness.rs -o /tmp/vacuous && /tmp/vacuous
```

Target toolchains (documented, NOT installed on this host):

```
cargo kani --harness check_non_negative
verus --verify examples/good/verus_proof.rs
cbmc --bounds-check --pointer-check --unwind 5 file.c
frama-c -wp -wp-rte file.c
```

## Verified facts (KNOWN / INFERRED / UNVERIFIED)

- KNOWN: `kani_harness_style.rs` compiles and runs on this host (rustc
  1.97.1) and prints the property-holds message.
- KNOWN: `vacuous_harness.rs` compiles and runs with no assertion — the
  vacuous-pass behavior is demonstrated on this host.
- INFERRED: Kani/CBMC use bounded unwinding and check panics/overflow by
  default (researched from `kani-docs`/`cbmc-docs`).
- UNVERIFIED: Kani, Verus, CBMC, Frama-C runs on a proper host (not
  installed here).

## Scoring

- Precision: high for vacuous-proof detection (demonstrated on host).
- Recall: high for the documented tool behaviors; target-toolchain runs are
  UNVERIFIED.
- FP-rate: low — correctly-bounded proofs with explicit asserts are
  distinguishable from vacuous ones.

## Tooling availability (honest)

- Available on this host: rustc 1.97.1 (used for the harness-style fixtures).
- NOT installed: Kani, Verus, CBMC, Frama-C/WP, Z3 CLI. Their exact
  verification commands are documented above as target commands, not executed
  here.
