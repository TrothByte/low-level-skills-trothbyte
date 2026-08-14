# Rust Panic Safety & Unwind Discipline

Each entry: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.
Claims marked `VERIFIED` were exercised against rustc 1.97.1 (see `evals/README.md`).

## 1. Panic reachability — untrusted input must not reach a panicking construct

- **RULE**: `unwrap()`, `expect()`, slice indexing, debug-only arithmetic overflow, and
  `RefCell` borrow violations panic. Any path where *untrusted input* can drive one of
  these is a panic-reachability bug: one malformed request terminates the thread (or the
  process on the main thread) — a denial of service. The borrow checker does not track
  panic reachability; it is entirely on the author.
- **WHY AI GETS IT WRONG**: "it compiles, so it's safe" — the type checker proves memory
  safety, not absence of panics. AI-generated code is measurably prone to unwrap-on-input
  patterns (CyberSecEval reports models generating unsafe `unwrap` usage). `unwrap()` is
  also the default reflexive fix an agent reaches for when a `Result` is "annoying".
- **CORRECT REASONING**: classify every value: *internal invariant* vs *external input*.
  `unwrap()`/`expect()` are only sound where the precondition is proved by the code that
  produced the value (e.g. a `Vec::get` result already checked). Data crossing a trust
  boundary must be handled with `Result`/`Option` combinators — never unwrapped.
- **EXAMPLE** (bad): `raw.trim().parse::<u32>().unwrap()` on a request field. VERIFIED:
  with input `"abc"` this panics `called \`Result::unwrap()\` on an \`Err\` value:
  ParseIntError { kind: InvalidDigit }`, exit code 101.
- **COUNTEREXAMPLE** (good): `match raw.parse() { Ok(id) => id, Err(_) => reject(raw) }`,
  or `?` inside a `fn -> Result`.
- **VERIFICATION**: grep for `unwrap()`/`expect()` on paths fed by external input; run with
  hostile inputs (`"abc"`, `""`, oversized integers); `cargo clippy -- -D clippy::unwrap_used`
  flags the construct (lint `unwrap_used`).
- **SOURCE**: `rust-reference` (panic.html: language constructs that panic automatically);
  `clippy-lints` (unwrap_used); `cwe` (CWE-248, CWE-190); `cyberseceval` (panic-pattern
  findings in generated code).

## 2. Panic strategy: `panic=unwind` vs `panic=abort`

- **RULE**: the default strategy is `unwind`: a panic unwinds the stack, runs `Drop`, and
  stops at a recovery point (`catch_unwind`, thread boundary). With `panic=abort`, a panic
  terminates the process immediately: no unwinding, no `Drop`, no `catch_unwind` — even if
  the API exists, it can never catch anything. The strategy is a build decision (`-C
  panic=abort`), not a runtime flag.
- **WHY AI GETS IT WRONG**: agents write `catch_unwind` around everything and assume it
  works "always"; they forget that under `panic=abort` the same binary aborts, and that
  crates with mixed strategies may fail to link. Agents also assume `panic=abort` "fixes"
  unwinding bugs — it converts panics into process death, which is worse for availability.
- **CORRECT REASONING**: `catch_unwind` only exists meaningfully under `unwind`. Choose the
  strategy per target: embedded/bare-metal usually `abort` (no unwinder); long-running
  servers with FFI or task isolation usually `unwind` plus explicit `catch_unwind` at
  boundaries. Detect at compile time with `cfg!(panic = "abort")` rather than assuming.
- **EXAMPLE** (bad): `-C panic=abort` deployed in a server where a single request panic is
  expected to be contained. VERIFIED: `bad_unwrap` under `-C panic=abort` exits
  `0xC0000409` (abort) instead of the recoverable-exit `101` of the unwind build.
- **COUNTEREXAMPLE** (good): `if cfg!(panic = "unwind") { catch_unwind(...) }`; keep
  request isolation under `unwind`, reserve `abort` for targets that cannot unwind.
  VERIFIED: `good_abort_strategy` prints the compiled-in strategy and exits 0 under both.
- **VERIFICATION**: `rustc -C panic=abort` and run; inspect `cfg!(panic = "abort")`.
- **SOURCE**: `rust-reference` (panic.html: panic strategy, standard behavior).

## 3. Unwinding across FFI is undefined behavior — catch at the boundary

- **RULE**: a panic that unwinds past a stack frame which does not allow unwinding is
  undefined behavior (Rust Reference `behavior-considered-undefined.html`, "unwinding past
  a stack frame that does not allow unwinding"; Rustonomicon: "unwinding into another
  language from Rust is Undefined Behavior"). `extern "C"` frames do not allow unwinding.
  A Rust `extern "C"` export must never let a panic escape it.
- **WHY AI GETS IT WRONG**: agents write `extern "C"` functions that `panic!` internally and
  assume the caller "handles it". On modern rustc the consequence looks like a crash
  (`panic in a function that cannot unwind`), so the UB is masked; on older toolchains or
  with a C++/C caller the unwind crosses the boundary and state is silently clobbered.
- **CORRECT REASONING**: the boundary is a *containment point*. Wrap the entire body in
  `catch_unwind`, translate the failure into a return code, and never let an unwind
  propagate out. Since Rust 1.71, `extern "C-unwind"` exists for the rare case where the
  foreign side is known to support unwinding; otherwise `extern "C"` + `catch_unwind`.
- **EXAMPLE** (bad):
  ```rust
  #[no_mangle]
  pub extern "C" fn process(v: i32) -> i32 {
      if v < 0 { panic!("negative input"); }  // unwinds toward the C caller
      v * 2
  }
  ```
  VERIFIED: rustc 1.97.1 aborts with `panic in a function that cannot unwind` then
  `thread caused non-unwinding panic. aborting.`, exit `0xC0000409`.
- **COUNTEREXAMPLE** (good):
  ```rust
  #[no_mangle]
  pub extern "C" fn process(v: i32) -> i32 {
      match std::panic::catch_unwind(|| work(v)) { Ok(c) => c, Err(_) => -1 }
  }
  ```
  VERIFIED: returns `-1` for negative input; exit 0.
- **VERIFICATION**: run the export with a panicking path; expect the abort message. Test
  `extern "C-unwind"` + `catch_unwind` in the caller to see the legal variant.
- **SOURCE**: `rust-reference` (behavior-considered-undefined.html, panic.html §Unwinding
  across FFI boundaries); `rustonomicon` (unwinding.html).

## 4. `catch_unwind` and `AssertUnwindSafe`

- **RULE**: `std::panic::catch_unwind` runs a closure and returns `Ok(result)` normally or
  `Err(Box<dyn Any>)` if the closure panicked. The closure must be `UnwindSafe`;
  `AssertUnwindSafe` suppresses the check where the author accepts responsibility. It
  catches only *unwinding* panics, not aborts; it is not a general try/catch and must not
  be used as one. Dropping the `Err` payload can itself panic.
- **WHY AI GETS IT WRONG**: agents add `AssertUnwindSafe` mechanically without realizing it
  papers over `&mut`/`&Cell` capture — after a caught panic the referenced state may be
  mid-mutation, and a `&mut` may have been invalidated. Agents also reach for
  `catch_unwind` where a `Result` is the correct answer, paying the unwind cost for a
  normal control flow.
- **CORRECT REASONING**: `catch_unwind` is for *exception safety at a boundary*, not
  control flow. Treat the `Err` payload as evidence the closure's state is broken; either
  discard the state or explicitly repair it before reusing. `AssertUnwindSafe` is an
  assertion, so keep the captured state out of later use or revalidate it.
- **EXAMPLE** (bad):
  ```rust
  let mut buf = vec![0u8; 16];
  catch_unwind(AssertUnwindSafe(|| { buf.truncate(0); panic!("x"); }))?;
  println!("{}", buf.len());  // reads state left mid-mutation
  ```
- **COUNTEREXAMPLE** (good): treat the guard as a unit of work whose state is discarded on
  `Err`; VERIFIED: `good_catch_unwind` catches, downcasts the payload, and verifies
  `resume_unwind` re-throws to the next enclosing catch.
- **VERIFICATION**: `cargo test` with a panicking closure; downcast the payload with
  `payload.downcast_ref::<&str>()`.
- **SOURCE**: `rust-reference` (panic.html: recovery mechanisms); `rustonomicon`
  (unwinding.html: catch_unwind).

## 5. `Drop` during unwind and panic-in-`Drop` double panic

- **RULE**: during unwinding, local `Drop` impls run "as if every function instantly
  returned" (Rustonomicon). A panic inside `Drop` while already unwinding is a *double
  panic*: the process aborts. Destructors must therefore be panic-free, or at minimum must
  not panic during unwind.
- **WHY AI GETS IT WRONG**: agents put fallible logic (locking, logging, assertions) in
  `Drop` and forget that during unwind the stack is already broken — one more panic turns
  a recoverable error into an abort. They also assume "Drop runs, so the program is clean"
  and miss that `Drop` running *during unwind* may observe half-torn state.
- **CORRECT REASONING**: `Drop::drop` is a cleanup opcode: it must not panic and must not
  depend on invariants that a mid-unwind stack cannot guarantee. Keep it infallible —
  releases only, never `unwrap()` on states you do not own, and ignore best-effort
  failures (`let _ = ...`). The Rust API Guidelines C-DTOR-FAIL codifies: a destructor
  that fails prevents cleanup of the remaining stack.
- **EXAMPLE** (bad):
  ```rust
  struct Cleanup;
  impl Drop for Cleanup { fn drop(&mut self) { panic!("cleanup failure"); } }
  fn work() { let _c = Cleanup; panic!("operation failed"); }
  ```
  VERIFIED: first panic `operation failed`, then `cleanup failure` during unwind; the
  process aborts, exit `0xC0000409`.
- **COUNTEREXAMPLE** (good): drop only performs infallible releases; recoverable failures
  are reported after the fact, e.g. `let _ = file.sync_all();` instead of
  `file.sync_all().unwrap();`.
- **VERIFICATION**: inject a panic into a `Drop` under a panicking path; the binary must
  abort. Run with `RUST_BACKTRACE=1` to see both panic sites.
- **SOURCE**: `rust-api-guidelines` (C-DTOR-FAIL); `rust-reference` (panic.html
  §Unwinding: destructor guarantee); `rustonomicon` (unwinding.html).

## 6. `RefCell` borrow panics and `Mutex`/`RwLock` poisoning

- **RULE**: `RefCell::borrow()`/`borrow_mut()` panic on borrow violation (`already
  mutably borrowed` / `already borrowed`). `Mutex::lock()` returns `Result<Guard,
  PoisonError>`; a thread that panics while holding the lock poisons it, and subsequent
  `lock()` calls return `Err` — unwrapping them turns one thread's panic into every other
  waiter's panic.
- **WHY AI GETS IT WRONG**: agents treat `RefCell` as "runtime borrow checking that never
  fails" and put `borrow_mut` in re-entrant or long-lived paths; and they write
  `.lock().unwrap()` reflexively, converting poisoning from an advisory signal into a
  cascade of panics.
- **CORRECT REASONING**: a `RefCell` borrow must be provably scoped (no re-entry while a
  borrow is live). For `Mutex`, decide the policy: propagate the poison (unwrap), recover
  (`poisoned.into_inner()` when the data is known good), or ignore by policy. Poisoning is
  advisory and must not be relied on for `unsafe` soundness (std::sync::Mutex docs).
- **EXAMPLE** (bad): `let mut v = cell.borrow_mut(); v.push(x); self.recheck();` where
  `recheck` re-borrows. VERIFIED: panics `RefCell already mutably borrowed`, exit 101.
- **COUNTEREXAMPLE** (good):
  ```rust
  let guard = match mtx.lock() { Ok(g) => g, Err(p) => p.into_inner() };
  ```
  VERIFIED: `good_mutex_poison` runs after a worker panicked while holding the lock, still
  exiting 0.
- **VERIFICATION**: exercise the re-entrant path with a test; assert poison with
  `is_poisoned()` and recover via `PoisonError::into_inner()`.
- **SOURCE**: `rust-reference` (panic.html: panic in std constructs); `cwe` (CWE-248).

## 7. `resume_unwind`

- **RULE**: `std::panic::resume_unwind(payload)` re-throws a previously caught panic
  payload, propagating it to the next enclosing recovery point (another `catch_unwind`, a
  thread boundary). It is the correct way to forward a panic you chose not to handle.
- **WHY AI GETS IT WRONG**: agents either drop the payload (silently swallowing the panic)
  or re-panic with `panic!("{}", msg)` — which loses the original payload and runs the
  panic hook again. `resume_unwind` preserves the payload and suppresses the duplicate hook.
- **CORRECT REASONING**: a caught `Err(payload)` is yours to resolve: handle it, or hand it
  back with `resume_unwind(payload)`. Do not fabricate a new panic from a debug-string.
- **EXAMPLE** (bad): `let _ = catch_unwind(|| panic!("x"));` — swallowed.
- **COUNTEREXAMPLE** (good): `panic::resume_unwind(catch_unwind(|| panic!("x")).err().unwrap());`
  VERIFIED: `good_catch_unwind` shows the payload is re-caught by an outer `catch_unwind`.
- **VERIFICATION**: catch, `resume_unwind`, and assert the outer catch receives it.
- **SOURCE**: `rust-reference` (panic.html); `rustonomicon` (unwinding.html).

## 8. `#[should_panic]`

- **RULE**: the `should_panic` test attribute makes a test pass only if the function
  panics; `expected = "..."` requires the panic message to contain that substring. The
  reference documents it under testing attributes; the test harness runs it via `--test`.
- **WHY AI GETS IT WRONG**: agents assume a panicking `#[test]` is a failure by definition
  and remove tests that document expected panics; or they write `#[should_panic]` on
  functions whose panic is not actually guaranteed (e.g. constant OOB indexing, which
  `rustc` rejects at compile time with the `unconditional_panic` lint before the test can
  run).
- **CORRECT REASONING**: `#[should_panic]` is a contract test: it pins *that* a path
  panics. Use runtime-computed values so the panic is genuinely reachable at runtime.
- **EXAMPLE** (bad): `#[should_panic] fn t() { let _ = data[7]; }` — denied by
  `unconditional_panic`. VERIFIED: rustc 1.97.1 rejects with `#![deny(unconditional_panic)]`.
- **COUNTEREXAMPLE** (good):
  ```rust
  #[test]
  #[should_panic(expected = "index out of bounds")]
  fn oob_panics() { let data = [1u8,2,3]; let i: usize = data.len() + 4; let _ = data[i]; }
  ```
  VERIFIED: harness reports `2 passed; 0 failed`.
- **VERIFICATION**: `rustc --edition 2021 --test file.rs` then run the produced binary.
- **SOURCE**: `rust-reference` (attributes/testing.html: should_panic).

## 9. Integer overflow panics are debug-only

- **RULE**: arithmetic overflow (`a + b`, `a * b`, ...) panics in debug builds and wraps
  (two's complement) in release builds. Overflow checking is a codegen option
  (`-C overflow-checks`), on by default for debug. Constant overflow is a compile error in
  both profiles via the `arithmetic_overflow` lint.
- **WHY AI GETS IT WRONG**: agents assume overflow behavior is uniform ("Rust always
  panics on overflow" / "Rust always wraps"). Code tested only under `cargo test` (debug)
  silently changes semantics under `--release` — and conversely, a release-only wrap bug is
  invisible in debug. Input-driven arithmetic can panic in debug = panic reachability.
- **CORRECT REASONING**: use checked/wrapping/saturating arithmetic
  (`checked_add`, `wrapping_add`, `saturating_add`) whenever the magnitude is not
  statically bounded; treat `+` on externally sized quantities as "panics in debug, wraps
  in release" until proven otherwise.
- **EXAMPLE** (bad): `let y = base + 1;` where `base` comes from input. VERIFIED: debug
  build panics `attempt to add with overflow`, exit 101; `-O` build prints `y = 0` (wraps).
- **COUNTEREXAMPLE** (good): `base.checked_add(1).ok_or(Error::Overflow)?`.
- **VERIFICATION**: run the same input under `rustc file.rs` and `rustc -O file.rs`;
  observe panic vs wrap.
- **SOURCE**: `rust-reference` (panic.html: panic on language constructs; `-C panic`
  interaction); `cwe` (CWE-190).

## Quick detection table

| Panic source | Detect | Fix |
|---|---|---|
| unwrap/expect on input | code review; `clippy::unwrap_used` | `Result`/`?`/`match` |
| indexing with input index | hostile-input tests | `.get()` + error |
| panic through `extern "C"` | run export; abort msg | `catch_unwind` at boundary |
| `RefCell` re-entry | panic msg | prove borrow scope |
| `.lock().unwrap()` cascade | poison test | match / `into_inner` |
| panic in `Drop` | double-panic abort | infallible `Drop` |
| overflow (debug only) | run debug vs release | `checked_*` |
