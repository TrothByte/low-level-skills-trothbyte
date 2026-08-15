---
name: zig-comptime-metaprogramming
description: Use when writing or reviewing Zig generics, reflection, comptime evaluation, @compileError gates, or inline-for metaprogramming. Prevents mixing runtime and comptime values, exceeding the comptime branch budget, using 0.15-era @Type, or letting lazy analysis skip intended compile errors. Version-pinned to Zig 0.15-0.17.
---

# Zig Comptime Metaprogramming

## When to use

- Writing or reviewing generics (`fn Foo(comptime T: type) type`), reflection, type
  reification, or comptime code generation.
- Diagnosing `unable to resolve comptime value`, branch-quota, or comptime-overflow errors.
- Choosing between `comptime` blocks, `inline for`, and `@typeInfo`-driven dispatch.
- Reviewing code that must build on Zig 0.16+ where `@Type` was split into
  `@Int` / `@Struct` / `@Union` / `@Enum` / `@Pointer` / `@Fn` / `@Tuple` / `@EnumLiteral`.

## When not to use

- Runtime dispatch or polymorphism — use function pointers or tagged unions.
- Reading user input or runtime state — comptime cannot see values not known to the compiler.
- Including files or preprocessor logic — use `@import`, `@embedFile`, and the build system.
- Optimizing hot loops — comptime changes semantics, not speed; measure with
  `performance-measurement-discipline`.

## What the agent often gets wrong

- Passing a runtime-known value to a `comptime` parameter — a compile error, not a branch.
- Assuming `comptime var` plus a plain `while` runs at comptime in an ordinary function —
  compile-time iteration requires `inline while` / `inline for`.
- Using the 0.15-era `@Type(.{ .int = .{ .signedness = .unsigned, .bits = 10 } })` on Zig
  0.16+ where `@Type` is removed; the migration is `@Int(.unsigned, 10)`.
- Believing comptime evaluation is unbounded — the default branch quota is 1000;
  recursion needs a deliberate `@setEvalBranchQuota`.
- Forgetting that `if`/`switch` on a comptime value is statically evaluated and the
  not-taken branch is never analyzed, so a bug there silently disappears.
- Putting `@compileError` on a path that lazy analysis never reaches — it never fires.
- Claiming comptime evaluation is "more optimized" code; it is a correctness/expressiveness tool.

## How to reason correctly

1. Classify each value as comptime-known or runtime-known. `comptime` parameters,
   `comptime` blocks, and `comptime var` require comptime-known values; any dependency on a
   runtime value is a compile error at that point.
2. Iterate compile-time collections with `inline for` / `inline while` plus a comptime
   induction variable; each iteration is analyzed and unrolled for its own comptime state.
3. Reflect with `@typeInfo` (tags in source order) and `@TypeOf`; dispatch on the union tag
   and access fields with `@field` / `@fieldParentPtr`. The print-in-Zig case study in the
   langref is the reference pattern.
4. On 0.16+, reify types with `@Int`, `@Struct`, `@Union`, `@Enum`, `@Pointer`, `@Fn`,
   `@Tuple`, `@EnumLiteral`; on 0.15 the same job used `@Type(.{ ... })`.
5. Raise the branch quota only for bounded, intended comptime recursion; leave the default
   1000 otherwise.
6. Put `@compileError` gates on paths that ARE analyzed when the user hits them — e.g. in
   the body of an instantiated generic or a top-level comptime expression — and verify with
   a bad example that the diagnostic actually fires.

## What to verify

- Every `comptime` parameter call site passes a comptime-known value.
- Compile-time iteration uses `inline for` / `inline while`, not a runtime `while` on a
  `comptime var`.
- Branch quota raised only for bounded recursion; default 1000 otherwise.
- No `@Type` anywhere in code that targets 0.16+; reification uses the split builtins.
- `@compileError` gates sit on analyzed paths; a bad example proves the diagnostic fires.
- Code compiles and tests pass under the pinned Zig version.

## How to verify

```
zig test examples/good/comptime_reflect.zig
zig test examples/good/inline_for.zig
zig test examples/good/type_builtins_016.zig
zig test examples/good/comptime_budget_ok.zig
zig build test                    # for projects with build.zig
```

Bad files must fail to compile with the expected diagnostic class:

```
zig test examples/bad/runtime_type_arg.zig    # error: unable to resolve comptime value
zig test examples/bad/comptime_budget.zig     # error: evaluation exceeded 1000 back branches
zig test examples/bad/type_015_syntax.zig     # 0.16+: @Type removed (exact error text UNVERIFIED)
```

Researched — zig not installed on this host; these commands are the recorded verification
plan, not run results.

## Where the knowledge comes from

- zig-langref §comptime (Compile-Time Parameters, Variables, Expressions; Generic Data
  Structures; Case Study: print in Zig) and §Builtin Functions (`@compileError`,
  `@compileLog`, `@setEvalBranchQuota`, `@typeInfo`, `@TypeOf`, `@field`, `@fieldParentPtr`,
  `@Int`, `@Struct`, `@Union`, `@Enum`, `@Pointer`, `@Fn`, `@Tuple`, `@EnumLiteral`).
- zig-release-notes 0.16.0 "Language Changes → @Type Replaced with Individual
  Type-Creating Builtin Functions" (proposal #10710).
- zig-std-source (std/lang/Type.zig, std/testing helpers used in the examples).

## Related skills

- `zig-version-migration` — the `@Type` split and other 0.15→0.16 breaking changes.
- `zig-error-model-and-defers` — `try` and error sets inside comptime functions.
- `zig-simd-vector-intrinsics` — comptime vector lengths and builtins.
- `zig-fuzzer-and-testing` — `comptime { try std.testing.expectEqual(...) }` assertions.
- `zig-build-system-and-packages` — generating Zig source at build time via
  `addAnonymousImport`.

## Evaluation

- Synthetic: bad cases (runtime value to comptime param, runtime `while` on a comptime var,
  `@Type` on 0.16+, unbounded comptime recursion, `@compileError` on a dead path) must be
  caught; good cases compile and pass.
- False-positive: legitimate `inline for` unrolling, bounded `@setEvalBranchQuota`, `@Int`/
  `@Struct` reification, and reached `@compileError` gates must NOT be flagged.
- Historical: the `@Type` → `@Int` migration (0.16.0) is the regression target; code is
  updated, not condemned as "bad practice" in 0.15.
- Adversarial: a generic instantiated only on a path that never reaches its `@compileError`.
- Commands and recorded results: `evals/README.md`.
