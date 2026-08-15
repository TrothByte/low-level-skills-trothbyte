---
name: zig-ffi-c-interop
description: Use when bridging Zig and C: extern struct layout, C type primitives, @cImport/translate-c, extern fn and callconv(.c), std.c, variadics, and linking C libraries. Prevents ABI-layout mistakes, wrong C type sizes, and pre-0.16 @cImport habits. Version-pinned to Zig 0.15-0.17.
---

# Zig FFI / C Interop

## When to use

- Calling C libraries from Zig (`extern fn`, `extern struct`, variadics, `std.c`).
- Exporting Zig code as a C library (`export fn`, `zig build-lib`).
- Importing C headers (`zig translate-c`, or 0.16+ build-system `addTranslateC`).
- Reviewing ABI assumptions across the Zig/C boundary: layout, types, calling convention.

## When not to use

- Pure Zig with no C boundary — no `extern`/`export` needed.
- C++ interop (name mangling, vtables) — Zig translates C, not C++; keep C++ behind an
  extern "C" wrapper.
- Cross-language ownership/FFI design — see `ffi-boundary-cross-language`.
- Layout reasoning independent of C — see `abi-layout-reasoning` and `zig-inline-asm-and-abi`.

## What the agent often gets wrong

- Using `packed struct` to represent a C struct — C structs have target-defined padding;
  use `extern struct` (or `@align`/padding explicitly). `packed struct` changes layout
  and bit packing semantics.
- Assuming `c_int`/`c_long` sizes: `c_long` is 32-bit on Windows (LLP64) and 64-bit on
  Linux/macOS; use the `c_*` primitives, never `i32`/`i64` guesses.
- Using `i32`/`i64`/`bool` in `extern struct` or exported function signatures where C
  expects `int`/`long`/`_Bool` (whose ABI sizes match only on some targets).
- Keeping `@cImport({ @cInclude(...) })` in the root module on 0.16+, where it is
  deprecated in favor of build-system `translate-c`.
- Forgetting to link libc: `zig test -lc`, or `link_libc = true` in build.zig, or
  `linkSystemLibrary("z", .{})` for third-party libs.
- Translating with a different `-target`/`-cflags` than the compile step — causes ABI
  mismatches (e.g. `-fshort-enums` changes enum sizes).
- Treating `[*c]T` as a regular pointer — it allows 0 and coerce-from-int; prefer normal
  pointers in hand-written code, keep C pointers in generated code.

## How to reason correctly

1. Pick the boundary types: use the `c_*` primitives for C scalar params/returns,
   `anyopaque` for `void`, `extern struct` for C structs, `[*c]T` only where
   translate-c produces it.
2. For C structs, declare `extern struct` (C layout: fields in order, target padding).
   For bitfields/packed layout use explicit integers with masks, because bitfields do not
   translate (demotion).
3. Declare C functions with `extern "c" fn name(...) ...;` (callconv(.c) implied by the
   `export`/`extern` keywords' C linkage when spelled `extern "c"`); variadics as
   `(a: c_int, ...)` read with `@cVaStart`/`@cVaArg`/`@cVaEnd`.
4. Import headers with `zig translate-c -target <same target> -cflags <same flags> --
   header.h` or (0.16+) `b.addTranslateC(...)`; expect demotion of untranslatable
   constructs to `opaque {}`/`extern`/`@compileError`.
5. Link: `-lc` for libc, `linkSystemLibrary("name", .{})` for system libs,
   `addCSourceFile` for vendored C, `linkLibrary` for Zig-built libs.
6. Export with `export fn` (implies C ABI) or `@export` of a `callconv(.c)` function;
   generate the header via the build system if needed.

## What to verify

- All boundary types are `c_*` primitives or `extern`-qualified types; no `packed` where C
  uses normal layout.
- `translate-c`/`@cImport` used with the same `-target` and `-cflags` as the build.
- Libc/system libs linked (`-lc`, `link_libc`, `linkSystemLibrary`).
- Function pointers and structs across the boundary use C layout; no Zig auto-reordering.
- 0.16+ code does not use `@cImport` (deprecated); build.zig uses `addTranslateC`.
- Cross-check sizes with `@sizeOf`/`@offsetOf` tests on the pinned target.

## How to verify

```
zig test examples/good/extern_struct.zig
zig test examples/good/export_lib.zig -lc
zig build-lib examples/good/export_lib.zig
zig translate-c examples/good/simple.h -target x86_64-linux-gnu
zig test examples/bad/c_long_assumption.zig -target x86_64-windows-gnu   # should be flagged
zig build test     # project with addTranslateC / linkSystemLibrary
```

Researched — zig not installed on this host; commands are the recorded verification plan.

## Where the knowledge comes from

- zig-langref §C (C Type Primitives, C Translation CLI, C Pointers, C Variadic
  Functions, Exporting a C Library, Mixing Object Files), §struct (extern struct),
  §Builtin Functions (@cVaStart/@cVaArg/@cVaEnd/@cVaCopy).
- zig-release-notes 0.16.0 (@cImport Moving to Build System).
- zig-std-source (std/c.zig — libc declarations).
- zig-build-guide (Linking to System Libraries; C integration).

## Related skills

- `ffi-boundary-cross-language` — cross-language boundary design (recommends this skill).
- `abi-layout-reasoning` — struct layout/padding rules that extern struct inherits.
- `zig-build-system-and-packages` — `addTranslateC`, `addCSourceFile`,
  `linkSystemLibrary`, `link_libc`.
- `zig-inline-asm-and-abi` — `callconv(.c)`, `@export`/`@extern` at the ABI level.
- `zig-version-migration` — `@cImport` deprecation in 0.16.

## Evaluation

- Synthetic: `packed` vs `extern` for C structs, `c_long`/`c_int` size assumptions,
  missing libc link, mismatched translate-c target, pre-0.16 `@cImport` — must be caught;
  good extern-struct/export examples must pass.
- False-positive: correct `extern struct` with explicit padding, `c_*` types, `@cVaStart`
  variadics, and build.zig `linkSystemLibrary` must NOT be flagged.
- Historical: 0.16.0 `@cImport` → `addTranslateC` migration is the regression target.
- Adversarial: a header translated with the wrong `-cflags` that produces subtly wrong
  enum/struct sizes — catchable only by target-consistent re-translation.
- Commands and recorded results: `evals/README.md`.
