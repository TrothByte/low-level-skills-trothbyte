# Zig Cross-Compilation Targets — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.
Version markers: KNOWN / INFERRED / UNVERIFIED.

## 1. Default target is the host; cross-compilation is explicit

- **RULE**: with no `-target`/`-Dtarget`, Zig targets the host computer; the resulting
  executable is "unsuitable for copying to a different computer". The target is composed
  of CPU architecture, CPU features, OS, OS version, ABI, and ABI version.
- **WHY AI GETS IT WRONG**: ships a host-built binary and calls it a release artifact;
  or believes `zig build` defaults to `x86_64-linux-gnu` regardless of host.
- **CORRECT REASONING**: state the deployment target explicitly. `zig targets` lists all
  targets the compiler knows; `zig build -Dtarget=<triple>` and `zig build-exe -target
  <triple>` select it.
- **EXAMPLE** (bad):
  ```zig
  // build.zig — release CI on x86_64-linux
  .target = b.graph.host,   // artifact will not run on the deployment ARM boxes
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  const target = b.standardTargetOptions(.{});   // zig build -Dtarget=aarch64-linux-gnu
  .target = target,
  ```
- **VERIFICATION**: `zig build -Dtarget=aarch64-linux-gnu --summary all` produces an
  ARM64 artifact from an x86_64 host.
- **SOURCE**: zig-langref §Targets.

## 2. Target triples and ABI suffixes

- **RULE**: triples are `arch-os-abi` (e.g. `x86_64-linux-gnu`, `aarch64-linux-musl`,
  `x86_64-windows-gnu`, `wasm32-wasi`). The ABI suffix matters: arm hard-float is
  `arm-linux-gnueabihf`; aarch64 uses `gnu` (no hf variant). `zig targets` is the
  authoritative spelling table.
- **WHY AI GETS IT WRONG**: writes `aarch64-linux-gnueabihf` (copied from 32-bit arm) or
  omits the ABI and gets `gnu` vs `musl` surprises.
- **CORRECT REASONING**: consult `zig targets` (and `zig targets --help` for the output
  structure) before writing a triple. For C sources, translate-c and the final build must
  share the same triple.
- **EXAMPLE** (bad):
  ```
  zig build-exe main.zig -target aarch64-linux-gnueabihf   # invalid combo
  ```
- **COUNTEREXAMPLE** (good):
  ```
  zig build-exe main.zig -target aarch64-linux-gnu
  zig build-exe main.zig -target aarch64-linux-musl
  ```
- **VERIFICATION**: both good commands compile; the bad one is rejected or produces a
  target mismatch (exact diagnostic UNVERIFIED on this host).
- **SOURCE**: zig-langref §Targets, §C (C Translation CLI — `varytarget.h` shows c_long
  changing with the triple).

## 3. std.Target.Query and standardTargetOptions

- **RULE**: `std.Target.Query` describes a target by fields (`.cpu_arch = .x86_64,
  .os_tag = .linux`); `b.resolveTargetQuery(query)` resolves it to a `ResolvedTarget`;
  `b.standardTargetOptions(.{})` exposes `-Dtarget`/`-Dcpu`. The build-guide's
  multi-target test example is the reference pattern.
- **WHY AI GETS IT WRONG**: hardcodes `b.graph.host`; writes triples as strings inside
  build.zig instead of using the enum fields.
- **CORRECT REASONING**: use `std.Target.Query{ .cpu_arch = ..., .os_tag = ... }` for
  programmatic target lists, `standardTargetOptions` for user-facing `-Dtarget`.
- **EXAMPLE** (bad):
  ```zig
  // build.zig — hardcodes the test matrix to the host
  const test_targets = [_]std.Target.Query{.{}};
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  const test_targets = [_]std.Target.Query{
      .{}, // native
      .{ .cpu_arch = .x86_64, .os_tag = .linux },
      .{ .cpu_arch = .aarch64, .os_tag = .macos },
  };
  for (test_targets) |q| {
      const unit_tests = b.addTest(.{ .root_module = b.createModule(.{
          .root_source_file = b.path("main.zig"),
          .target = b.resolveTargetQuery(q),
      })});
      const run = b.addRunArtifact(unit_tests);
      run.skip_foreign_checks = true; // don't fail when host can't execute
      test_step.dependOn(&run.step);
  }
  ```
- **VERIFICATION**: `zig build test --summary all` builds all matrix targets and skips
  foreign execution.
- **SOURCE**: zig-build-guide (Testing — test_targets + skip_foreign_checks);
  zig-std-source (std/Target.zig).

## 4. zig cc is a cross-compiler

- **RULE**: `zig cc`/`zig c++` are Clang-based (0.16: Clang 21.1.8) and cross-compile C/C++
  with `-target`; Zig bundles libc: glibc 2.43, musl 1.2.5, MinGW-w64, WASI libc (0.16.0),
  plus headers — no system cross-toolchain required.
- **WHY AI GETS IT WRONG**: invokes `zig cc` without `-target` expecting a portable binary,
  or claims a Windows cross-build needs MSVC.
- **CORRECT REASONING**: `zig cc -target x86_64-windows-gnu hello.c -o hello.exe` links
  against Zig's MinGW; `zig cc -target aarch64-linux-musl -static` gives a static musl
  binary. `zig cc --version` reports the Clang version.
- **EXAMPLE** (bad):
  ```
  zig cc hello.c -o hello.exe        # host-targeted .exe, not a Windows binary
  ```
- **COUNTEREXAMPLE** (good):
  ```
  zig cc -target x86_64-windows-gnu hello.c -o hello.exe
  zig cc -target aarch64-linux-musl -static hello.c -o hello
  ```
- **VERIFICATION**: `file`/`objdump -f` on the outputs (binutils-docs) shows the target
  triple in the header.
- **SOURCE**: zig-release-notes 0.15.x/0.16.0 (Toolchain — zig cc, bundled libc);
  zig-langref §C.

## 5. CPU features: baseline, native, or named models

- **RULE**: `-Dcpu`/`-mcpu` accept `baseline`, `native`, or a model from `zig cc -mcpu
  help` (e.g. `znver2`), plus `+feature`/`-feature` suffixes. `baseline` is the portable
  choice for distribution; `native` is host-only.
- **WHY AI GETS IT WRONG**: writes ad-hoc feature lists or `-mcpu=native` into release
  builds, breaking the artifact on other machines.
- **CORRECT REASONING**: release artifacts for distribution use `baseline` (or a named
  model matching the deployment fleet); `native` only for host-local runs. Verify the
  feature name with `zig cc -mcpu help` — guessing causes "unknown feature" errors.
- **EXAMPLE** (bad):
  ```
  zig build -Dcpu=native -Doptimize=ReleaseFast   # binary tied to this CPU
  ```
- **COUNTEREXAMPLE** (good):
  ```
  zig build -Dcpu=baseline -Doptimize=ReleaseFast
  zig build -Dcpu=x86_64_v3 -Doptimize=ReleaseFast   # named model/family
  ```
- **VERIFICATION**: `zig cc -mcpu help` lists models and features; `zig build
  -Dcpu=baseline` succeeds.
- **SOURCE**: zig-build-guide (Standard Configuration Options — `-Dcpu`);
  zig-langref §Targets.

## 6. builtin.target vs deprecated builtin.os/cpu/abi

- **RULE**: `@import("builtin").target` is the current target struct (`.cpu.arch`,
  `.os.tag`, `.abi`, `.cpu.model`, `.cpu.features`). The top-level `builtin.os`,
  `builtin.cpu`, `builtin.abi` are deprecated and scheduled for removal in 0.18.0.
- **WHY AI GETS IT WRONG**: writes `builtin.cpu.arch` or `builtin.os.tag` from older
  tutorials; gets deprecation warnings/errors on 0.17-dev.
- **CORRECT REASONING**: use `builtin.target.cpu.arch` etc. Compile-time branching on the
  target is the canonical portable pattern.
- **EXAMPLE** (bad, deprecated on 0.16+):
  ```zig
  const is_windows = builtin.os.tag == .windows;
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  const is_windows = builtin.target.os.tag == .windows;
  const arch = builtin.target.cpu.arch;
  ```
- **VERIFICATION**: `zig test` with the deprecated spelling emits a deprecation warning
  (0.16; exact text UNVERIFIED).
- **SOURCE**: zig-langref §Compile Variables (the master dump shows the Deprecated notes
  and `builtin.target` fields); zig-release-notes 0.16.0.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Default | host target; cross-compilation requires `-target`/`-Dtarget` |
| Triple | `arch-os-abi`; `aarch64-linux-gnu`, not `gnueabihf`; check `zig targets` |
| Query | `std.Target.Query{ .cpu_arch, .os_tag, .abi }` + `resolveTargetQuery` |
| zig cc | Clang-based cross-compiler; bundled glibc 2.43 / musl 1.2.5 / MinGW / WASI |
| CPU | `-Dcpu=baseline` portable; `native` host-only; `zig cc -mcpu help` for names |
| builtin | `builtin.target`; `builtin.os/cpu/abi` deprecated, removed in 0.18 |
| Foreign runs | `-fqemu`/`-fwasmtime`/`-fwine`/`-frosetta` integrations; `skip_foreign_checks` |
| Tiers | Tier 1 native backends; Tier 2/3 via LLVM; Tier 4 assembly only |
