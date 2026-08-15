---
name: zig-version-migration
description: Use when migrating Zig code across 0.15-0.17: Writergate std.io-to-std.Io changes, Juicy Main and non-global environment, @Type split, unmanaged containers, @cImport deprecation, and version pinning with build.zig.zon and zigup. Prevents pasting stale APIs and mis-attributing migration errors. Version-pinned.
---

# Zig Version Migration

## When to use

- Upgrading a Zig project or a snippet from 0.15 to 0.16 (or 0.16 to 0.17-dev) and
  triaging compile errors.
- Reviewing code that mixes API generations (e.g. `std.io.getStdOut().writer().print()` on
  0.16+, `@Type` on 0.16+).
- Deciding how to pin the compiler (zigup / download page) and encode the minimum version
  in `build.zig.zon`.
- Determining whether a claim about the std API is still true for the pinned version.

## When not to use

- Core language semantics that are stable across versions — use the topic skills
  (comptime, errors, allocators).
- C/C++ toolchain drift — see `build-toolchain-version-drift` (build-systems domain).
- Zig 0.12- and 0.13-era code — the delta list here starts at the 0.15.0 breaking changes;
  earlier migrations need the 0.14/0.15 notes.

## What the agent often gets wrong

- Pasting the pre-0.15 `std.io.getStdOut().writer().print()` idiom into 0.16+ code and
  calling the failure a compiler bug.
- Forgetting the `{f}` rule: types with a `format` method now require `{f}`; `{}` is a
  compile error at those sites (0.15 Writergate).
- Writing `pub fn main() void` with `std.os.environ` — 0.16 removed global environment
  access; env vars arrive via the `std.process.Init` parameter.
- Using `@Type(.{...})` for reification on 0.16+, or `std.meta.Int` (deprecated) instead
  of `@Int`.
- Calling `std.ArrayList(i32).init(gpa)` — 0.15 made containers unmanaged by default;
  the type is constructed `.empty` and the allocator is passed per method.
- Keeping `@cImport` in the root module on 0.16+ where it is deprecated in favor of
  build-system `translate-c`.
- Blaming Zig for "regressions" that are documented migrations (the release notes are the
  authority, and version-sensitive claims must be marked).

## How to reason correctly

1. Identify the pinned version first. The supported range here is 0.15.0 (2025-07 era),
   0.16.0 (2026-04), and 0.17.0-dev (master, 2026-08). Use `zig version` and the
   `@import("builtin").zig_version` compile constant to verify.
2. For each compile error, map it to the release-notes section of the version you are
   moving TO: Writergate and `{f}` (0.15.0), Juicy Main / `@Type` split / `@cImport`
   deprecation / unmanaged containers (0.16.0), allocator renames (0.17-era langref).
3. Migrate I/O to the new contract: create a buffered writer with
   `std.fs.File.stdout().writer(&buf)`, use `.interface`, call `.print`/`.writeAll`, and
   `.flush()` — "Don't forget to flush".
4. Migrate main: `pub fn main(init: std.process.Init) !void` (or `.Minimal`) when the
   program needs argv/environ/allocators/io; a bare `main() void` is still legal but
   cannot see environment variables.
5. Reify types with `@Int`/`@Struct`/`@Union`/`@Enum`/`@Pointer`/`@Fn`/`@Tuple`.
6. Pin the toolchain: `zigup` for per-project versions, `minimum_zig_version` in
   `build.zig.zon`, and check the version-specific langref (0.15.2 / 0.16.0) before
   trusting a std API.

## What to verify

- Source contains no pre-0.15 I/O (`std.io.`), no `@Type`, no `std.os.environ`,
  no managed-container `.init(gpa)`.
- Every `.print`/`.format` call uses the correct specifier (`{f}` where a format method
  exists, `{s}` for strings, `{d}` for numbers, `{t}` for tags).
- `main` reads env/args only through `std.process.Init(.Minimal)`.
- `build.zig.zon` sets `minimum_zig_version`; toolchain pinned via `zigup`.
- Migration compiles with the exact pinned compiler, not a "works in head" version.

## How to verify

```
zigup list                     # show installed versions
zigup 0.16.0                   # pin
zig version                    # must print 0.16.0
zig test examples/good/main_016.zig
zig test examples/good/io_016.zig
zig test examples/bad/io_015_style.zig    # fails on 0.16+: std.io removed
zig test examples/bad/main_old_global_env.zig  # fails on 0.16+: std.os.environ gone
zig build test
```

Researched — zig not installed on this host; commands are the recorded verification plan.

## Where the knowledge comes from

- zig-release-notes 0.15.x ("Writergate", `{f}` Required to Call format Methods, async
  and await removed, Inline Assembly typed clobbers, ArrayList unmanaged default) and
  0.16.0 ("Juicy Main", Environment Variables Become Non-Global, @Type Replaced, @cImport
  Moving to Build System, Migration to Unmanaged Containers).
- zig-langref master + 0.16.0 docs (Hello World and std.Io examples; @Int/@Struct/@Union/
  @Enum/@Pointer/@Fn/@Tuple/@EnumLiteral).
- zig-build-guide (build.zig.zon conventions; standard options).

## Related skills

- `zig-comptime-metaprogramming` — `@Type` split details (skill owns the reification rules).
- `zig-ffi-c-interop` — `@cImport` deprecation → translate-c in the build system.
- `zig-inline-asm-and-abi` — typed clobbers (0.15 breaking change).
- `zig-allocators-and-memory-management` — unmanaged containers; allocator renames.
- `zig-build-system-and-packages` — `minimum_zig_version`, package pinning.

## Evaluation

- Synthetic: `std.io` idioms, `@Type`, `std.os.environ`, managed `.init(gpa)`, and
  missing-`{f}` fixtures must be caught; 0.16-style `std.Io` + Juicy Main must pass.
- False-positive: correct 0.16 `main(init: std.process.Init)`, `std.Io.Writer` usage with
  flush, `@Int` reification must NOT be flagged.
- Historical: the 0.15.0 Writergate and 0.16.0 Juicy Main/`@Type`/`@cImport` migrations
  are the regression targets; fixes must follow the release-notes upgrade guides.
- Adversarial: a snippet that compiles under a pinned 0.15.2 but not 0.16.0, where the
  agent must identify the actual breaking change rather than "fix" the build by loosening
  `minimum_zig_version`.
- Commands and recorded results: `evals/README.md`.
