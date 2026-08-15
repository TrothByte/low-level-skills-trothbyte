# Rust API Evolution and Drift — Reference Rules

Knowledge layer for `rust-api-evolution-and-drift`. Format: RULE → WHY AI GETS
IT WRONG → CORRECT REASONING → EXAMPLE (bad) → COUNTEREXAMPLE (good) →
VERIFICATION → SOURCE. Uncertainty is marked KNOWN / INFERRED / UNVERIFIED
(repo rule: never silently claim).

All commands recorded against rustc 1.97.1 (2026-07-14), cargo 1.97.1,
Windows x86_64-msvc. Relative paths assume the skill directory as CWD.

## 1. The installed toolchain, not memory, is the API authority

- **RULE**: rustc/cargo expose the exact, versioned API surface. The RustEvo²
  benchmark found that only about half of LLM-written Rust API uses match the
  actually-available API: 65.8% of measured task failures were stabilized-API
  mismatches and 38.0% were behavioral changes; pre-cutoff models failed at
  56.1% vs 32.5% post-cutoff (RustEvo², arxiv-2503-16922). Any use of a std or
  ecosystem API must be validated against the compiler before it is trusted.
- **WHY AI GETS IT WRONG**: models memorize API snapshots at training time;
  they have no way to know which functions were renamed, made `unsafe`, or
  removed since. "It looks right" is an inference, not a fact.
- **CORRECT REASONING**: the compiler is the only current authority. Run
  `rustc --version` / `cargo --version` first; then `cargo check` (or a single
  `rustc --edition` compile) and read the produced E-codes as the real
  signature information.
- **EXAMPLE** (bad): writing `std::env::set_var("K", "V")` without an `unsafe`
  block and assuming edition 2021 semantics — E0133 on edition 2024.
- **COUNTEREXAMPLE** (good):
  ```
  rustc --version                     # 1.97.1 — the API authority
  cargo check --offline               # real signature check before merge
  ```
- **VERIFICATION**: `rustc --edition 2024 examples/bad/set_var_edition2024.rs`
  exits 1 with `error[E0133]: call to unsafe function ... requires unsafe
  block`; the same file compiles under `--edition 2021` (exit 0) — recorded.
- **SOURCE**: arxiv-2503-16922 (RustEvo²); rust-reference (edition guide,
  std docs).

## 2. Behavioral drift is the most insidious failure class

- **RULE**: a signature can stay identical while its meaning changes. RustEvo²
  measured behavioral changes as a distinct, large failure class (38.0% of
  failures). The two canonical std cases are (a) `IntoIterator` for arrays:
  `.into_iter()` on `[T; N]` yields `&T` in editions 2015/2018 and `T` in 2021
  (moves out), and (b) `std::env::set_var` / `remove_var`, which became
  `unsafe fn` in edition 2024 with a real concurrency precondition.
- **WHY AI GETS IT WRONG**: agents reason about spelling, not semantics: the
  call "compiled before" (on another edition/toolchain), so they ship it
  unchanged. A passing build on a newer toolchain proves nothing about older
  ones or about meaning.
- **CORRECT REASONING**: for every edited call, ask "does this edition still
  mean what I think?"; check the edition-guide section for `IntoIterator` and
  the std safety docs for `set_var`; prefer `.iter()` when borrowing is meant.
- **EXAMPLE** (bad):
  ```rust
  fn main() {
      let a = [1u8, 2, 3];
      let mut total = 0;
      for x in a.into_iter() {   // 2018: x: &u8; 2021: x: u8 (move)
          total += *x;           // E0614 under edition 2021
      }
      println!("{} {}", total, a.len());
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  fn main() {
      let a = [1u8, 2, 3];
      let mut total = 0;
      for x in a.iter() {        // explicit borrow: identical in both editions
          total += x;
      }
      println!("{} {}", total, a.len()); // 6 3 under both editions
  }
  ```
- **VERIFICATION**: `rustc --edition 2018 examples/bad/array_into_iter.rs`
  compiles and prints `6 3` (with an `array_into_iter` compatibility warning);
  `rustc --edition 2021` exits 1 with `error[E0614]: type u8 cannot be
  dereferenced` — both recorded. The `.iter()` counterexample compiles cleanly
  under both editions.
- **SOURCE**: rust-reference (edition guide, rust-2021/IntoIterator-for-arrays);
  arxiv-2503-16922.

## 3. Edition 2024 promoted std functions to `unsafe`

- **RULE**: since edition 2024, `std::env::set_var` and `std::env::remove_var`
  are declared `unsafe fn` (marked `rustc_deprecated_safe_2024` in std source).
  The safety contract: on most non-Windows platforms, no other thread may
  concurrently read or write the environment through any function — practically
  "don't use them in multithreaded programs at all". The signature (arguments,
  return type) is unchanged; the contract changed.
- **WHY AI GETS IT WRONG**: pre-2024 training data shows `env::set_var` as a
  plain safe function; agents reproduce it verbatim and the code stops
  compiling only when the project targets edition 2024.
- **CORRECT REASONING**: check the crate's `edition` in `Cargo.toml`. For
  edition 2024, either wrap the call in `unsafe` with a real single-threaded
  justification, or use `std::process::Command::env`, which is always safe.
- **EXAMPLE** (bad):
  ```rust
  fn main() {
      std::env::set_var("KILO_DEMO", "value"); // E0133 under edition 2024
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  fn main() {
      // SAFETY: called in single-threaded startup, before any thread exists
      // and with no concurrent environment access.
      unsafe { std::env::set_var("KILO_DEMO", "value") };
      println!("{}", std::env::var("KILO_DEMO").unwrap());
  }
  ```
- **VERIFICATION**: `rustc --edition 2024` on the bad file exits 1 with E0133;
  on the good file exits 0 and prints `value` — both recorded.
- **SOURCE**: rust-reference (edition guide 2024 unsafe attributes; std env
  module docs — the `# Safety` section on `set_var`).

## 4. `#[deprecated]` is the standard library's drift signal

- **RULE**: the std (and ecosystem) marks known-superseded items with
  `#[deprecated(since = "...", note = "...")]`; calling one without
  `#[allow(deprecated)]` produces a `deprecated` warning that names the
  replacement. Deprecated items are eventually removed or denied.
- **WHY AI GETS IT WRONG**: agents treat warnings as noise, keep using the old
  name, and the code breaks on the next toolchain/dependency bump. A stale
  pattern is exactly what a pre-cutoff model reproduces.
- **CORRECT REASONING**: every deprecation warning is an instruction: read the
  `note`, adopt the replacement, and reserve `#[allow(deprecated)]` for a
  deliberate, commented bridge. Never add a dependency or API that is already
  deprecated in the pinned version.
- **EXAMPLE** (bad): `crate-api-drift-bad` — `parse_config_legacy()` called
  un-gated:
  ```
  warning: use of deprecated function `parse_config_legacy`: use `parse_config` instead
  ```
- **COUNTEREXAMPLE** (good): `crate-api-gate` — the replacement is called and
  the legacy call is gated:
  ```rust
  let v = parse_config();
  #[allow(deprecated)]
  let legacy = parse_config_legacy();
  ```
  `cargo check --offline` is clean (exit 0, no warnings).
- **VERIFICATION**: `cargo check --offline` inside `examples/bad/crate-deprecation`
  prints the deprecation warning and exits 0; the same command inside
  `examples/good/crate-api-gate` is warning-free — both recorded.
- **SOURCE**: rust-reference (attributes `deprecated`); rust-api-guidelines
  (C-DEPRECATED-style hygiene, INFERRED mapping).

## 5. MSRV: APIs stabilized after the declared minimum

- **RULE**: `rust-version` in `Cargo.toml` declares the minimum Rust version.
  Every API used must have been stabilized by that version; otherwise the
  pinned toolchain reports E0599 (`no method named ... found`) or E0557/... .
  The build gate must run on the pinned toolchain, not on the newest one.
- **WHY AI GETS IT WRONG**: models pick the newest API they know (e.g.
  `Option::is_some_and`, stabilized in 1.70) regardless of the project's MSRV,
  then validate against a fresh toolchain where it always compiles.
- **CORRECT REASONING**: read `rust-version` before writing code; for a crate
  that must build on the MSRV, prefer APIs stabilized at or before it, and
  verify with the actual pinned toolchain: `rustup toolchain install 1.70`
  then `cargo +1.70 check`.
- **EXAMPLE** (bad): a crate declaring `rust-version = "1.69"` using
  `Option::is_some_and` — E0599 on the 1.69 toolchain (target verification;
  the 1.69 toolchain is not installed on this machine, so the specific error
  text is INFERRED from rustc semantics, the mechanism is KNOWN).
- **COUNTEREXAMPLE** (good): the demo crates declare `rust-version = "1.70"`
  and the `cargo run` output includes the declared value read from the
  environment:
  ```
  declared rust-version: 1.70
  ```
- **VERIFICATION**: `cargo run --offline` inside `examples/good/crate-api-gate`
  exits 0 and prints the declared rust-version — recorded.
- **SOURCE**: rust-reference (Cargo manifest `rust-version`); cargo-semver-checks
  (MSRV-aware breaking checks).

## 6. Library releases need `cargo semver-checks`

- **RULE**: for published library crates, API breakage must be measured, not
  guessed. cargo-semver-checks compares the public API against a released
  baseline and reports each breaking change (removed item, changed signature,
  tightened bounds, etc.) with its semver class.
- **WHY AI GETS IT WRONG**: agents decide "this refactor is internal" without
  checking exported items; a removed `pub fn` is a breaking change even if no
  internal caller uses it.
- **CORRECT REASONING**: treat every change to `pub` items as possibly
  breaking until `cargo semver-checks` says otherwise; release `0.x` bumps for
  any reported break.
- **EXAMPLE** (bad): deleting an exported type from a published `1.x` crate
  and releasing a patch version — downstream `cargo check` failures.
- **COUNTEREXAMPLE** (good): run `cargo semver-checks check-release` in CI and
  gate the release on a clean report.
- **VERIFICATION**: `cargo semver-checks` is not installed on this machine —
  documented target verification (UNVERIFIED locally).
- **SOURCE**: cargo-semver-checks; arxiv-2503-16922 (stabilized-API failure
  class).

## Quick reference table

| Drift class | Symptom | Correct response |
|---|---|---|
| Stabilized/renamed API | E0308/E0599 on pinned toolchain | Check signature via `cargo check` against installed version |
| Behavioral (edition) | 2018 compiles, 2021 E0614/E0382 | Use `.iter()`, compile with the crate's edition |
| Behavioral (unsafe) | `set_var` E0133 on edition 2024 | `unsafe` block with real single-threaded contract, or `Command::env` |
| Deprecated item | `#[warn(deprecated)]` with note | Adopt the named replacement; gate bridges explicitly |
| MSRV mismatch | E0599 on pinned older toolchain | Read `rust-version`; verify with `cargo +<msrv> check` |
| Library breakage | Downstream breaks after release | `cargo semver-checks` before tagging |
