# Evaluation — zig-error-model-and-defers

Skill: `skills/zig/zig-error-model-and-defers`.
Stability target: `researched`. Toolchain: zig is NOT installed on this host; the code
targets the 0.15–0.17 API surface (verified against the langref Errors/defer/Illegal
Behavior sections). Verification commands below are the recorded plan, not run results.

## Synthetic evals

| Case | Fixture | Expected | Command |
|---|---|---|---|
| easy/negative | `bad/unwrap_null.zig` | panic: attempted to unwrap null (Debug) | `zig test` |
| easy/negative | `bad/double_cleanup.zig` | double free on the error path | `zig test` |
| medium/negative | `bad/ignored_error.zig` | `catch unreachable` on a fallible op — review flag | `zig test` + review |
| hard/negative | review | defer order misread (LIFO), or `return` inside defer | review |
| positive | `good/defers.zig` | passes; LIFO + per-iteration scope | `zig test` |
| positive | `good/error_union.zig` | passes; try/catch/switch/errdefer | `zig test` |

## False-positive evals (correct code must not be flagged)

- `good/error_union.zig` — `errdefer gpa.free(buf)` in a test whose success path never
  frees is correct (buf is a test-local; the errdefer is the only path that could leak).
- `try`-propagated `error.OutOfMemory` and explicit `error.InvalidDigit` handling — correct.
- A `defer` closing one file while an `errdefer` frees a distinct buffer — two resources,
  two mechanisms, correct.

## Historical evals

- 0.15.x "New Rules for Arithmetic on undefined": `_ = a + b;` with `b: u32 = undefined`
  became a compile error ("use of undefined value here causes illegal behavior") — a
  related Illegal Behavior tightening an agent may reintroduce.
- Error return traces remain Debug-only; claims that traces exist in ReleaseFast are stale.

## Adversarial evals

- A resource that leaks only on the error path (missing `errdefer`), where the success
  path looks clean — the cleanup matrix (success vs error × each resource) gate.
- A double-free that triggers only when the inner call fails — `defer` + `errdefer` on the
  same pointer.
- `catch unreachable` on a syscall/allocator that "never fails" in tests but does under
  the adversarial input — the fuzz-input pass must surface it.

## Verified facts

- KNOWN (from langref; not run on this host):
  - `defer` runs at scope exit in reverse order; not run for unentered scopes;
    `return` inside `defer` is a compile error.
  - `errdefer` runs only on the error path.
  - Unwrapping null/error, OOB, integer overflow, and div-by-zero are Illegal Behavior —
    panic in Debug/ReleaseSafe, assumed-absent in ReleaseFast/ReleaseSmall, compile errors
    at comptime when known.
  - Error return traces exist in Debug (`@errorReturnTrace`), not in release modes.
  - `std.testing.expectError`/`expectEqual` check error codes and payloads.
- INFERRED: exact default trace depth.
- UNVERIFIED (needs zig on this host): exact panic messages and trace output.

## Target toolchains (absent, documented)

- zig 0.15.2 / 0.16.0 / 0.17.0-dev: not installed. First execution plan: install zig,
  then run the commands in SKILL.md §How to verify.
