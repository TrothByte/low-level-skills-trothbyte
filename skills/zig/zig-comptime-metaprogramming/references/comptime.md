# Zig Comptime Metaprogramming — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.
Version markers: KNOWN / INFERRED / UNVERIFIED.

## 1. comptime parameters are generics; the call site value must be comptime-known

- **RULE**: a parameter declared `comptime T: type` (or any `comptime` parameter) must
  receive a value known at compile time, or the compiler errors. Inside the function the
  value is comptime-known. This is how Zig implements generics — "compile-time duck typing".
- **WHY AI GETS IT WRONG**: writes `max(if (cond) u32 else i32, ...)` with a runtime
  `cond`, expecting a runtime branch; or passes runtime data to a comptime parameter and
  blames the compiler.
- **CORRECT REASONING**: if `cond` is runtime-known, `if (cond) u32 else i32` is not a
  comptime value, so the call cannot be analyzed. A `comptime` parameter means "the value is
  known at compile time here" on both sides of the call.
- **EXAMPLE** (bad):
  ```zig
  fn max(comptime T: type, a: T, b: T) T { return if (a > b) a else b; }
  test "runtime type" {
      var runtime_bool: bool = undefined;
      _ = &runtime_bool;
      _ = max(if (runtime_bool) u32 else i32, 1, 2); // error: unable to resolve comptime value
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn max(comptime T: type, a: T, b: T) T { return if (a > b) a else b; }
  test "comptime type" {
      const T = if (@import("builtin").os.tag == .windows) u32 else i64;
      _ = max(T, 1, 2); // comptime-known branch, fine
  }
  ```
- **VERIFICATION**: `zig test` — bad file fails with `unable to resolve comptime value`;
  good file passes.
- **SOURCE**: zig-langref §comptime (Compile-Time Parameters).

## 2. Inside a comptime expression the compiler interprets the code

- **RULE**: inside a `comptime` block, all `if`/`while`/`for`/`switch` are evaluated at
  compile time, all variables are comptime, `return`/`try` are invalid, and any code with
  global runtime side effects is a compile error. The same function can be called from
  runtime and from comptime without modification.
- **WHY AI GETS IT WRONG**: assumes a `comptime` block is a hint; or expects `comptime {
  foo(); }` to defer work to runtime.
- **CORRECT REASONING**: `comptime { ... }` is a hard requirement, not an optimization
  hint. `comptime` blocks are where you compute constants, assert type properties, and
  build data tables.
- **EXAMPLE** (bad):
  ```zig
  extern fn exit() noreturn;
  test "comptime call of extern" {
      comptime { exit(); } // error: comptime call of extern function
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  test "fibonacci at comptime and runtime" {
      const expectEqual = @import("std").testing.expectEqual;
      try expectEqual(@as(u32, 13), fibonacci(7));      // runtime
      try comptime expectEqual(@as(u32, 13), fibonacci(7)); // comptime
  }
  fn fibonacci(index: u32) u32 {
      if (index < 2) return index;
      return fibonacci(index - 1) + fibonacci(index - 2);
  }
  ```
- **VERIFICATION**: `zig test` on both files.
- **SOURCE**: zig-langref §comptime (Compile-Time Expressions).

## 3. Compile-time iteration needs inline while / inline for

- **RULE**: to iterate a comptime-known collection inside an ordinary function and have
  each iteration analyzed (with its own comptime state), use `inline while` or `inline
  for`. A plain `while` with a `comptime var` does not unroll; in a normal function it is
  runtime code, and using a comptime variable there is an error.
- **WHY AI GETS IT WRONG**: writes `comptime var i = 0; while (i < n) : (i += 1)` and
  expects compile-time unrolling; or uses a runtime `for` and then indexes a comptime
  construct with `i`.
- **CORRECT REASONING**: `inline for (0..n) |i|` generates one copy of the body per
  value of `i`, each analyzed with `i` as a comptime constant. This is how `std.fmt`
  parses format strings and how the print case study works.
- **EXAMPLE** (bad):
  ```zig
  fn sum3() u32 {
      var total: u32 = 0;
      comptime var i = 0;
      while (i < 3) : (i += 1) {   // not inline: runtime loop on comptime var
          total += i;              // error: use of comptime value in runtime expression
      }
      return total;
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn sum3() u32 {
      var total: u32 = 0;
      inline for (0..3) |i| {      // unrolled: three analyzed copies
          total += i;
      }
      return total;
  }
  ```
- **VERIFICATION**: `zig test` — bad fails to compile; good passes and `objdump`/LIR shows
  three adds.
- **SOURCE**: zig-langref §comptime (Compile-Time Variables), §while (inline while),
  §for (inline for).

## 4. Type reflection: @typeInfo / @TypeOf / @field / @fieldParentPtr

- **RULE**: `@typeInfo(T)` returns a tagged union describing the type; struct/union/enum
  field order is guaranteed to match the source. `@TypeOf` yields a type with no runtime
  side effects. `@field(obj, "name")` accesses a field/declaration by comptime string;
  `@fieldParentPtr` recovers the container from a field pointer.
- **WHY AI GETS IT WRONG**: enumerates `@typeInfo` union tags with the wrong spelling
  (`.struct` instead of `."struct"`), forgets switch prongs must handle every tag, or
  names the union type `std.builtin.Type` on a version where it moved to `std.lang.Type`.
- **CORRECT REASONING**: match on tags as written in source: `."struct"`, `."union"`,
  `."enum"`, etc. Prefer `switch (info) { .@"struct" => |s| ..., else => ... }`. Avoid
  naming the reflection namespace; let `@typeInfo` infer it.
- **EXAMPLE** (bad):
  ```zig
  fn countFields(comptime T: type) usize {
      return @typeInfo(T).struct.fields.len; // error when T is not a struct
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn countFields(comptime T: type) usize {
      return switch (@typeInfo(T)) {
          .@"struct" => |s| s.fields.len,
          .@"union" => |u| u.fields.len,
          else => @compileError("expected struct or union, found " ++ @typeName(T)),
      };
  }
  ```
- **VERIFICATION**: `zig test examples/good/comptime_reflect.zig`.
- **SOURCE**: zig-langref §Builtin Functions (@typeInfo, @typeName, @TypeOf, @field,
  @fieldParentPtr).

## 5. Reifying types: @Type (0.15) vs the split builtins (0.16+)

- **RULE**: Zig 0.16.0 replaced `@Type(.{ ... })` with `@Int`, `@Struct`, `@Union`,
  `@Enum`, `@Pointer`, `@Fn`, `@Tuple`, `@EnumLiteral` (proposal #10710). On 0.16+,
  `@Type` no longer exists; on 0.15 the split builtins do not exist.
- **WHY AI GETS IT WRONG**: writes `@Type(.{ .int = .{ .signedness = .unsigned, .bits =
  10 } })` from memory and gets a "no builtin function @Type" error on 0.16+, or
  retrofits `@Struct` into 0.15 code.
- **CORRECT REASONING**: on 0.16+ use `@Int(.unsigned, 10)` for integers and
  `@Struct(.auto, null, &.{"x","y"}, &.{u32, u32}, &@splat(.{}))` for structs. `@splat`
  fills "default" field-attribute arrays. There is no `@Array`/`@Optional`/`@ErrorUnion` —
  use normal syntax.
- **EXAMPLE** (bad, 0.16+):
  ```zig
  const Pair = @Type(.{ .@"struct" = .{ .layout = .auto, .fields = &.{
      .{ .name = "x", .type = u32, .default_value_ptr = null, .is_comptime = false, .alignment = @alignOf(u32) },
      .{ .name = "y", .type = u32, .default_value_ptr = null, .is_comptime = false, .alignment = @alignOf(u32) },
  }, .decls = &.{}, .is_tuple = false } });
  ```
- **COUNTEREXAMPLE** (good, 0.16+):
  ```zig
  const Pair = @Struct(.auto, null, &.{ "x", "y" }, &.{ u32, u32 }, &@splat(.{}));
  ```
- **VERIFICATION**: `zig test examples/good/type_builtins_016.zig` passes;
  `zig test examples/bad/type_015_syntax.zig` fails on 0.16+ with a missing-`@Type`
  error (exact text UNVERIFIED — zig not installed on this host).
- **SOURCE**: zig-release-notes 0.16.0 (Language Changes → @Type Replaced); zig-langref
  §Builtin Functions (@Int, @Struct, @Union, @Enum, @Pointer, @Fn, @Tuple, @EnumLiteral).

## 6. The comptime branch budget is 1000 by default

- **RULE**: the compiler gives up after 1000 comptime branches unless `@setEvalBranchQuota`
  raises the limit for the enclosing scope. This catches infinite recursion at compile time
  (KNOWN: the langref notes a stack-overflow design flaw for `i32` recursion in current
  master).
- **WHY AI GETS IT WRONG**: writes unbounded comptime recursion and reports a confusing
  "exceeded 1000 back branches" error, or sprinkles huge quotas everywhere.
- **CORRECT REASONING**: default 1000 is deliberate. Raise it only for bounded, intended
  computation (e.g. a known-depth table build), and keep the raise local to the function.
- **EXAMPLE** (bad):
  ```zig
  fn loop() void { loop(); }
  test "infinite comptime recursion" {
      comptime loop(); // error: evaluation exceeded 1000 back branches
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  test "bounded comptime recursion" {
      @setEvalBranchQuota(100_000);
      comptime {
          try @import("std").testing.expectEqual(@as(u32, 6765), fib(20));
      }
  }
  fn fib(index: u32) u32 {
      if (index < 2) return index;
      return fib(index - 1) + fib(index - 2);
  }
  ```
- **VERIFICATION**: `zig test examples/bad/comptime_budget.zig` fails;
  `zig test examples/good/comptime_budget_ok.zig` passes.
- **SOURCE**: zig-langref §comptime, §Builtin Functions (@setEvalBranchQuota).

## 7. @compileError and lazy analysis

- **RULE**: `@compileError(msg)` is a compile error when semantically analyzed. Because
  declarations are analyzed lazily, an uninstantiated generic or unreachable branch never
  evaluates its `@compileError` — that is the mechanism, not a bug.
- **WHY AI GETS IT WRONG**: expects `@compileError` at the bottom of an uninstantiated
  generic to fire, then "fixes" it by removing the gate or by eager-loading everything.
- **CORRECT REASONING**: the gate fires exactly when the compiler reaches it: instantiate
  the generic with the offending type in a test, or place the gate in a top-level comptime
  expression. `@hasDecl` / sentinel-value feature detection is the intended alternative for
  "supported?" checks (see 0.15.1 release notes usingnamespace removal).
- **EXAMPLE** (bad):
  ```zig
  fn unsupported(comptime T: type) void {
      _ = T;
      @compileError("T unsupported"); // only fires if this line is analyzed
  }
  test "never fires" {
      // never calling unsupported() means the gate is silent
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn unsupported(comptime T: type) void {
      if (T == u8) @compileError("T unsupported");
  }
  test "gate fires on bad instantiation" {
      unsupported(u8); // analyzed here -> compile error
  }
  ```
- **VERIFICATION**: `zig test` on the good pattern fails as intended; the bad pattern
  compiles silently (must be caught by review).
- **SOURCE**: zig-langref §Builtin Functions (@compileError), §compilation model (lazy
  analysis); zig-release-notes 0.15.1 (usingnamespace removal — feature detection pattern).

## 8. comptime for vs inline for — pick by what you need

- **RULE**: `comptime for` does not exist as separate syntax; the choice is between
  evaluating the whole loop at comptime (inside a `comptime` block, plain `for` is
  evaluated at comptime) and unrolling per-iteration code with `inline for` in a runtime
  function. The langref "print" case study uses `inline for (format, 0..) |c, i|`.
- **WHY AI GETS IT WRONG**: writes `inline for` when the loop body must stay runtime
  (calling a function with runtime side effects per iteration), or expects plain `for` to
  unroll.
- **CORRECT REASONING**: unrolling (inline) specializes each iteration with distinct
  comptime state; comptime evaluation (comptime block) produces a single computed result.
  If the body needs runtime arguments, use a normal `for`.
- **EXAMPLE** (bad): `inline for` over a huge runtime-sized range — `inline` requires the
  length to be comptime-known.
- **COUNTEREXAMPLE** (good):
  ```zig
  fn apply(comptime n: usize, x: u32) u32 {
      var result = x;
      inline for (0..n) |_| result += 1; // comptime-known n required
      return result;
  }
  test "unroll" { try @import("std").testing.expectEqual(@as(u32, 3), apply(3, 0)); }
  ```
- **VERIFICATION**: `zig test examples/good/inline_for.zig`.
- **SOURCE**: zig-langref §comptime (Case Study: print in Zig), §for (inline for).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| comptime param | value must be comptime-known at the call site; generics = duck typing |
| comptime block | hard requirement: if/while/for/switch evaluated at comptime |
| comptime iteration | `inline for`/`inline while` unroll; plain `for` in a `comptime` block evaluates |
| reflection | `@typeInfo` union tags: `."struct"`, `."union"`, `."enum"`; fields in source order |
| reification 0.15 | `@Type(.{ .int = ..., .@"struct" = ... })` |
| reification 0.16+ | `@Int`, `@Struct`, `@Union`, `@Enum`, `@Pointer`, `@Fn`, `@Tuple`, `@EnumLiteral` |
| branch budget | default 1000 comptime branches; `@setEvalBranchQuota` for bounded recursion |
| @compileError | fires only when the line is analyzed (lazy analysis); test bad instantiation |
| @compileLog | prints at comptime and forces a compile error until removed |
