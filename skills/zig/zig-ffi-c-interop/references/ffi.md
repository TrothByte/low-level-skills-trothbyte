# Zig FFI / C Interop — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.
Version markers: KNOWN / INFERRED / UNVERIFIED.

## 1. extern struct has C layout; packed struct is not a C struct

- **RULE**: `extern struct` follows the C ABI: fields in source order with target-defined
  padding. `packed struct` is a bit-packed container with Zig bit-level semantics and is
  NOT the same as a C struct (bitfields especially). For a C struct with bitfields,
  translate-c demotes/refuses — use integer fields plus masks.
- **WHY AI GETS IT WRONG**: writes `packed struct` to "match the C layout" and produces a
  wire format that does not match the C header; or assumes C structs have no padding.
- **CORRECT REASONING**: use `extern struct` and add explicit padding fields where the C
  header has them. Verify with `@offsetOf`/`@sizeOf` against the C compiler on the target.
- **EXAMPLE** (bad):
  ```zig
  // C: struct Pair { uint32_t x; uint8_t tag; };
  const Pair = packed struct { x: u32, tag: u8 }; // wrong padding, wrong alignment
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  // matches C layout for the same struct on x86-64
  const Pair = extern struct { x: u32, tag: u8 };
  comptime {
      if (@sizeOf(Pair) != 8) @compileError("unexpected size");
  }
  ```
- **VERIFICATION**: `zig test examples/good/extern_struct.zig` passes with
  `@sizeOf`/`@offsetOf` checks.
- **SOURCE**: zig-langref §struct (extern struct), §C (Translation failures).

## 2. C type primitives — never guess sizes

- **RULE**: `c_char c_short c_ushort c_int c_uint c_long c_ulong c_longlong c_ulonglong
  c_longdouble` have guaranteed C ABI compatibility; `anyopaque` is C `void`. `c_long`
  is 64-bit on Linux/macOS SysV but 32-bit on Windows (LLP64).
- **WHY AI GETS IT WRONG**: writes `i32` for `int` and `i64` for `long`, which silently
  breaks on Windows where `long` is 32 bits, or uses `bool` where C passes `int`.
- **CORRECT REASONING**: at the boundary, the type IS the C type. Use the `c_*` spelling
  so the compiler chooses the target-correct size; keep `bool` only where C uses `_Bool`.
- **EXAMPLE** (bad):
  ```zig
  extern "c" fn time(p: ?*i64) i64;   // C: time_t*; on Windows i64 != time_t ABI
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  extern "c" fn time(p: ?*c_long) c_long; // C-compatible spelling
  ```
- **VERIFICATION**: `zig test examples/bad/c_long_assumption.zig -target x86_64-windows-gnu`
  and re-run with a `@compileError` size check on the target.
- **SOURCE**: zig-langref §C (C Type Primitives).

## 3. extern fn, callconv(.c), and variadics

- **RULE**: `extern "c" fn` declares a C-ABI function; `fn add(count: c_int, ...)
  callconv(.c) c_int` defines a C variadic, read via `@cVaStart`/`@cVaArg`/`@cVaEnd`
  (with `defer @cVaEnd(&ap)`). Passing a Zig `bool`/`u8` to a C varargs slot may be
  promoted per the C default promotions — match C semantics.
- **WHY AI GETS IT WRONG**: forgets `callconv(.c)` on a variadic definition; forgets
  `defer @cVaEnd(&ap)`; passes Zig ints to `printf`-style varargs assuming no promotion.
- **CORRECT REASONING**: variadics at the C boundary must use the C calling convention;
  `ap` must be ended on all paths. Use the langref `add(count, ...)` example as the
  template.
- **EXAMPLE** (bad):
  ```zig
  fn mySum(count: c_int, ...) c_int {   // missing callconv(.c)
      var ap = @cVaStart();
      return ap.?;                       // no @cVaEnd — leak-ish, incomplete
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn mySum(count: c_int, ...) callconv(.c) c_int {
      var ap = @cVaStart();
      defer @cVaEnd(&ap);
      var sum: c_int = 0;
      var i: usize = 0;
      while (i < @as(usize, @intCast(count))) : (i += 1) {
          sum += @cVaArg(&ap, c_int);
      }
      return sum;
  }
  ```
- **VERIFICATION**: `zig test examples/good/export_lib.zig -lc` (langref's variadic tests
  are the reference).
- **SOURCE**: zig-langref §C (C Variadic Functions), §Builtin Functions (@cVaStart,
  @cVaArg, @cVaEnd, @cVaCopy).

## 4. @cImport is deprecated (0.16); use translate-c

- **RULE**: 0.16.0 deprecated `@cImport`; C translation now happens in the build system
  via `b.addTranslateC(.{ .root_source_file = b.path("src/c.h"), .target, .optimize })`
  exposed as a module import (`.imports = &.{.{ .name = "c", .module = translate_c.createModule() }}`)
  or via the official translate-c package. The CLI `zig translate-c` remains available.
- **WHY AI GETS IT WRONG**: keeps `const c = @cImport({ @cInclude("stdio.h"); });` in
  0.16+ code and gets deprecation breakage; or translates once with host target/flags and
  links with different ones.
- **CORRECT REASONING**: match the translate step to the build: same target, same
  `-cflags` (e.g. `-fshort-enums` changes enum sizes). The langref's c.zig → build.zig
  upgrade guide is the canonical migration.
- **EXAMPLE** (bad, 0.16+):
  ```zig
  const c = @cImport({
      @cInclude("stdio.h");
      @cInclude("math.h");
  });
  ```
- **COUNTEREXAMPLE** (good, 0.16+):
  ```zig
  // build.zig:
  // const translate_c = b.addTranslateC(.{ .root_source_file = b.path("src/c.h") });
  // const exe = b.addExecutable(.{ .name, .root_module = b.createModule(.{
  //     .imports = &.{.{ .name = "c", .module = translate_c.createModule() }},
  // })});
  const c = @import("c");
  ```
- **VERIFICATION**: `zig build test` with the addTranslateC wiring; `zig translate-c
  -target x86_64-linux-gnu examples/good/simple.h`.
- **SOURCE**: zig-release-notes 0.16.0 (@cImport Moving to Build System); zig-langref §C
  (C Translation CLI).

## 5. C pointers [*c]T are for generated code

- **RULE**: `[*c]T` — the C pointer — supports both single-item and many-item syntax,
  coerces to/from integers, allows 0, and appears from translate-c. It cannot express
  Zig-only pointer attributes (alignment). Hand-written FFI should prefer normal pointers
  (`*T`, `[*]T`, `?*T`).
- **WHY AI GETS IT WRONG**: writes `[*c]u8` everywhere "because it's C", losing alignment
  safety and optionality clarity; or uses a normal pointer where the C API means "0 is
  allowed" without making it optional.
- **CORRECT REASONING**: translate-c emits C pointers; hand-written code uses Zig pointer
  types and spells nullability with `?`. Dereferencing address 0 is safety-checked
  Illegal Behavior on non-freestanding targets.
- **EXAMPLE** (bad):
  ```zig
  extern "c" fn strlen(s: [*c]const u8) usize; // works, but opaque about nullability
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  extern "c" fn strlen(s: [*:0]const u8) usize; // sentinel-terminated, non-null
  extern "c" fn getenv(name: [*:0]const u8) ?[*:0]const u8; // nullable via ?
  ```
- **VERIFICATION**: `zig test -lc` with both declarations; C pointers accepted, normal
  pointers give stronger checks.
- **SOURCE**: zig-langref §C (C Pointers).

## 6. Linking libc and system libraries

- **RULE**: `zig test foo.zig -lc` links libc; in build.zig set `.link_libc = true` in the
  module or `exe.root_module.linkSystemLibrary("z", .{})` for system libs, `linkLibrary`
  for Zig-built libs, `addCSourceFile` for vendored C sources.
- **WHY AI GETS IT WRONG**: declares `extern "c" fn printf(...)` and tests without `-lc`,
  producing unresolved-symbol errors the agent blames on Zig.
- **CORRECT REASONING**: extern declarations need the symbol at link time; the linker must
  know where. `-lc`/`link_libc` is the standard way; `std.c` (zig-std-source) provides
  libc declarations when linked.
- **EXAMPLE** (bad):
  ```zig
  pub extern "c" fn printf(format: [*:0]const u8, ...) c_int;
  test "printf" { _ = printf("hi\n"); }   // needs -lc at the test command
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  pub extern "c" fn printf(format: [*:0]const u8, ...) c_int;
  test "printf" { _ = printf("hi\n"); }   // zig test -lc
  ```
- **VERIFICATION**: `zig test examples/good/export_lib.zig -lc` passes; without `-lc`
  the link fails with unresolved `printf`.
- **SOURCE**: zig-langref §C (Mixing Object Files); zig-build-guide (Linking to System
  Libraries); zig-std-source (std/c.zig).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| C structs | `extern struct` (C layout); `packed struct` is bit-packing, not C |
| C void | `anyopaque` |
| C scalars | `c_int`, `c_long`, ... — target-correct sizes; `c_long` is 32-bit on Windows |
| Variadics | `(count: c_int, ...) callconv(.c)`; `@cVaStart` + `@cVaArg` + `defer @cVaEnd` |
| C imports | `@cImport` deprecated (0.16) → `addTranslateC`/`zig translate-c` |
| translate-c | same `-target` and `-cflags` as the build, or ABI drifts |
| C pointers | `[*c]T` from translate-c; hand-written code uses `*T`/`[*]T`/`?*T` |
| Link | `-lc` / `link_libc`; `linkSystemLibrary`; `addCSourceFile`; `linkLibrary` |
| Export | `export fn` = C ABI; `@export` needs `callconv(.c)` |
