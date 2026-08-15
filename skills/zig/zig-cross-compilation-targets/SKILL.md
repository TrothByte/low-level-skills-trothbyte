---
name: zig-cross-compilation-targets
description: Use when building for a target other than the host: -Dtarget triples, std.Target.Query, zig cc as a cross-compiler, CPU features (-mcpu), bundled libc, and running foreign binaries. Prevents host-only builds, wrong ABI suffixes, and feature-guessing. Version-pinned to Zig 0.15-0.17.
---

# Zig Cross-Compilation Targets

## When to use

- Building for another OS/arch/ABI with `zig build -Dtarget=...` or `zig build-exe -target ...`.
- Using `zig cc`/`zig c++` as a drop-in cross-compiler for C/C++.
- Selecting CPU features (`-Dcpu`, `-mcpu baseline/native/model`) in build scripts.
- Detecting the target in code (`@import("builtin").target`, `std.Target`).
- Planning to run a foreign binary (QEMU/Wine/wasmtime integration).

## When not to use

- Host-only single-file builds — `zig build-exe` without `-target` is enough.
- CPU microarchitecture tuning of a host build — see `cache-and-numa-optimization`.
- Cross-language ABI layout questions — see `abi-layout-reasoning` and
  `zig-ffi-c-interop`.
- Emulator setup itself — see `qemu-system-setup`.

## What the agent often gets wrong

- Assuming the host target is fine and shipping binaries that only run on the build
  machine (the langref is explicit: default = host).
- Writing triples with wrong ABI suffixes: `aarch64-linux-gnueabihf` vs
  `aarch64-linux-gnu` (arm vs arm64 hard-float), or omitting the ABI and getting
  `gnu`/`musl` confusion.
- Guessing CPU feature strings instead of using `zig targets` / `zig cc -mcpu help`
  (`baseline`, `native`, or a named model like `znver2`).
- Forgetting that libc is bundled: `zig cc -target x86_64-windows-gnu` links against
  Zig's MinGW/libc stubs with no system toolchain.
- Using deprecated `@import("builtin").os`/`.cpu`/`.abi` on 0.16+ (removal planned for
  0.18) instead of `builtin.target`.
- Claiming a target is "unsupported" because it is Tier 3/4 for the compiler when it still
  cross-compiles via LLVM.

## How to reason correctly

1. State the target as a full triple (arch-os-abi) whenever the ABI matters;
   `zig targets` prints the complete table, including Tier 1-4 status.
2. In build.zig, use `b.standardTargetOptions(.{})` so users pass `-Dtarget=...`;
   for programmatic lists use `std.Target.Query{ .cpu_arch = .x86_64, .os_tag = .linux }`
   and `b.resolveTargetQuery(...)`.
3. For C/C++, use `zig cc -target <triple>`; it is Clang-based and ships glibc, musl,
   MinGW-w64, WASI libc and headers — no system cross toolchain needed.
4. Pick CPU features deliberately: `-mcpu baseline` (portable), `native` (host), or a
   model from `zig cc -mcpu help`; `-Dcpu` accepts `baseline`/`native`/model and
   `feature`/`-feature` suffixes.
5. Read the target in code via `@import("builtin").target` (`.cpu.arch`, `.os.tag`,
   `.abi`); on 0.16+ the top-level `builtin.os`/`cpu`/`abi` are deprecated.
6. For running foreign binaries, use the build system integrations (`-fqemu`,
   `-fwasmtime`, `-fwine`, `-frosetta`) — documented in the build-guide help text; these
   require the corresponding emulator installed (absent on this host).

## What to verify

- `-target`/`-Dtarget` matches the deployment environment, including the ABI suffix.
- `zig targets` consulted for arch/OS/ABI spellings before writing a triple.
- `-mcpu`/`-Dcpu` selection is explicit in release builds (`baseline` or a named model).
- C code cross-compiles with `zig cc -target <triple>` and links against the bundled libc.
- Code uses `builtin.target` (not deprecated `builtin.os/cpu/abi`) on 0.16+.
- Foreign binaries are executed through the recorded integration, not claimed to run.

## How to verify

```
zig targets                              # full target table incl. tiers
zig build -Dtarget=x86_64-windows-gnu --summary all
zig build -Dtarget=aarch64-linux-gnu --summary all
zig build -Dcpu=baseline -Dtarget=x86_64-linux-gnu --summary all
zig cc -target x86_64-windows-gnu -O2 -c hello.c
zig cc -target aarch64-linux-musl -static hello.c
zig cc -mcpu help                        # CPU feature names and models
zig build test --summary all             # with test_targets + skip_foreign_checks
```

Researched — zig not installed on this host; commands are the recorded verification plan.

## Where the knowledge comes from

- zig-langref §Targets (composition of arch/features/OS/version/ABI; `zig targets`;
  host default), §Compile Variables (builtin.target; builtin.os/cpu/abi deprecated,
  removal in 0.18), §C Translation CLI (-target).
- zig-release-notes 0.15.x/0.16.0 (Target Support: tier system, bundled libc — musl 1.2.5,
  glibc 2.43, MinGW-w64, WASI libc; zig cc based on Clang 21).
- zig-build-guide (Standard Configuration Options: `-Dtarget`, `-Dcpu`, `-Doptimize`;
  system integrations `-fqemu`, `-fwine`, `-fwasmtime`, `-frosetta`).
- zig-std-source (std/Target.zig — Query, resolveTargetQuery).

## Related skills

- `zig-build-system-and-packages` — `standardTargetOptions`, `resolveTargetQuery`.
- `zig-ffi-c-interop` — `zig cc` and translate-c targets must match.
- `zig-inline-asm-and-abi` — calling conventions differ per target triple.
- `zig-version-migration` — `builtin.os/cpu` deprecation (0.16, removed 0.18).
- `qemu-system-setup` — running foreign binaries.

## Evaluation

- Synthetic: host-only build where a target was intended, wrong ABI suffix, guessed CPU
  feature names, deprecated `builtin.os` — must be caught; multi-target build.zig and
  `builtin.target` code must pass.
- False-positive: `zig build -Dtarget=aarch64-linux-gnu` and `-Dcpu=baseline` release
  builds, and `zig cc -target` C cross-compiles must NOT be flagged.
- Historical: the `builtin.os/cpu/abi` deprecation (0.16, removal 0.18) and the bundled-
  libc changes (musl 1.2.5/glibc 2.43 in 0.16) are regression targets.
- Adversarial: a triple that parses but targets the wrong ABI (e.g. `x86_64-linux-gnueabihf`)
  — must be caught by consulting `zig targets`, not by "it compiles".
- Commands and recorded results: `evals/README.md`.
