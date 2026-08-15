# Evaluation — zig-version-migration

Skill: `skills/zig/zig-version-migration`.
Stability target: `researched`. Toolchain: zig is NOT installed on this host; the code
targets the 0.15–0.17 API surface (verified against the 0.15.x and 0.16.0 release notes and
the langref). Verification commands below are the recorded plan, not run results.

## Synthetic evals

| Case | Fixture | Expected | Command |
|---|---|---|---|
| easy/negative | `bad/io_015_style.zig` | fails on 0.16+: `std.io` removed, `bufferedWriter` gone | `zig test` |
| easy/negative | `bad/main_old_global_env.zig` | fails on 0.16+: args/env non-global | `zig test` |
| medium/negative | review | `@Type(.{...})` and `std.meta.Int` on 0.16+ | review + `zig test` |
| medium/negative | review | `{f}` missing at a format-method call site | `zig test` |
| positive | `good/io_016.zig` | passes; std.Io writer + flush | `zig test` |
| positive | `good/main_016.zig` | passes; Juicy Main with Init | `zig test` |

## False-positive evals (correct code must not be flagged)

- `good/main_016.zig` — `main(init: std.process.Init) !void` with `init.gpa`/`init.io`/
  `init.minimal.args` is the documented 0.16 pattern.
- `good/io_016.zig` — buffered `std.fs.File.stdout().writer(&buf)` with `flush()` is the
  documented replacement for BufferedWriter.
- `{f}` on a type with a format method and `{any}` on a type without one are both correct
  on 0.15+.
- `@Int(.unsigned, 10)` on 0.16+ is correct, not a "renamed @Type".

## Historical evals

- 0.15.0 Writergate: migrate `std.io.getStdOut().writer().print()` → buffered
  `std.Io.Writer` + `flush()`; the deleted CountingWriter/BufferedWriter/LinearFifo/
  RingBuffer/BoundedArray must be replaced with `std.Io.Writer.Discarding`/`.Allocating`/
  `.fixed` and unmanaged containers.
- 0.16.0 Juicy Main: `std.os.environ` global access → `Init` parameter.
- 0.16.0 `@Type` → split builtins; `@cImport` → build-system translate-c.
- 0.15/0.16 unmanaged containers: `.init(gpa)` no longer exists on `std.ArrayList`.

## Adversarial evals

- A snippet that compiles on pinned 0.15.2 but fails on 0.16.0, where the agent must name
  the actual breaking change (e.g. `std.os.environ`) instead of "fixing" the build by
  deleting the `minimum_zig_version` field or downgrading silently.
- A claim of API availability on a version range where the API differs — the agent must
  mark it KNOWN/INFERRED/UNVERIFIED and pin the version.

## Verified facts

- KNOWN (from release notes and download page; not run on this host):
  - 0.15.1 released 2025-08-19, 0.15.2 2025-10-11, 0.16.0 2026-04-13; master
    0.17.0-dev 2026-08.
  - Writergate: `std.Io.Reader/Writer` are non-generic with the buffer above the vtable;
    `std.fs.File.Writer` memoizes stat size/seek position; `adaptToNewApi` bridges old
    streams; `{f}` required for format methods.
  - Juicy Main: `std.process.Init` carries minimal/arena/gpa/io/environ_map/preopens;
    env and args are only available through the Init parameter.
  - 0.16.0 removed `@Type` (#10710) and deprecated `@cImport`.
  - The build-guide testing example itself demonstrates the unmanaged-container breakage
    (`no member named 'init'`), confirming the 0.16 default.
- UNVERIFIED (needs zig on this host): exact compile-error text for `bad/io_015_style.zig`
  and `bad/main_old_global_env.zig`; `zigup` exact CLI (INFERRED from community usage).

## Target toolchains (absent, documented)

- zig 0.15.2 / 0.16.0 / 0.17.0-dev: not installed. First execution plan: `zigup` or
  ziglang.org/download, then run the commands in SKILL.md §How to verify against each
  pinned version.
