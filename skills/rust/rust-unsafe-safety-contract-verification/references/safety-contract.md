# Rust Unsafe Safety-Contract Verification — Reference Rules

Knowledge layer for `rust-unsafe-safety-contract-verification`. Format: RULE →
WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE (bad) → COUNTEREXAMPLE
(good) → VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED /
UNVERIFIED.

All commands recorded against rustc/cargo 1.97.1, Windows. Relative paths
assume the skill directory as CWD. Miri is not installed locally — it is the
documented target verification.

## 1. A SAFETY comment is a claim, not a mechanism

- **RULE**: the Rust Reference requires each `unsafe` block to be justified
  by a SAFETY comment, but nothing checks the comment: rustc compiles code
  whose claimed invariant exists only in prose. The Bun PathString.rs class
  (2026, documented in the 2026-08-15 agent-failures survey) is a fabricated
  "caller guarantees ..." contract around a raw pointer that permitted a
  use-after-free. A comment that cannot be verified by the compiler or the
  enclosing safe API is a lie.
- **WHY AI GETS IT WRONG**: models pattern-match the string "SAFETY: caller
  guarantees X is valid" from training data and generate it for code where no
  such guarantee exists, because the text is what they were trained to emit.
- **CORRECT REASONING**: evaluate the comment like code: name the mechanism
  (a lifetime, `PhantomData`, a validated invariant) that makes the claim
  true, and prove the claim is reachable only through safe entry points.
- **EXAMPLE** (bad): `examples/bad/fake_safety_contract.rs` — `RawBuf`
  claims "caller guarantees" the buffer outlives it; nothing encodes that,
  the buffer is dropped, and the read prints garbage (recorded: `128`).
- **COUNTEREXAMPLE** (good): `examples/good/phantomdata_invariant.rs` —
  `PhantomData<&'a T>` ties the pointer to a real borrow; the SAFETY comment
  names exactly that mechanism.
- **VERIFICATION**: rustc compiles BOTH (exit 0) — recorded. The difference
  is shown by the misuse test (rule 3) and by Miri (target verification).
- **SOURCE**: rust-reference (unsafety, SAFETY comments); arxiv-2503-02335
  (repair systems spend most effort on exactly these comment-code mismatches).

## 2. "Caller must ..." preconditions must be encoded in the type system

- **RULE**: every precondition of an unsafe item must be either (a) enforced
  by the type signature (lifetimes, `PhantomData`, `&mut` exclusivity,
  validity invariants) or (b) established by a safe constructor that the
  caller cannot bypass. A precondition that only lives in the SAFETY comment
  is not a contract; it is documentation of a bug.
- **WHY AI GETS IT WRONG**: agents write `unsafe fn` that takes raw pointers
  and push all obligations to "the caller" — semantically valid only if the
  signature lets the caller prove them. Without a lifetime or marker, it
  cannot.
- **CORRECT REASONING**: redesign so the unsafe interior is reachable only
  through safe constructors that receive `&'a T` and store
  `PhantomData<&'a T>`, making the caller's obligations the borrow checker's
  obligations.
- **EXAMPLE** (bad): `unsafe fn wrap(ptr: *const u8) -> RawBuf` with a claim
  that the caller keeps the buffer alive — no lifetime, no marker.
- **COUNTEREXAMPLE** (good): `fn new(r: &'a T) -> Borrowed<'a, T>` — the
  lifetime parameter and `PhantomData<&'a T>` field ARE the mechanism.
- **VERIFICATION**: E0597/E0515 on misuse (rule 3) proves the encoding.
- **SOURCE**: rust-reference (lifetimes); rustonomicon (phantom data,
  meet-safe-and-unsafe).

## 3. The misuse test proves whether the invariant is real

- **RULE**: an invariant encoded in types makes its violation a compile
  error; a fabricated invariant compiles and misbehaves at runtime. The
  discriminating experiment is: write the code that violates the claimed
  invariant and observe the verdict.
- **WHY AI GETS IT WRONG**: agents verify unsafe code by running the happy
  path. Happy paths are identical for real and fake contracts.
- **CORRECT REASONING**: try to hold `Borrowed<'static, u32>` from a local
  `v`; a real contract gives E0597; a fake one compiles. Always include the
  misuse as a negative test (even if only as a documented `cargo check`
  expectation).
- **EXAMPLE** (bad): `examples/bad/lifetime_escape.rs` — escaping `'a`.
- **COUNTEREXAMPLE** (good): same code but compiled only with the real
  contract, where the escape is rejected:
  ```
  error[E0597]: `v` does not live long enough
  ```
- **VERIFICATION**: `rustc --edition 2021 examples/bad/lifetime_escape.rs`
  exits 1 with E0597 (recorded); `rustc` on `fake_safety_contract.rs` exits 0
  (recorded).
- **SOURCE**: rust-reference (borrow checker, lifetime elision).

## 4. cargo check cannot tell a real contract from a fake one

- **RULE**: rustc validates types, not comments. A crate containing BOTH a
  PhantomData-backed struct and a fabricated-contract struct passes
  `cargo check` and `cargo test` identically — demonstrated by
  `examples/good/crate-invariant-check` (both unit tests pass, exit 0).
- **WHY AI GETS IT WRONG**: agents treat "compiles + tests pass" as proof of
  unsafe correctness; for SAFETY comments it is evidence of nothing.
- **CORRECT REASONING**: treat a green build as the baseline, then audit the
  contract: does the comment name a type mechanism? Does the misuse compile?
  Run Miri.
- **EXAMPLE** (bad): the crate's `fake_contract` module — compiles, its test
  passes, the contract is a lie.
- **COUNTEREXAMPLE** (good): the crate's `Borrowed` — compiles, its test
  passes, and the misuse is rejected (rule 3).
- **VERIFICATION**: `cargo check --offline` and `cargo test --offline` inside
  `examples/good/crate-invariant-check` both exit 0 — recorded.
- **SOURCE**: rust-reference (behavior considered undefined — UB is not
  statically detected); rust-miri.

## 5. PhantomData<&'a T> is the mechanism, not bookkeeping

- **RULE**: `PhantomData<&'a T>` makes a raw-pointer struct act as though it
  owns a borrow `&'a T` for variance, auto-trait, and drop-check purposes.
  Without it, a struct holding `*const T` has no lifetime relation to `T`
  and the compiler permits the pointer to outlive the data it references.
- **WHY AI GETS IT WRONG**: agents drop the `PhantomData` field as "unused",
  then wonder why the borrow checker allows UAF.
- **CORRECT REASONING**: every raw-pointer struct that intends to be tied to
  a borrow carries `PhantomData<&'a T>` (or `PhantomData<&'a mut T>`); this
  is what makes E0597 appear in rule 3.
- **EXAMPLE** (bad): `RawBuf { ptr: *const u8 }` — no marker, no lifetime.
- **COUNTEREXAMPLE** (good): `Borrowed<'a, T> { ptr: *const T, _marker:
  PhantomData<&'a T> }`.
- **VERIFICATION**: `phantomdata_invariant.rs` runs and prints `42`
  (recorded); `lifetime_escape.rs` fails E0597 (recorded).
- **SOURCE**: rustonomicon (phantom data); rust-reference (drop check).

## 6. Raw pointers do not keep memory alive

- **RULE**: a raw pointer is a provenance-carrying address, not an owner.
  Dropping the backing value (or `Box`, `String`, `Vec`) frees the memory;
  the pointer dangles. Dereferencing it is UB ("dereferencing a dangling
  pointer" in the Reference's UB list) and typically reads garbage, not a
  crash.
- **WHY AI GETS IT WRONG**: agents write `let p = buf.as_ptr(); drop(buf);`
  and reason "the pointer still has the address, so it works".
- **CORRECT REASONING**: lifetime is established by the borrow, not the
  address; after `drop`, the pointer must never be used. Encoded contract:
  the `&'a T`/`PhantomData` relationship outlives every use of the pointer.
- **EXAMPLE** (bad): `fake_safety_contract.rs` — `drop(buf)` then
  `h.read()`; prints `128` (recorded).
- **COUNTEREXAMPLE** (good): `Borrowed::new(&v)` used only while `v` is
  alive; prints `42` (recorded).
- **VERIFICATION**: Miri flags the dangling deref (target verification);
  the print of garbage is the local symptom.
- **SOURCE**: rust-reference (behavior-considered-undefined.html); rust-miri.

## 7. Miri is the ground truth; ASan/TSan are not

- **RULE**: Miri interprets the program under the aliasing/provenance model
  and reports the exact UB class (dangling deref, uninit read, invalid
  value, race). ASan/TSan do not model Rust aliasing or provenance; their
  silence is not a pass. Automated unsafe-repair benchmarks (RustBrain)
  reach 94.3% pass and 80.4% execution against Miri (arxiv-2503-02335),
  confirming these classes are Miri-detectable.
- **WHY AI GETS IT WRONG**: agents propose ASan as the unsafe checker and
  stop after a clean run.
- **CORRECT REASONING**: run `cargo +nightly miri run` / `miri test` on the
  crate; treat every report as a real defect to fix at the contract level.
- **EXAMPLE** (bad): a fake-contract crate with an ASan-clean debug run.
- **COUNTEREXAMPLE** (good): `cargo +nightly miri test` with zero reports.
- **VERIFICATION**: Miri is not installed locally (confirmed 2026-08-15:
  `cargo +nightly miri --version` → not installed) — documented target
  verification. The RustBrain figures are KNOWN from the abstract.
- **SOURCE**: rust-miri; arxiv-2503-02335; rust-reference.

## Quick reference table

| Question | Good pattern | Fake pattern | Distinguisher |
|---|---|---|---|
| Does the SAFETY comment name a type mechanism? | `PhantomData<&'a T>` | "caller guarantees..." | manual audit |
| Is the misuse a compile error? | E0597 / E0515 | compiles (UAF) | `rustc` the misuse |
| Does `cargo check`/`test` differ? | green | green | it cannot — audit manually |
| Does Miri flag it? | no reports | dangling deref | `cargo +nightly miri run` |
| Runtime symptom when misused | borrow checker prevents it | prints garbage | execute the misuse |
