# Zig Error Model and Defers — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.
Version markers: KNOWN / INFERRED / UNVERIFIED.

## 1. defer runs at scope exit, LIFO

- **RULE**: `defer` executes unconditionally when its enclosing scope exits — and in the
  REVERSE order of declaration. A `defer` inside a loop body runs at the end of each
  iteration. Defers in a branch that was never entered do not run.
- **WHY AI GETS IT WRONG**: expects function-level cleanup; expects forward (FIFO) order;
  writes `defer` inside `if (false)` and is surprised it never runs.
- **CORRECT REASONING**: cleanup should mirror acquisition in reverse. The langref prints
  "2 1" for two defers declared 1 then 2.
- **EXAMPLE** (bad):
  ```zig
  fn f() void {
      var a: usize = 1;
      {
          defer a = 2;   // runs at end of this block
          a = 1;
      }
      std.debug.print("{d}", .{a}); // prints 2, not 1 — agent expected function-level
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn g() void {
      defer print("1 ");
      defer print("2 ");
      print("0 ");
  }
  // prints "0 2 1" — LIFO
  ```
- **VERIFICATION**: `zig test examples/good/defers.zig`; the langref defer_examples are
  the reference.
- **SOURCE**: zig-langref §defer.

## 2. errdefer runs only on the error path

- **RULE**: `errdefer` runs when the scope exits via an error; on a successful return it
  does not. It is the tool for "clean up only if this step failed" — distinct from
  `defer`'s always-run cleanup.
- **WHY AI GETS IT WRONG**: writes `errdefer` expecting cleanup on success too; or uses
  `defer` for an allocation that must survive into the returned value (double-free of the
  result on success).
- **CORRECT REASONING**: choose per resource: `defer` if every exit needs the free;
  `errdefer` if the success path hands ownership to the caller.
- **EXAMPLE** (bad):
  ```zig
  fn make(allocator: std.mem.Allocator) ![]u8 {
      const buf = try allocator.alloc(u8, 16);
      errdefer allocator.free(buf);   // frees only on error — correct here
      // but the agent pairs it with:
      defer allocator.free(buf);      // double free on error, frees the returned buf on success
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn make(allocator: std.mem.Allocator) ![]u8 {
      const buf = try allocator.alloc(u8, 16);
      errdefer allocator.free(buf);   // exactly once, only when an error escapes
      // ... fill buf ...
      return buf;                     // caller owns it now
  }
  ```
- **VERIFICATION**: `zig test examples/bad/double_cleanup.zig` — the error path frees
  twice; `zig test examples/good/defers.zig` passes.
- **SOURCE**: zig-langref §Errors (errdefer), §defer.

## 3. return is not allowed inside defer

- **RULE**: a `return` inside a `defer` expression is a compile error.
- **WHY AI GETS IT WRONG**: writes cleanup that tries to "override" the return value.
- **CORRECT REASONING**: `defer` bodies may not divert control flow; restructure by
  computing the value first and cleaning up after.
- **EXAMPLE** (bad):
  ```zig
  fn f() error{Failure}!void {
      defer {
          return error.DeferError; // error: cannot return from defer expression
      }
      return error.Failure;
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn f() error{Failure}!void {
      defer {}   // cleanup only
      return error.Failure;
  }
  ```
- **VERIFICATION**: `zig test` — bad fails to compile; good passes.
- **SOURCE**: zig-langref §defer.

## 4. Error sets and error unions; try / catch / else |err|

- **RULE**: `error{T}!T` is an error union; `try` propagates the error, `catch` (with
  `|err|` for the payload) handles it, `else |err|` in a `switch` covers all errors.
  Error sets can be explicit (`error{Foo}`), inferred, or the global set (`anyerror`).
  Zig errors are values — `@errorName(err)` and error return traces (Debug) give context.
- **WHY AI GETS IT WRONG**: treats errors like exceptions (implicit stack, arbitrary
  "throws"); writes `catch unreachable` for fallible calls; mixes optional and error-union
  unwrapping syntax.
- **CORRECT REASONING**: an error union is `error set` + `payload`; unwrap with `try`
  (propagate), `catch` (handle), or `switch`. Handle `error.OutOfMemory` as data.
- **EXAMPLE** (bad):
  ```zig
  fn parseNumber(s: []const u8) u32 {
      return std.fmt.parseInt(u32, s, 10) catch unreachable; // parse can fail
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn parseNumber(s: []const u8) !u32 {
      return std.fmt.parseInt(u32, s, 10); // propagates error.InvalidCharacter etc.
  }
  ```
- **VERIFICATION**: `zig test examples/bad/ignored_error.zig` (review: `catch unreachable`
  hides real failure); `zig test examples/good/error_union.zig` passes.
- **SOURCE**: zig-langref §Errors (Error Union Type; catch; try; Error Return Traces).

## 5. Optionals are not error unions

- **RULE**: `?T` means "maybe absent"; unwrap with `if (x) |v|`, `orelse`, or `.?`.
  Unwrapping null is Illegal Behavior (panics in Debug/ReleaseSafe). `while (x) |v|` loops
  until null. Optional loops and error-union loops (`while (f()) |v|` / `else |err|`) are
  distinct langref constructs.
- **WHY AI GETS IT WRONG**: `.?` on a possibly-null value; treats `null` as "error"; uses
  optional syntax on error unions.
- **CORRECT REASONING**: nullability and failure are separate axes. Guard before unwrap.
- **EXAMPLE** (bad):
  ```zig
  fn firstChar(s: ?[]const u8) u8 {
      return s.?[0]; // panic if null
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn firstChar(s: ?[]const u8) ?u8 {
      const value = s orelse return null;
      return if (value.len > 0) value[0] else null;
  }
  ```
- **VERIFICATION**: `zig test examples/bad/unwrap_null.zig` panics in Debug;
  the good version returns null gracefully.
- **SOURCE**: zig-langref §Optionals, §Illegal Behavior (Attempt to Unwrap Null),
  §while (with Optionals).

## 6. Illegal Behavior is safety-checked in Debug/ReleaseSafe

- **RULE**: unwrapping null/error, indexing out of bounds, integer overflow, division by
  zero, truncating casts, and invalid enum casts are Illegal Behavior. In Debug and
  ReleaseSafe they panic with a trace; in ReleaseFast/ReleaseSmall the optimizer assumes
  they cannot happen. In `comptime` evaluation they are always compile errors when known.
- **WHY AI GETS IT WRONG**: "fixes" a Debug panic by switching to ReleaseFast (hiding the
  bug); or claims Zig is "safe" because code panics — the panic is the detector, not the
  fix.
- **CORRECT REASONING**: keep safety modes on during development; treat each panic as a
  contract violation to fix at the call site (guard, propagate, or use wrapping ops).
- **EXAMPLE** (bad):
  ```zig
  const idx = computeIndex();
  const value = table[idx];  // panics when idx >= table.len in Debug
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  const idx = computeIndex();
  const value = if (idx < table.len) table[idx] else null; // explicit bound
  ```
- **VERIFICATION**: run the bad index in Debug (panic) vs ReleaseFast (garbage — flag);
  `zig test` in Debug catches it.
- **SOURCE**: zig-langref §Illegal Behavior, §Build Mode, §unreachable.

## 7. Error return traces

- **RULE**: in Debug builds, errors carry a return trace of up to N stack frames (the
  default build prints the trace on failure); `@errorReturnTrace()` exposes it. Errors
  are values, so the trace is metadata for debugging, not part of the error's identity.
- **WHY AI GETS IT WRONG**: compares errors by trace; assumes traces exist in ReleaseFast
  (they do not — the trace machinery is disabled).
- **CORRECT REASONING**: match errors by code (`error.X`), print context with
  `@errorName`; treat traces as debug-only aids.
- **EXAMPLE** (bad):
  ```zig
  const a = error.Foo;
  const b = error.Foo;
  if (a == b) {} // true: same code, correct
  // but the agent compares @errorReturnTrace() results — nonsense in ReleaseFast
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  switch (foo()) {
      error.Bar => std.debug.print("bar\n", .{}),
      else => |err| std.debug.print("{s}\n", .{@errorName(err)}),
  }
  ```
- **VERIFICATION**: `zig test` in Debug prints the trace; ReleaseFast does not
  (UNVERIFIED exact output on this host).
- **SOURCE**: zig-langref §Errors (Error Return Traces; Implementation Details),
  §Builtin Functions (@errorReturnTrace, @errorName).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| defer | runs at scope exit, LIFO, not if scope never entered; no `return` inside |
| errdefer | runs only on the error path — distinct cleanup per resource |
| defer+errdefer | never free the same resource in both — double free on the error path |
| error union | `error{T}!T`; `try`/`catch`/`else \|err\|`; errors are values |
| optionals | `?T`; guard before `.?`; null unwrap = Illegal Behavior panic |
| Illegal Behavior | panic in Debug/ReleaseSafe; optimizer assumes absent in ReleaseFast/ReleaseSmall |
| error traces | Debug-only metadata; compare by code, print with `@errorName` |
| OutOfMemory | an error value — propagate, never `catch unreachable` blindly |
