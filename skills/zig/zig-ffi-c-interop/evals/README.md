# Evaluation — zig-ffi-c-interop

Skill: `skills/zig/zig-ffi-c-interop`.
Stability target: `researched`. Toolchain: zig is NOT installed on this host; the code
targets the 0.15–0.17 API surface (verified against the langref C section and the 0.16.0
release notes). Verification commands below are the recorded plan, not run results.

## Synthetic evals

| Case | Fixture | Expected | Command |
|---|---|---|---|
| easy/negative | `bad/packed_for_c.zig` | `packed struct` for a C struct flagged; size mismatch | `zig test` |
| easy/negative | `bad/c_long_assumption.zig` | ABI mistake on Windows flagged | `zig test -target x86_64-windows-gnu` |
| medium/negative | review | `@cImport` used in 0.16+ code | review |
| medium/negative | review | translate-c `-target`/`-cflags` mismatch | review |
| positive | `good/extern_struct.zig` | passes; `@sizeOf`/`@offsetOf` checks | `zig test` |
| positive | `good/export_lib.zig` | passes with `-lc`; variadic + export | `zig test -lc` |

## False-positive evals (correct code must not be flagged)

- `good/extern_struct.zig` — `extern struct` with an explicit `reserved` padding field and
  `@sizeOf`/`@offsetOf` comptime checks is the correct C-layout pattern.
- `good/export_lib.zig` — `export fn` (C ABI) and a variadic `callconv(.c)` with
  `defer @cVaEnd(&ap)` are correct.
- `[*:0]const u8` sentinel-terminated and `?[*:0]const u8` nullable declarations are the
  preferred hand-written spelling of C strings.
- `linkSystemLibrary("z", .{})` / `link_libc = true` in build.zig — correct linking.

## Historical evals

- 0.16.0 `@cImport` → `addTranslateC` migration: the upgrade guide (c.zig → build.zig +
  c.h) is the regression target; the deprecated form must be rewritten.
- 0.15 Writergate changed `std.io.getStdOut()` — C interop tutorials mixing eras are the
  classic agent trap (see `zig-version-migration`).

## Adversarial evals

- A header translated with the wrong `-cflags` (e.g. without `-fshort-enums`) that
  produces subtly wrong enum sizes — must be caught by re-translating with matching flags,
  not by patching the generated Zig by hand.
- An `extern struct` whose padding is "fixed" by inserting Zig-specific `@align` — must be
  caught by `@sizeOf`/`@offsetOf` checks against the C compiler.
- A `c_long` assumption verified only on Linux — the Windows `-target` run must fail.

## Verified facts

- KNOWN (from langref §C and 0.16.0 release notes; not run on this host):
  - `c_*` primitives guarantee C ABI compatibility; `anyopaque` is C `void`.
  - `extern struct` uses C layout; `packed struct` is bit-packing.
  - 0.16.0 deprecated `@cImport`; build-system `addTranslateC` is the replacement.
  - Variadics use `callconv(.c)` and `@cVaStart`/`@cVaArg`/`@cVaEnd` (with `defer
    @cVaEnd`).
  - `zig translate-c` must use the same `-target` and `-cflags` as the build.
  - C pointers `[*c]T` come from translate-c; hand-written code prefers normal pointers.
- INFERRED: `c_long` is 32-bit on Windows x64 (LLP64) — standard MSVC ABI fact; exact
  `@sizeOf` values are target-dependent (the example asserts and fails loudly).
- UNVERIFIED (needs zig on this host): exact diagnostic text for the bad fixtures.

## Target toolchains (absent, documented)

- zig 0.15.2 / 0.16.0 / 0.17.0-dev: not installed. A C compiler (`cc`) is needed to
  double-check `extern struct` layouts for the adversarial evals. First execution plan:
  install zig, then run the commands in SKILL.md §How to verify.
