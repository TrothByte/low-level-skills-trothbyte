# Evaluation — zig-comptime-metaprogramming

Skill: `skills/zig/zig-comptime-metaprogramming`.
Stability target: `researched`. Toolchain: zig is NOT installed on this host; the code
targets the 0.15–0.17 API surface (verified against langref master and the 0.16.0 release
notes). Verification commands below are the recorded plan, not run results.

## Synthetic evals

| Case | Fixture | Expected | Command |
|---|---|---|---|
| easy/negative | `bad/runtime_type_arg.zig` | compile error `unable to resolve comptime value` | `zig test` |
| easy/negative | `bad/comptime_while.zig` | compile error (comptime value in runtime expression) | `zig test` |
| medium/negative | `bad/type_015_syntax.zig` | 0.16+ rejects `@Type` (error text UNVERIFIED) | `zig test` |
| medium/negative | `bad/comptime_budget.zig` | compile error `exceeded 1000 back branches` | `zig test` |
| hard/negative | `bad`/review | `@compileError` on a never-analyzed path compiles silently | review |
| positive | `good/comptime_reflect.zig` | passes | `zig test` |
| positive | `good/inline_for.zig` | passes | `zig test` |
| positive | `good/type_builtins_016.zig` | passes | `zig test` |
| positive | `good/comptime_budget_ok.zig` | passes | `zig test` |

## False-positive evals (correct code must not be flagged)

- `good/inline_for.zig` — legitimate comptime unrolling with a comptime induction variable.
- `good/comptime_budget_ok.zig` — `@setEvalBranchQuota` for bounded, intended recursion.
- `good/type_builtins_016.zig` — `@Int`/`@Struct`/`@Tuple`/`@EnumLiteral` reification.
- `@compileError` reached through a test-instantiated generic is correct gating, not a bug.
- `comptime { try std.testing.expectEqual(...) }` assertions in tests are correct.

## Historical evals

- 0.16.0 proposal #10710 removed `@Type`; `bad/type_015_syntax.zig` reproduces the
  migration breakage and must be rewritten with `@Struct`/`@Int` (see `good/type_builtins_016.zig`).
- 0.15.1 removed `usingnamespace`; conditional-inclusion code must use sentinel values
  (`const foo = if (have_foo) 123 else {};`) plus `@TypeOf(...) == void` checks — a
  comptime-pattern regression an agent should not reintroduce.

## Adversarial evals

- A generic that contains `@compileError` but is never instantiated with a bad type: the
  gate must NOT be reported as "dead code"; the correct action is a test that instantiates
  it, proving the diagnostic fires.
- An agent "fixing" `bad/comptime_budget.zig` by removing the recursion instead of
  recognizing an intended bounded computation must be caught by the branch-quota rules.
- A `comptime` block that computes a table and leaks the budget into a sibling scope —
  `@setEvalBranchQuota` must stay local to the function.

## Verified facts

- KNOWN (langref + 0.16.0 release notes, not run on this host):
  - `@Type` removed in 0.16.0, replaced by 8 builtins (`@EnumLiteral`, `@Int`, `@Tuple`,
    `@Pointer`, `@Fn`, `@Struct`, `@Union`, `@Enum`).
  - `@Int(.unsigned, 10)` is the documented replacement for
    `@Type(.{ .int = .{ .signedness = .unsigned, .bits = 10 } })` and for
    `std.meta.Int` (now deprecated).
  - Default comptime branch quota is 1000; `@setEvalBranchQuota` raises it.
  - `@typeInfo` field order matches source order.
  - The langref notes a current design flaw: infinite comptime recursion on `i32` can
    stack-overflow the compiler instead of cleanly reporting the budget error.
- UNVERIFIED (needs zig on this host): exact error text for the `@Type` removal and for
  the `comptime_while.zig` failure; actual test output of all examples.

## Target toolchains (absent, documented)

- zig 0.15.2 / 0.16.0 / 0.17.0-dev: not installed. First execution plan: install via
  `zigup` (or download from ziglang.org/download), then run the commands in SKILL.md §How
  to verify against each pinned version.
