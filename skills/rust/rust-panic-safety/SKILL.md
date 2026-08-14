---
name: rust-panic-safety
description: Use when writing, reviewing, or debugging Rust code where a panic may be reachable — unwrap/expect on untrusted input, unwinding through extern "C" exports, catch_unwind boundaries, RefCell/Mutex poisoning, Drop during unwind, or panic=abort vs panic=unwind. Teaches panic reachability and unwind discipline as a single reasoning model.
---

# Rust Panic Safety & Unwind Discipline

## When to use

- Handling untrusted input (network, files, CLI, FFI) in Rust: deciding whether a
  `Result`/`Option` is safe to `unwrap` or must be handled.
- Writing or reviewing `extern "C"` exports where an internal `panic!` could escape.
- Choosing the panic strategy (`panic=unwind` vs `panic=abort`) for a target.
- Using `catch_unwind` / `AssertUnwindSafe` / `resume_unwind` at isolation boundaries.
- Reviewing `RefCell` borrows, `Mutex`/`RwLock` poison handling, or `Drop` impls.
- Explaining a crash: `panic in a function that cannot unwind`, `thread panicked while
  panicking`, or a debug-only overflow panic.

## When not to use

- `unsafe` memory-safety reasoning (aliasing, provenance) — use `rust-unsafe-reasoning`.
- ABI/layout pinning for FFI structs — use `ffi-boundary-cross-language`.
- Inter-thread ordering/atomics — use `memory-ordering-reasoning`.
- Error *design* (which errors exist) rather than panic *reachability* — prefer
  `rust-positive-api-design` guidance on `Result` types.

## What the agent often gets wrong

- "It compiles, so it's safe." The borrow checker proves memory safety, NOT absence of
  panics. Untrusted input can still reach `unwrap()` and kill the process (panic
  reachability is a DoS, CWE-248).
- "`catch_unwind` always works." Under `panic=abort` a panic aborts the process; the API
  exists but never catches anything.
- "`extern "C"` just needs a panic-free body." A panic escaping an `extern "C"` frame is
  UB by spec; rustc 1.71+ masks it with an abort at the boundary.
- "`AssertUnwindSafe` is a formality." It asserts that captured `&mut`/`&Cell` state is
  safe to unwind across; after a caught panic that state may be mid-mutation.
- "`Drop` always cleans up cleanly." During unwind a panicking `Drop` double-panics and
  aborts the process.
- "`.lock().unwrap()` is the standard idiom." It converts one thread's panic into a
  poisoning cascade across every waiter.
- "Rust panics on overflow." Only in debug builds; release builds wrap silently.

## How to reason correctly

1. Trace every value from its source: *internal invariant* or *external input*? Anything
   crossing a trust boundary must stay in `Result`/`Option` — never unwrap it.
2. For each panicking construct (`unwrap`, indexing, arithmetic, `borrow_mut`, `lock`),
   ask: can any reachable input violate its precondition? If yes, replace with the
   checked variant (`?`, `.get()`, `checked_add`, poison recovery).
3. At every FFI export, treat the function body as "could panic": wrap in `catch_unwind`
   and translate the failure to a return code. Never let an unwind cross `extern "C"`.
4. Decide the panic strategy deliberately. `unwind` enables recovery; `abort` makes any
   panic fatal. Detect with `cfg!(panic = "abort")`, don't assume.
5. For `Drop`: make it infallible and independent of invariants that a mid-unwind stack
   cannot uphold (double panic = abort).
6. Only use `catch_unwind` for boundary isolation — never as a general try/catch; that is
   what `Result` is for.

## What to verify

- No `unwrap()`/`expect()`/indexing reachable from external input.
- Every `extern "C"` export contains the panic (verified: a panicking path aborts with
  `panic in a function that cannot unwind`; the fixed version returns an error code).
- Poisoning policy is explicit: `match lock() { Ok(g) => g, Err(p) => p.into_inner() }`
  or a conscious propagate decision.
- `Drop` impls never panic; no path panics while another panic is unwinding.
- The panic strategy is the intended one for the target.

## How to verify

```
rustc --edition 2021 bad.rs -o bad && ./bad        # hostile input; record panic + exit code
rustc --edition 2021 good.rs -o good && ./good     # must print "OK", exit 0
rustc --edition 2021 -C panic=abort good_abort_strategy.rs -o p  && ./p
rustc --edition 2021 --test good_should_panic.rs -o t && ./t     # should_panic contract
cargo clippy -- -D clippy::unwrap_used             # lint panic-prone unwraps
```

## Where the knowledge comes from

- The Rust Reference — panic.html (strategy, unwinding, FFI unwinding), behavior-considered-undefined.html (`unwinding past a stack frame that does not allow unwinding`), attributes/testing.html (`should_panic`)
- The Rustonomicon — unwinding.html (unwinding across FFI is UB; catch_unwind; destructors during unwind)
- Rust API Guidelines — C-DTOR-FAIL (destructors that fail)
- MITRE CWE-248/CWE-190; rust-clippy `unwrap_used`; CyberSecEval panic-pattern findings

## Related skills

- `rust-unsafe-reasoning` — unsafe semantics that can panic or interact with unwinding (require)
- `ffi-boundary-cross-language` — ABI/layout and boundary hygiene (recommend)
- `memory-ordering-reasoning` — cross-thread state visible to a survivor thread (recommend)
- `safe-low-level-from-scratch` — positive path for writing safe low-level Rust

## Evaluation

Synthetic: easy (unwrap on input), medium (panic through `extern "C"` export, RefCell
re-entry), hard (Drop panic during unwind), adversarial (`panic=abort` deployment where
`catch_unwind` is expected to recover). False-positive: correct `Result`-based handling,
poison recovery, and a panic-free `extern "C"` export with `catch_unwind` must NOT be
flagged. See `evals/README.md` for commands and recorded rustc 1.97.1 results.
