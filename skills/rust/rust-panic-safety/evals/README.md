# Evaluation — rust-panic-safety

Skill: `skills/rust/rust-panic-safety`. Toolchain: rustc 1.97.1, edition 2021, Windows.
Each bad example panics or aborts (recorded below); each good example prints `OK` and
exits 0. All claims marked VERIFIED were exercised in this run.

## Synthetic evals

- **easy/positive**: correct `Result`-based handling of hostile input
  (`examples/good/good_result.rs`). Must NOT flag.
- **easy/negative**: `unwrap()` on untrusted input (`bad_unwrap.rs`) — must detect panic
  reachability and fix with `Result`/`?`.
- **medium/negative**: panic through an `extern "C"` export (`bad_ffi_panic.rs`) — must
  contain it with `catch_unwind` at the boundary.
- **medium/negative**: `RefCell` borrow panicking in a re-entrant hot path
  (`bad_refcell.rs`) — must fix borrow scope.
- **hard/negative**: `Drop` that panics during an unwind (`bad_drop_unwind.rs`) — double
  panic aborts; must make `Drop` infallible.
- **hard/ambiguous**: `panic=abort` deployment where the codebase relies on
  `catch_unwind` (`good_abort_strategy.rs` under `-C panic=abort`) — agent must mark the
  strategy mismatch.
- **adversarial**: debug-only overflow that "passes" release tests and vice versa
  (overflow_check) — agent must not assume uniform overflow behavior.
- **adversarial**: code that compiles and passes naive tests but panics on out-of-bounds
  user input (`bad_index.rs` with index 7) — the tests never exercised the boundary.

## False-positive evals (correct code must not be flagged)

- `good_result.rs` — `Result` handling of untrusted input; do NOT demand it "return
  without errors".
- `good_ffi_catch.rs` — `extern "C"` export with `catch_unwind` + error code; do NOT flag
  as "can still panic".
- `good_mutex_poison.rs` — explicit `PoisonError::into_inner()` recovery; do NOT demand
  `.unwrap()` or mark the recovery as a bug.
- `good_catch_unwind.rs` — `catch_unwind` + `AssertUnwindSafe` used for boundary isolation;
  do NOT flag the `AssertUnwindSafe` wrapper.
- `good_abort_strategy.rs` — correct `cfg!(panic = "unwind")` branching; do NOT flag the
  `catch_unwind` under the `abort` branch as dead code without the build context.

## Verification commands (executed)

```
rustc --edition 2021 <file>.rs -o <out>
rustc --edition 2021 -C panic=abort <file>.rs -o <out>
rustc --edition 2021 --test examples/good/good_should_panic.rs -o <out>
```

## Verified facts (rustc 1.97.1, Windows)

| Case | Command | Exit | Observed |
|---|---|---|---|
| `good_result` "abc" | `rustc --edition 2021 good_result.rs` | 0 | `rejected: invalid id: "abc"` + `OK` |
| `good_checked_index` "7" | compile + run | 0 | `rejected: index 7 out of bounds` + `OK` |
| `good_ffi_catch` | compile + run | 0 | `OK`; `process(-1)` → -1, panic hook still printed to stderr |
| `good_mutex_poison` | compile + run | 0 | `OK`; lock recovered via `into_inner`, `is_poisoned()==true` |
| `good_catch_unwind` | compile + run | 0 | `guard dropped (destructor runs during unwind)` + `OK` |
| `good_abort_strategy` | compile + run | 0 | `strategy: unwind, catch_unwind caught the panic` + `OK` |
| `good_abort_strategy` | `-C panic=abort` | 0 | `strategy: abort, catch_unwind would never return` + `OK` |
| `good_should_panic` | `rustc --test` | 0 | `test result: ok. 2 passed; 0 failed` |
| `bad_unwrap` "abc" | compile + run | 101 | `called \`Result::unwrap()\` on an \`Err\` value: ParseIntError { kind: InvalidDigit }` |
| `bad_unwrap` "abc" | `-C panic=abort` | -1073740791 (0xC0000409) | same message, process aborted instead of unwound panic |
| `bad_index` "7" | compile + run | 101 | `index out of bounds: the len is 3 but the index is 7` |
| `bad_refcell` | compile + run | 101 | `RefCell already mutably borrowed` |
| `bad_ffi_panic` | compile + run | -1073740791 (0xC0000409) | `panicked: negative input` then `panic in a function that cannot unwind` then `thread caused non-unwinding panic. aborting.` |
| `bad_drop_unwind` | compile + run | -1073740791 (0xC0000409) | `panicked: operation failed` then `panicked: cleanup failure` (panic in Drop during unwind) → abort |
| overflow `255+1` | `rustc` (no -O) | 101 | `attempt to add with overflow` |
| overflow `255+1` | `rustc -O` | 0 | prints `y = 0` (wraps) |
| constant `data[7]` | compile | error | `#![deny(unconditional_panic)]` (compile-time) |
| `extern "C-unwind"` + catch | compile + run | 0 | panic caught at boundary; `OK` |

Note: exit `-1073740791` == `0xC0000409` (STATUS_STACK_BUFFER_OVERRUN), the Windows
`abort`/`__fastfail` code. Under `panic=unwind` an uncaught panic in `main` exits 101.

## Panic message corpus (for eval assertions)

- `called \`Result::unwrap()\` on an \`Err\` value: ...`
- `index out of bounds: the len is N but the index is M`
- `already mutably borrowed` / `already borrowed`
- `attempt to add with overflow`
- `panic in a function that cannot unwind` (extern "C" guard abort), followed by
  `thread caused non-unwinding panic. aborting.` (observed for `bad_ffi_panic`)
- double panic (panic in `Drop` during unwind) prints both panic sites then aborts;
  Linux prints `thread panicked while panicking. aborting.`, Windows (observed) aborts
  with exit `0xC0000409` and no trailing line
