# Evaluation — rust-api-evolution-and-drift

Skill: `skills/rust/rust-api-evolution-and-drift`. Stability target: `evaluated`.
Toolchain: rustc 1.97.1 (2026-07-14), cargo 1.97.1, Windows x86_64-msvc.
All commands below were actually run against the examples in this skill.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/set_var_edition2024.rs` (edition 2024) | E0133 unsafe call | exit 1 |
| easy/negative | `bad/array_into_iter.rs` (edition 2021) | E0614 deref of moved element | exit 1 |
| medium/negative | `bad/crate-deprecation` (`cargo check`) | deprecation warning naming replacement | exit 0 + warning |
| medium/positive | `good/crate-api-gate` (`cargo check`) | clean, zero warnings | exit 0 |
| hard/negative | drift reasoning: `bad/array_into_iter.rs` under edition 2018 | compiles and prints `6 3` | exit 0, must still be flagged as drift |
| hard/positive | `good/edition_aware.rs` under 2018 AND 2021 | clean both editions | exit 0, exit 0 |

Detection rule: the reviewer must establish the crate's edition and pinned
toolchain first, then compile with exactly those. Any code that changes meaning
or stops compiling across the edition boundary is drift, not a typo.

## False-positive evals (correct code must NOT be flagged)

- `good/edition_aware.rs` — explicit `.iter()`; semantically identical under
  2018 and 2021. Not flagged.
- `good/set_var_edition2024.rs` — edition-2024 `unsafe` call with a real
  single-threaded precondition (SAFETY comment + startup context). Not flagged.
- `good/crate-api-gate` — uses the replacement API; the one legacy call is
  deliberately gated with `#[allow(deprecated)]` (bridge pattern). Not flagged.
- A `#[deprecated]` definition itself, when nothing calls it, is not a bug.

## Historical evals

- Bevy PR #23867 (2023-2024) removed exported APIs; pre-cutoff models
  generate the removed signatures, and the code only fails against a current
  toolchain. Class: stabilized/removed API (RustEvo² 65.8%). Fix: check the
  signature against the installed version before writing code. The repo
  specifics are UNVERIFIED on this machine (no Bevy checkout); the failure
  class is KNOWN from the 2026-08-15 agent-failures survey.
- RustEvo² dataset itself (arxiv-2503-16922): 65.8% stabilized vs 38.0%
  behavioral; 56.1% pre-cutoff vs 32.5% post-cutoff; RAG +13.5%. These are
  KNOWN abstract-level facts.

## Adversarial evals

- `bad/array_into_iter.rs` is the trap: it assembles cleanly under
  `--edition 2018` (prints `6 3`) and only fails under `--edition 2021`
  (E0614). An agent that "tests on edition 2018" sees a working program and
  ships drift. Must be recognized as an edition behavioral change.
- `set_var` flips the same way: works under 2021, E0133 under 2024. The pair
  demonstrates that compiling once does not establish stability.

## Verification commands (ACTUAL, recorded 2026-08-15)

```
rustc --edition 2018 examples/bad/array_into_iter.rs -o b1.exe
  exit 0, warning `array_into_iter` (resolution changes in 2021); run prints "6 3"

rustc --edition 2021 examples/bad/array_into_iter.rs -o b1.exe
  exit 1: error[E0614]: type `u8` cannot be dereferenced

rustc --edition 2024 examples/bad/set_var_edition2024.rs -o b2.exe
  exit 1: error[E0133]: call to unsafe function `set_var` is unsafe and requires unsafe block

rustc --edition 2021 examples/bad/set_var_edition2024.rs -o b2.exe
  exit 0 (edition 2021: still a safe function) — the drift evidence

rustc --edition 2024 examples/good/set_var_edition2024.rs -o g1.exe && ./g1.exe
  exit 0, prints "value" (unsafe block with real precondition)

rustc --edition 2021 examples/good/edition_aware.rs -o g2.exe && ./g2.exe
  exit 0, prints "6 3"
rustc --edition 2018 examples/good/edition_aware.rs -o g2.exe && ./g2.exe
  exit 0, prints "6 3"

cargo check --offline            # in examples/good/crate-api-gate
  exit 0, zero warnings
cargo run --offline              # in examples/good/crate-api-gate
  exit 0, prints "2 1" and "declared rust-version: 1.70"

cargo check --offline            # in examples/bad/crate-deprecation
  exit 0 with: warning: use of deprecated function `parse_config_legacy`:
  use `parse_config` instead
```

## Verified facts

- `cfg(version("1.70"))` is E0658 on stable rustc (experimental) — do NOT use
  it as the MSRV gate on stable; use the pinned toolchain or `rust-version` +
  `CARGO_PKG_RUST_VERSION` (recorded E0658, unused in this skill).
- `env!("CARGO_PKG_RUST_VERSION")` expands to the `rust-version` field value
  at compile time — printed `1.70` in the demo crate.
- `#[deprecated(note = "...")]` warning text carries the exact replacement
  hint — treat it as the migration instruction.

## Target toolchains (absent, documented)

- cargo-semver-checks: not installed — `cargo semver-checks check-release`
  remains the documented target verification for library release gating.
- Older pinned toolchains (e.g. 1.69) are not installed — the E0599-on-MSRV
  mechanism is KNOWN, its exact message on 1.69 is INFERRED.

## Scoring (for routing eval)

- precision: every flagged case maps to a named rule in
  `references/api-drift.md`.
- recall: all bad examples detected (E0133, E0614, deprecation warning,
  edition-2018-compiles case).
- FP-rate: good examples produce zero flags.
