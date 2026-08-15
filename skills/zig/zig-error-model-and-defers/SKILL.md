---
name: zig-error-model-and-defers
description: Use when writing or reviewing Zig error handling: error sets and unions, try/catch, defer/errdefer LIFO semantics, optionals, and Illegal Behavior rules. Prevents resource leaks on error paths, double frees from defer+errdefer, wrong defer order, and unwrap panics. Version-pinned to Zig 0.15-0.17.
---

# Zig Error Model and Defers

## When to use

- Designing functions that fail: `error{T}!T` signatures, `try`, `catch`, error sets.
- Writing resource cleanup with `defer`/`errdefer` and reasoning about order.
- Reviewing optional handling (`?T`, `orelse`, `if (x) |v|`, `.?`) and Illegal Behavior
  triggers (unwrap null, unwrap error, overflow, OOB).
- Reading error return traces and `@errorName`/`@errorReturnTrace`.

## When not to use

- Exception-style control flow or cross-language error interop — see
  `ffi-boundary-cross-language` and `c-errno-and-syscall-returns`.
- Defining what is UB in general — see `c-undefined-behavior`/`compiler-ub-assumptions`;
  this skill covers Zig's *safety-checked* Illegal Behavior.
- Allocation-lifetime reasoning — see `zig-allocators-and-memory-management` (which owns
  the defer/deinit pairing patterns).

## What the agent often gets wrong

- Believing `defer` runs at function exit only — it runs at scope exit, LIFO (reverse
  order of declaration), and not at all if the scope was never entered.
- Writing both `defer` and `errdefer` that free the same resource on the error path —
  a double free exactly when cleanup matters most.
- Using `catch unreachable` for fallible operations (allocation, syscalls) and calling it
  "recoverable"; `error.OutOfMemory` is data, not a lie.
- Confusing optionals with error unions: `if (opt) |v|` vs `foo() catch |err|`; or using
  `.?` when null is a legitimate input — unwrap-null is Illegal Behavior (panic in
  Debug/ReleaseSafe).
- Assuming `errdefer` runs on successful return too — it runs only when the function
  returns an error.
- Putting `return` inside a `defer` — a compile error.
- Treating Zig errors as exceptions with implicit context — errors are values; error
  return traces (Debug) provide the context.

## How to reason correctly

1. Decide the failure mode: fallible functions return `error{T}!T`; absence of a value is
   an optional `?T`. `try` propagates; `catch`/`else |err|` handles locally; `switch` on
   an error union gives the full control flow.
2. Clean up in the reverse order of acquisition: `defer` LIFO pairs every acquisition with
   a free that runs on every exit — success and error. Use `errdefer` only for cleanup
   that must happen exclusively on the error path.
3. Never free the same resource with both `defer` and `errdefer`; structure cleanup so
   exactly one runs.
4. Scope defers tightly: a `defer` inside a loop body runs at the end of each iteration,
   not at function end.
5. Guard Illegal Behavior: unwrap only after `if`/`orelse`/`catch` established the
   payload; bounds-check indexes; use `+%`/`+|-` when wrap/saturate is intended.
6. Read error return traces in Debug to locate the origin; use `@errorName` for messages.

## What to verify

- Every resource acquisition is paired with exactly one cleanup path (defer/errdefer),
  and none pair both for the same resource.
- `errdefer` appears only where the error path needs distinct cleanup.
- No `catch unreachable` on genuinely fallible calls; `error.OutOfMemory` propagates.
- Optionals are unwrapped only after null handling; no `.?` on possibly-null values.
- No `return` inside `defer`; no LIFO-order dependence on cleanup.
- Error sets are explicit where the API demands it; inferred sets are fine internally.

## How to verify

```
zig test examples/good/defers.zig
zig test examples/good/error_union.zig
zig test examples/bad/double_cleanup.zig     # double free on the error path (fails/caught)
zig test examples/bad/unwrap_null.zig         # panic: attempted to unwrap null
zig test examples/bad/ignored_error.zig       # catch unreachable on a fallible op (review)
zig build test
```

Researched — zig not installed on this host; commands are the recorded verification plan.

## Where the knowledge comes from

- zig-langref §defer, §errdefer, §Errors (Error Set Type, Error Union Type, catch, try,
  errdefer, Error Return Traces), §Optionals, §unreachable, §Illegal Behavior, §while
  (error-union and optional loops).
- zig-std-source (std/testing.zig — expectError, expectEqual; std/start.zig error handling).
- zig-release-notes 0.15.x (New Rules for Arithmetic on undefined — a related Illegal
  Behavior tightening).

## Related skills

- `zig-allocators-and-memory-management` — defer/deinit pairing, leak detection.
- `zig-comptime-metaprogramming` — error sets inside comptime functions.
- `zig-ffi-c-interop` — errno-style error interop with C.
- `zig-inline-asm-and-abi` — unchecked Illegal Behavior (missing clobbers).
- `c-errno-and-syscall-returns` — comparable C error-return discipline.

## Evaluation

- Synthetic: `defer`+`errdefer` double free, wrong LIFO order, `catch unreachable` on
  fallible ops, `.?` on null, error-set mismatch — must be caught; good defer/errdefer and
  error-union examples must pass.
- False-positive: a `defer` that closes a file once while an `errdefer` handles a
  distinct resource, and a justified `catch unreachable` after an invariant proves
  success, must NOT be flagged.
- Historical: 0.15.x arithmetic-on-`undefined` tightening is a related regression target.
- Adversarial: a resource that leaks only on the error path (no errdefer) and a
  double-free that only triggers when the inner call fails — cleanup-matrix analysis.
- Commands and recorded results: `evals/README.md`.
