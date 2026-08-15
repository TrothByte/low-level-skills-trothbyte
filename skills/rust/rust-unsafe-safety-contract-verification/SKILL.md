---
name: rust-unsafe-safety-contract-verification
description: Use when auditing unsafe Rust: every unsafe block must carry a SAFETY comment whose preconditions are real and encoded in the type system. Prevents fabricated contracts — like Bun PathString.rs (2026) — that claim a 'caller guarantees' invariant no type enforces, silently permitting use-after-free.
---

# Rust Unsafe Safety-Contract Verification

## When to use

- Auditing any `unsafe { }`, `unsafe fn`, or `unsafe impl` for the truth of
  its SAFETY comment.
- Reviewing raw-pointer structs and handles whose safety depends on a claimed
  "caller guarantees ..." invariant.
- Deciding whether an invariant is enforced by types or only asserted in a
  comment.
- Verifying unsafe code with Miri before it is merged.

## When not to use

- General unsafe reasoning (aliasing, provenance, validity) — use
  `rust-unsafe-reasoning`.
- Safe code — the borrow checker enforces those contracts.
- C code — SAFETY comments are Rust-specific; use `c-undefined-behavior`.

## What the agent often gets wrong

- "The SAFETY comment says it, so it is safe." A comment is not a mechanism:
  rustc compiles code whose SAFETY claim names an invariant nothing enforces
  (Bun PathString.rs, 2026 — a fabricated "caller guarantees ..." contract
  around a raw pointer permitted a use-after-free).
- "Caller guarantees X" is accepted at face value. Every precondition must be
  checkable by the type system or the enclosing safe API, otherwise it is a
  lie that propagates to every caller.
- "It compiles and the tests pass, so the contract holds." `cargo check`
  passes for the fake contract AND the real one — verified below; rustc
  cannot read comments. Only type encoding (or Miri) distinguishes them.
- "PhantomData is optional bookkeeping." `PhantomData<&'a T>` is the
  mechanism that ties a raw pointer to a borrow; without it the struct has
  no lifetime relationship and the compiler cannot reject use-after-scope.
- "The raw pointer keeps memory alive." It does not; only ownership/borrows
  do. Dropping the backing value while the pointer is cached is UAF that
  usually prints garbage, not a crash.
- "Miri is overkill." Miri is the ground truth for exactly these classes
  (dangling deref, uninit reads, aliasing); automated unsafe repair systems
  that target Miri reach 94.3% pass / 80.4% execution on their benchmark
  (arxiv-2503-02335), showing the classes are real and Miri-detectable.

## How to reason correctly

1. For every `unsafe` item, write the SAFETY comment BEFORE the code, naming
   each precondition as a fact that exists in the types: a lifetime parameter
   (`'a`), a `PhantomData<&'a T>`, a validated length, a proven alignment.
   If you cannot name a type-level mechanism, the block is not justified.
2. Ask "what stops this code from being misused?" For a raw-pointer handle,
   the answer must be a type/lifetime structure (e.g. `PhantomData<&'a T>`)
   plus a safe constructor that ties the pointer to a borrow. "Callers must
   be careful" is not an answer.
3. Prove the invariant with the compiler: attempt the misuse. A lifetime
   encoded in types makes misuse a compile error (E0597/E0515); a fabricated
   contract compiles and exhibits UB at runtime.
4. Verify with Miri (`cargo +nightly miri run` / `miri test`) before
   trusting; ASan/TSan do not model Rust aliasing and their silence is not
   a pass.
5. Prefer safe abstractions: a safe constructor taking `&'a T`, slice-based
   APIs, and `PhantomData` keep the invariant in the type system where the
   compiler enforces it forever.

## What to verify

- Every unsafe block has a SAFETY comment that names a mechanism the type
  system can verify (lifetimes, `PhantomData`, validity invariants).
- No invariant is claimed solely as "caller must ..." — every such claim is
  either encoded in the signature or the block is unsafe-invalid.
- The misuse test compiles to an error for the good pattern and silently
  compiles (UAF) for the fabricated pattern.
- Miri reports zero UB on the code paths (target verification).
- `cargo check` passes (necessary but never sufficient).

## How to verify

```
rustc --edition 2021 examples/good/phantomdata_invariant.rs -o g.exe && ./g.exe
  # prints 42 — the invariant is enforced by PhantomData<&'a T>
rustc --edition 2021 examples/bad/lifetime_escape.rs -o e.exe
  # E0597: `v` does not live long enough — the type-level guard works
rustc --edition 2021 examples/bad/fake_safety_contract.rs -o f.exe && ./f.exe
  # exit 0, prints garbage — UAF; the SAFETY comment was a lie
cargo check --offline   # inside examples/good/crate-invariant-check
cargo test --offline    # both structs' tests pass — rustc cannot tell them apart
cargo +nightly miri run # target verification: flags the fake contract
```

## Where the knowledge comes from

- The Rust Reference — unsafety, SAFETY comment conventions, "behavior
  considered undefined" (rust-reference).
- The Rustonomicon — the safety contract model, `PhantomData`, ownership
  (rustonomicon).
- Miri — operational UB detection; the documented target verification
  (rust-miri).
- arxiv-2503-02335 — RustBrain: 94.3% pass / 80.4% execution on Miri for
  automated unsafe-repair, confirming Miri-detectable classes.
- Bun PathString.rs (2026) — fabricated SAFETY contract instance from the
  2026-08-15 agent-failures survey (KNOWN; file-level details UNVERIFIED on
  this machine).

## Related skills

- `rust-unsafe-reasoning` — the semantics SAFETY comments must reference
  (recommend)
- `rust-panic-safety` — unsafe drop/cleanup contracts (cross-link)
- `rust-ffi-boundary` — unsafe across the FFI edge (cross-link)
- `zeroize-constant-time` — unsafe zeroing with real preconditions
  (cross-link)

## Evaluation

- Synthetic: fabricated-contract code must be flagged; PhantomData-backed
  code must pass.
- False-positive: correct unsafe with a valid SAFETY comment referencing
  type-level invariants must NOT be flagged.
- Historical: Bun PathString.rs class — "caller guarantees" with no
  lifetime/PhantomData backing.
- Adversarial: the fake contract compiles and prints garbage; the misuse of
  the real contract is a compile error.
- Commands and recorded results: `evals/README.md`.
