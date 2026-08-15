# Evaluation — zig-cross-compilation-targets

Skill: `skills/zig/zig-cross-compilation-targets`.
Stability target: `researched`. Toolchain: zig is NOT installed on this host; the code
targets the 0.15–0.17 API surface (verified against the langref Targets section, the
build-guide, and release notes). Verification commands below are the recorded plan, not
run results.

## Synthetic evals

| Case | Fixture | Expected | Command |
|---|---|---|---|
| easy/negative | `bad/build_host_only.zig` | host-only pin flagged; ignores `-Dtarget` | `zig build` + review |
| medium/negative | `bad/deprecated_builtin_os.zig` | deprecation warning/error on 0.16+ | `zig test` |
| medium/negative | review | wrong ABI suffix in a `-target` invocation | `zig targets` check |
| positive | `good/build.zig` | multi-target matrix builds | `zig build test --summary all` |
| positive | `good/main.zig` | `builtin.target` usage | `zig test` |

## False-positive evals (correct code must not be flagged)

- `good/build.zig` — `standardTargetOptions`, `std.Target.Query` matrix,
  `skip_foreign_checks = true`.
- `zig build -Dtarget=x86_64-windows-gnu` and `-Dcpu=baseline` — correct release practice.
- `zig cc -target aarch64-linux-musl -static` — correct cross-C compile.
- `builtin.target.cpu.arch` checks — correct, not "unportable".

## Historical evals

- 0.16.0: `builtin.os/cpu/abi` deprecated; removal planned for 0.18.0 (langref master
  dump shows `Deprecated; to be removed in 0.18.0`).
- 0.16.0 toolchain: zig cc on Clang 21.1.8; bundled musl 1.2.5, glibc 2.43, Linux 6.19
  headers, MinGW-w64, WASI libc — claims about needing system cross toolchains are stale.
- 0.16.0: OpenBSD 7.8+ dynamic libc cross-compilation via stub libraries.

## Adversarial evals

- A triple that parses but targets the wrong ABI (`x86_64-linux-gnueabihf`) — the gate is
  consulting `zig targets`, not "it compiled".
- A `-Dcpu=native` release build that passes CI on the build machine but crashes with SIGILL
  on deployment hardware — the baseline-vs-native rule.
- Code that reads `builtin.os.tag` at comptime and silently branches wrong on 0.17-dev —
  deprecation must be caught.

## Verified facts

- KNOWN (from langref Targets / Compile Variables and release notes; not run on this host):
  - Default target is the host; `zig targets` lists all targets.
  - `zig cc` is Clang-based (0.16: 21.1.8) and cross-compiles with bundled libc
    (glibc 2.43, musl 1.2.5, MinGW-w64, WASI libc).
  - `builtin.os`/`cpu`/`abi` deprecated, removal in 0.18.0.
  - Build-guide documents `-fqemu`, `-fwasmtime`, `-fwine`, `-frosetta` system
    integrations and `skip_foreign_checks`.
- INFERRED: `x86_64_v3` model availability and the exact `-Dcpu` syntax accept list.
- UNVERIFIED (needs zig on this host): exact deprecation diagnostics and triple-rejection
  errors; target support tier status for the current master snapshot.

## Target toolchains (absent, documented)

- zig 0.15.2 / 0.16.0 / 0.17.0-dev: not installed. QEMU/Wine/wasmtime integrations are
  documented but absent. First execution plan: install zig, then run the commands in
  SKILL.md §How to verify.
