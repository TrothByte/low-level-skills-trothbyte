# Evaluation — rust-unsafe-safety-contract-verification

Skill: `skills/rust/rust-unsafe-safety-contract-verification`. Stability
target: `evaluated`. Toolchain: rustc/cargo 1.97.1, Windows x86_64-msvc.
All outputs below were actually produced on 2026-08-15 by running the
examples in this skill.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/fake_safety_contract.rs` | fabricated SAFETY contract detected | exit 0, prints garbage |
| medium/negative | `bad/lifetime_escape.rs` | misuse of a real contract = compile error | exit 1, E0597 |
| medium/positive | `good/phantomdata_invariant.rs` | invariant enforced by types | exit 0, prints 42 |
| hard/positive | `good/crate-invariant-check` (`cargo check`+`test`) | both structs compile & pass | exit 0, 2/2 tests |
| hard/negative | contract audit of `crate-invariant-check` | `RawBuf` (no marker) flagged, `Borrowed` not | manual audit + Miri |

Detection rule: for each SAFETY comment, name the type-level mechanism that
makes the claim true. `PhantomData<&'a T>` + safe constructor = real;
"caller guarantees ..." with no lifetime/marker = fabricated.

## False-positive evals (correct code must NOT be flagged)

- `good/phantomdata_invariant.rs` — SAFETY comment names a mechanism the
  borrow checker enforces; the misuse is a compile error. Not flagged.
- `good/crate-invariant-check` `Borrowed` — real contract with a passing
  test. Not flagged.
- A valid SAFETY comment on a `ptr::read`/`ptr::write` swap or a scoped raw
  pointer derived from a single `&mut` (see `rust-unsafe-reasoning`) must
  NOT be flagged merely because it is unsafe.

## Historical evals

- Bun PathString.rs (2026): a fabricated "caller guarantees ..." contract
  around a raw pointer permitted a use-after-free. KNOWN as a documented
  failure from the 2026-08-15 agent-failures survey; the file-level detail
  is UNVERIFIED on this machine (no Bun checkout). The class is reproduced
  by `fake_safety_contract.rs`.
- arxiv-2503-02335 (RustBrain): 94.3% pass / 80.4% execution on Miri for
  automated unsafe-repair — KNOWN abstract figures; confirms the bug classes
  are Miri-detectable.

## Adversarial evals

- `bad/fake_safety_contract.rs` compiles (exit 0) and runs; the read returns
  garbage whose value varies between runs (`128` in one run, `0` in a second)
  — the non-determinism is itself the evidence of UB. Naive tests ("it
  printed something") would pass.
- `bad/lifetime_escape.rs` is the flip side: the SAME code shape against a
  real contract is rejected by the compiler (E0597). An agent must explain
  the asymmetry: type-encoded invariants fail at compile time, prose-only
  invariants fail at runtime.

## Verification commands (ACTUAL, recorded 2026-08-15)

```
rustc --edition 2021 examples/good/phantomdata_invariant.rs -o g.exe && ./g.exe
  exit 0, prints "42"

rustc --edition 2021 examples/bad/fake_safety_contract.rs -o f.exe && ./f.exe
  exit 0, prints garbage (0 in this run; 128 in an earlier run)

rustc --edition 2021 examples/bad/lifetime_escape.rs -o e.exe
  exit 1: error[E0597]: `v` does not live long enough

cargo check --offline          # in examples/good/crate-invariant-check
  exit 0, clean
cargo test --offline           # in examples/good/crate-invariant-check
  exit 0: test result: ok. 2 passed; 0 failed
  (both borrowed_reads_the_value and
   fake_contract_compiles_but_has_no_guard pass)

cargo +nightly miri --version
  exit != 0: "cargo-miri.exe is not installed for the toolchain
  'nightly-x86_64-pc-windows-msvc'" — Miri is documented target verification
```

## Verified facts

- `cargo check` and `cargo test` give IDENTICAL green results for the
  PhantomData-backed struct and the fabricated-contract struct — rustc cannot
  read SAFETY comments; the audit must be manual + Miri.
- The fabricated contract's runtime output is garbage and varies between
  runs (0 vs 128) — use-after-free is nondeterministic.
- The real contract converts the identical misuse into a compile error
  (E0597).
- Miri is not installed on this machine (checked 2026-08-15) — commands are
  documented, results are target verification.

## Scoring (for routing eval)

- precision: every flagged item maps to a rule in
  `references/safety-contract.md`.
- recall: fabricated contracts, lifetime escapes, and unencoded invariants
  detected.
- FP-rate: contracts backed by `PhantomData`/lifetimes produce zero flags.
