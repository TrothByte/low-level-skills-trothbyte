# Zig Version Migration — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.
Version markers: KNOWN / INFERRED / UNVERIFIED. Reference versions: 0.15.0 (2025-07),
0.16.0 (2026-04-13), master 0.17.0-dev (2026-08).

## 1. The Writergate: std.io → std.Io non-generic buffered streams

- **RULE**: 0.15.0 replaced the generic `std.io.Reader/Writer` with non-generic
  `std.Io.Reader/Writer` that carry the buffer above the vtable. Buffers live in the
  interface; `std.fs.File.stdout().writer(&buf)` returns a writer with an `.interface`;
  you must `.flush()`.
- **WHY AI GETS IT WRONG**: pastes `var bw = std.io.bufferedWriter(std.out.writer()); try
  bw.writer().print(...); try bw.flush();` (0.14-era) — `std.io` is gone and
  `BufferedWriter` was deleted.
- **CORRECT REASONING**: new contract:
  ```zig
  var stdout_buffer: [4096]u8 = undefined;
  var stdout_writer = std.fs.File.stdout().writer(&stdout_buffer);
  const stdout = &stdout_writer.interface;
  try stdout.print("hi {s}\n", .{"there"});
  try stdout.flush();
  ```
  Old streams expose `adaptToNewApi(&.{})`. CountingWriter/BufferedWriter/LinearFifo/
  RingBuffer/BoundedArray were deleted; `std.Io.Writer.Discarding`/`.Allocating`/`.fixed`
  cover the counting/fixed-buffer roles.
- **EXAMPLE** (bad, fails on 0.16+):
  ```zig
  const stdout_file = std.fs.File.stdout().writer();
  var bw = std.io.bufferedWriter(stdout_file);
  try bw.writer().print("...", .{});
  try bw.flush();
  ```
- **COUNTEREXAMPLE** (good, 0.16):
  ```zig
  var buf: [1024]u8 = undefined;
  var w = std.fs.File.stdout().writer(&buf);
  try w.interface.print("...", .{});
  try w.interface.flush();
  ```
- **VERIFICATION**: `zig test examples/bad/io_015_style.zig` fails with a
  missing-`std.io` error; `zig test examples/good/io_016.zig` passes.
- **SOURCE**: zig-release-notes 0.15.1 (Writergate: Motivation, Adapter API, New
  std.Io.Writer and std.Io.Reader API, Upgrading std.io.getStdOut().writer().print(),
  CountingWriter/BufferedWriter Deleted); zig-langref master (std.Io examples).

## 2. "{f}" is required to call format methods

- **RULE**: since 0.15, printing a value whose type has a `format` method requires the
  `{f}` specifier; bare `{}` is a compile error there. `{any}` skips the method. Format
  methods now have signature `pub fn format(self: @This(), writer: *std.Io.Writer)
  std.Io.Writer.Error!void` — no format string, no options.
- **WHY AI GETS IT WRONG**: writes `std.debug.print("{}", .{std.zig.fmtId("x")})` and
  gets "ambiguous format string; specify {f}" — then "fixes" it by removing the format
  method or by `{any}` when the intent was formatting.
- **CORRECT REASONING**: `{f}` always calls the format method; `{any}` never does;
  `{}` refuses to guess. State from the old options param moves into helper wrappers like
  `std.fmt.Alt`.
- **EXAMPLE** (bad):
  ```zig
  std.debug.print("{}", .{std.zig.fmtId("example")}); // compile error since 0.15
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  std.debug.print("{f}", .{std.zig.fmtId("example")});
  ```
- **VERIFICATION**: `zig test` on the pair; `-freference-trace` finds all breakage sites.
- **SOURCE**: zig-release-notes 0.15.1 ("{f}" Required to Call format Methods; Format
  Methods No Longer Have Format Strings or Options; New Formatted Printing Specifiers).

## 3. Juicy Main: main may take std.process.Init; environment is non-global

- **RULE**: since 0.16.0, `main` may take one of: no parameters, `std.process.Init.Minimal`
  (argv/environ), or `std.process.Init` (Minimal + arena, gpa, io, environ_map, preopens).
  Environment variables and args are no longer global state; `std.os.environ` is gone as a
  general mechanism.
- **WHY AI GETS IT WRONG**: writes `pub fn main() void` and then reads `std.os.environ`
  or calls `std.process.argsAlloc` from anywhere; or claims argv is available in a
  bare-main program.
- **CORRECT REASONING**: thread the needed values through parameters. A bare main is still
  legal but cannot see argv/env. Functions that need env values accept `*const
  std.process.Environ.Map`.
- **EXAMPLE** (bad, fails on 0.16+):
  ```zig
  const std = @import("std");
  pub fn main() void {
      const argv = std.process.argsAlloc(std.heap.page_allocator) catch return;
      _ = argv; // 0.16: args are only reachable via the Init parameter
  }
  ```
- **COUNTEREXAMPLE** (good, 0.16):
  ```zig
  const std = @import("std");
  pub fn main(init: std.process.Init) !void {
      const args = try init.minimal.args.toSlice(init.arena.allocator());
      std.log.info("argc={d}", .{args.len});
  }
  ```
- **VERIFICATION**: `zig test examples/bad/main_old_global_env.zig` fails on 0.16+;
  `zig test examples/good/main_016.zig` passes.
- **SOURCE**: zig-release-notes 0.16.0 ("Juicy Main"; Environment Variables and Process
  Arguments Become Non-Global); zig-langref master (Hello World; Entry Point).

## 4. @Type split (0.16.0) and @cImport deprecation (0.16.0)

- **RULE**: 0.16.0 removed `@Type` (proposal #10710), adding `@Int`, `@Struct`, `@Union`,
  `@Enum`, `@Pointer`, `@Fn`, `@Tuple`, `@EnumLiteral`; `std.meta.Int`/`std.meta.Tuple`
  are deprecated. 0.16.0 also deprecated `@cImport` in favor of build-system
  `addTranslateC`.
- **WHY AI GETS IT WRONG**: reifies with `@Type(.{...})` or `std.meta.Int` on 0.16+; keeps
  `const c = @cImport({ @cInclude("stdio.h"); });` and is surprised translate-c moved.
- **CORRECT REASONING**: `@Int(.unsigned, 10)` replaces both `@Type(.{ .int = ... })` and
  `std.meta.Int(.unsigned, 10)`. C imports: `zig translate-c src/c.h` at build time,
  expose as a module import named `c` (or use the official translate-c package).
- **EXAMPLE** (bad, fails on 0.16+):
  ```zig
  const U = @Type(.{ .int = .{ .signedness = .unsigned, .bits = 10 } }); // @Type removed
  const c = @cImport(@cInclude("stdio.h"));                              // deprecated
  ```
- **COUNTEREXAMPLE** (good, 0.16):
  ```zig
  const U = @Int(.unsigned, 10);
  // build.zig: const translate_c = b.addTranslateC(.{ .root_source_file = b.path("src/c.h") });
  //            then add translate_c.createModule() as import "c"
  ```
- **VERIFICATION**: `zig test examples/bad/type_015_syntax.zig` (see
  zig-comptime-metaprogramming) and review of build.zig for `addTranslateC`.
- **SOURCE**: zig-release-notes 0.16.0 (@Type Replaced with Individual Type-Creating
  Builtin Functions; @cImport Moving to Build System); zig-langref (C Translation CLI).

## 5. Containers became unmanaged by default (0.15→0.16)

- **RULE**: 0.15 made `std.ArrayList` default to unmanaged (`std.ArrayList(T) = .empty`,
  allocator per call); 0.16 continued the migration ("Managed" gone; `ArrayHashMap` etc.
  renamed to unmanaged variants; `PriorityQueue`/`PriorityDequeue` lost the allocator
  field). `init(gpa)` on the plain `std.ArrayList(T)` no longer exists.
- **WHY AI GETS IT WRONG**: writes `var list = std.ArrayList(i32).init(gpa); defer
  list.deinit();` from 0.14 memory — the 0.16 build guide's own example fails with
  "struct 'array_list.Aligned(i32,null)' has no member named 'init'".
- **CORRECT REASONING**: `var list: std.ArrayList(i32) = .empty; defer list.deinit(gpa);
  try list.append(gpa, 42);`. For managed behavior, keep the allocator in your own struct
  and pass it through.
- **EXAMPLE** (bad):
  ```zig
  var list = std.ArrayList(i32).init(gpa);
  defer list.deinit();
  try list.append(42);
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  var list: std.ArrayList(i32) = .empty;
  defer list.deinit(gpa);
  try list.append(gpa, 42);
  ```
- **VERIFICATION**: `zig build test` — bad fails at compile time with a "no member named
  'init'" diagnostic (exact text UNVERIFIED on this host).
- **SOURCE**: zig-release-notes 0.15.1 (ArrayList: make unmanaged the default) and 0.16.0
  (Migration to "Unmanaged" Containers); zig-build-guide (Testing section — the failing
  example is the documentation itself).

## 6. Version pinning methodology

- **RULE**: pin the compiler per project: `zigup` (community version manager, KNOWN;
  INFERRED as the de-facto standard) or direct downloads from ziglang.org/download with
  signatures; record `minimum_zig_version` in `build.zig.zon`; keep the version-specific
  langref open (0.15.2 / 0.16.0 links on the download page).
- **WHY AI GETS IT WRONG**: targets "latest Zig" with no pin, then writes APIs that exist
  in 0.17-dev but not 0.16.0 (or the reverse), and marks version-sensitive claims as KNOWN.
- **CORRECT REASONING**: the std API drifts between 0.15, 0.16 and 0.17-dev. Every claim
  gets a version marker: KNOWN (verified in a specific release's docs/notes), INFERRED
  (derived), UNVERIFIED (not run). `zig version` and `@import("builtin").zig_version`
  confirm the toolchain.
- **EXAMPLE** (bad): `zig build -Dtarget=...` on an un-pinned toolchain while the code
  requires 0.16.0's `std.process.Init` — builds non-reproducibly.
- **COUNTEREXAMPLE** (good):
  ```zig
  // build.zig.zon
  .{
      .name = .my_pkg,
      .version = "0.1.0",
      .minimum_zig_version = "0.16.0",
      .paths = .{ "build.zig", "build.zig.zon", "src" },
  }
  ```
  ```powershell
  zigup 0.16.0
  zig version   # 0.16.0
  zig build test
  ```
- **VERIFICATION**: `zigup list` shows the pinned 0.16.0; `zig version` matches;
  `zig build test` succeeds with the pinned compiler and fails (documented) on 0.15.2 for
  `std.process.Init` code.
- **SOURCE**: zig-build-guide (build.zig.zon and standard options); zig-release-notes
  (version timeline in the download page is the primary timeline source, KNOWN:
  0.15.1 2025-08-19, 0.15.2 2025-10-11, 0.16.0 2026-04-13).

## Quick reference table

| Topic | 0.15 | 0.16+ |
|---|---|---|
| Streams | `std.io` deprecated; `std.Io.Writer` buffered | `std.Io` only; `.interface`, `.flush()` |
| Format methods | `{f}` required; no format string/options | same |
| main | `pub fn main() void` (no env) | `main(init: std.process.Init)` / `.Minimal` |
| Reification | `@Type(.{...})`, `std.meta.Int` | `@Int`/`@Struct`/`@Union`/`@Enum`/`@Pointer`/`@Fn`/`@Tuple` |
| C imports | `@cImport` | deprecated → `addTranslateC` in build.zig |
| Containers | unmanaged default (0.15) | fully unmanaged; `ArrayHashMap`→`array_hash_map.*` |
| async/await | removed (0.15) | std.Io interface instead |
| Inline asm clobbers | typed `.{ .rcx = true }` (0.15) | same |
| Allocators | `GeneralPurposeAllocator` | `DebugAllocator` / `smp_allocator` (master) |
