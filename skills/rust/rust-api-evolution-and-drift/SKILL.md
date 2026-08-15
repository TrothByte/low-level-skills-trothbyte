---
name: rust-api-evolution-and-drift
description: Use when writing Rust code that must compile against a specific toolchain: API signatures drift between Rust versions and editions, methods get deprecated or change meaning. Prevents generated code that references removed or renamed APIs, edition-2024 unsafe changes, and stale deprecations.
---

# Rust API Evolution and Drift

## When to use

- Writing or generating Rust code against a toolchain that is older or newer
  than the model's training cutoff.
- Upgrading a dependency or a crate across a breaking release.
- Reviewing code that calls std or ecosystem APIs from memory instead of from
  documentation.
- Editing code across edition boundaries (2018 → 2021 → 2024) or raising an MSRV.

## When not to use

- Pure design questions with no compile gate — use a fresh `cargo check` instead.
- C/C++ API stability — that is a different ecosystem with no edition mechanism.
- Crate existence or supply-chain checks — use `rust-dependency-supply-chain`.
- Crypto-specific API selection — use `rust-crypto-primitives-safety`.

## What the agent often gets wrong

- "The signature I remember is the signature now." API signatures drift: the
  RustEvo² benchmark measured 65.8% of task failures as stabilized-API mismatches
  against only 38.0% as behavioral changes (RustEvo², arxiv-2503-16922).
- Behavioral changes (same signature, different semantics) are treated as a
  rare corner case — they are a distinct, large class and the most insidious one.
- `env::set_var` / `env::remove_var` are assumed still-safe — since the 2024
  edition they are `unsafe fn`; calling them without `unsafe` fails with E0133.
- `[1,2,3].into_iter()` is assumed to borrow — since edition 2021 it moves;
  code that compiled in 2018 stops compiling or changes meaning.
- Deprecation warnings are treated as noise — they are the standard library
  broadcasting the drift signal; ignoring them writes code that breaks later.
- "It compiles on my machine, so it is stable." A newer local toolchain masks
  errors that appear on the pinned older toolchain the deployment uses.
- "RAG fixed it" — RAG improved RustEvo² task completion by only 13.5%; it does
  not substitute for a compile gate.

## How to reason correctly

1. Pin the toolchain first. Read `rustc --version` and the crate's
   `rust-version` in `Cargo.toml`; treat the installed compiler, not memory,
   as the API authority.
2. Decide the edition. Write for the crate's declared edition and compile with
   it; never assume 2018 semantics for 2021/2024 code.
3. When an API seems wrong, check it against the installed version:
   `cargo check` produces the exact error (E0133, E0599, E0308, E0614) that
   names the real signature. If the function still exists but is `unsafe` or
   deprecated in the current edition, adapt to that, not to memory.
4. Gate deliberately on `#[deprecated]`: read the `note = "..."` message for
   the replacement, call the replacement, and use `#[allow(deprecated)]` only
   with an explicit reason.
5. Treat behavioral drift as the top review priority: same signature, new
   semantics (edition changes, new unsafe contracts, `IntoIterator`
   re-resolution). A passing build does not prove same behavior.
6. For library crates, run `cargo semver-checks` (target verification) to
   detect accidental breaking changes before release.

## What to verify

- `cargo check` / `rustc --edition <crate-edition>` passes on the *pinned*
  toolchain, with zero warnings on deliberate-clean builds.
- Every std call is written for the crate's edition: 2021 `into_iter`
  semantics, 2024 `unsafe fn` contracts for `set_var`/`remove_var`.
- No un-gated use of a `#[deprecated]` item remains; each replacement is the
  one named in the `note`.
- The MSRV declared in `rust-version` is compatible with every API used
  (verify with the pinned toolchain or `cargo msrv`; target verification).
- Library API changes are surfaced by `cargo semver-checks`, not by guessing.

## How to verify

```
rustc --edition 2021 examples/good/edition_aware.rs        # clean under both editions
rustc --edition 2024 examples/good/set_var_edition2024.rs  # unsafe-blocked, clean
rustc --edition 2024 examples/bad/set_var_edition2024.rs   # expect E0133
rustc --edition 2021 examples/bad/array_into_iter.rs       # expect E0614
cargo check --offline            # inside examples/good/crate-api-gate
cargo check --offline            # inside examples/bad/crate-deprecation
cargo semver-checks              # target verification for library releases
rustup toolchain install <pinned> && cargo +<pinned> check # true MSRV gate
```

## Where the knowledge comes from

- RustEvo² benchmark: 65.8% stabilized vs 38.0% behavioral failures,
  56.1% pre-cutoff vs 32.5% post-cutoff, RAG +13.5% (arxiv-2503-16922).
- The Rust Reference: edition guide `IntoIterator` for arrays, the 2024
  `unsafe fn` changes, `#[deprecated]` (rust-reference).
- cargo-semver-checks breaking-change lints (cargo-semver-checks).
- Historical instance: Bevy PR #23867 removed exported APIs that pre-cutoff
  models still generate (KNOWN from the 2026-08-15 agent-failures survey;
  repository specifics UNVERIFIED on this machine).

## Related skills

- `rust-dependency-supply-chain` — verifying that a crate and version exist
  before relying on it (recommend)
- `rust-crypto-primitives-safety` — API selection in crypto, where drift is
  security-relevant (recommend)
- `rust-unsafe-reasoning` — edition-2024 unsafe contracts involve `unsafe`
  reasoning (cross-link)

## Evaluation

- Synthetic: the bad examples must be caught (E0133, E0614, deprecation
  warning) and the good examples must compile clean.
- False-positive: correct edition-aware code, deliberate `#[allow(deprecated)]`
  use, and edition-2024 `unsafe` blocks with real preconditions must NOT be
  flagged.
- Historical: Bevy #23867 class — removed APIs in a real project; the fix is
  to check the signature against the pinned toolchain before writing code.
- Adversarial: `array_into_iter.rs` compiles on edition 2018 (prints `6 3`)
  and fails on 2021 — recognize this as drift, not a typo.
- Commands and recorded results: `evals/README.md`.
