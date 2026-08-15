---
name: zig-build-system-and-packages
description: Use when creating or reviewing build.zig/build.zig.zon, wiring packages and dependencies, integrating C sources, or orchestrating test/run steps. Prevents 0.14-era API usage, hardcoded paths that break caching, missing install steps, and wrong dependency wiring. Version-pinned to Zig 0.15-0.17.
---

# Zig Build System and Packages

## When to use

- Authoring `build.zig` / `build.zig.zon` for a project or library.
- Wiring dependencies (`b.dependency`, `.imports`), user options (`b.option`), and
  standard options (`-Dtarget`, `-Doptimize`).
- Integrating C sources and libraries (`addCSourceFile`, `link_libc`,
  `linkSystemLibrary`, `linkLibrary`, `addTranslateC`).
- Orchestrating steps: install, run, test, fuzz (`--fuzz`), codegen tools.

## When not to use

- Compiling a single file with no project structure — `zig build-exe`/`zig test` are enough
  (the build guide says so explicitly).
- Build logic that must be portable beyond Zig (CI templates, Makefiles) — that's
  build-systems domain (`build-system-cmake-diagnostics`).
- Runtime behavior of the built programs — see the topic skills.

## What the agent often gets wrong

- Using the 0.14-era API `b.addExecutable(.{ .name, .root_source_file = ..., .target,
  .optimize })` — 0.15+ wraps sources in `b.createModule(.{ .root_source_file })` and
  passes `.root_module`.
- Hardcoding output paths instead of using `b.installArtifact`/`--prefix`/`LazyPath` —
  hardcoded paths break caching, concurrency, and composability (documented build-guide
  rule).
- Forgetting that `zig build` alone does nothing until the install step has dependencies:
  every artifact must reach `installArtifact` or a named step.
- Wiring imports with `@import("c")`-style string names that don't match the module names
  added in `.imports`.
- Using `exe.root_module.addCSourceFile` vs `addCSourceFiles`/old `addCSourceFile` APIs
  from wrong versions; or adding C files without `.link_libc = true`.
- Confusing `.zig-cache` (rebuildable, never commit) with `zig-out` (install prefix,
  user-chosen via `--prefix`).
- Putting modules/artifacts from `b.dependency` into `imports` without creating the module
  (`dep.module("name")`), or inventing `fingerprint`/`hash` fields for 0.14+ zon files.

## How to reason correctly

1. Model the project as a DAG of steps. `addExecutable`/`addLibrary`/`addTest` create
   compile steps; `addRunArtifact` creates a run step; `test_step.dependOn(&run.step)`
   wires them; `b.installArtifact` connects artifacts to the default install step.
2. Describe every source location with `b.path(...)` or a `LazyPath`; never with
   absolute paths. Outputs come from `addOutputFileArg`/`addWriteFiles`, not hardcoded
   names.
3. Wire modules by name: `b.createModule(.{ .root_source_file, .target, .optimize,
   .imports = &.{ .{ .name = "c", .module = ... } }, .link_libc })`, and import them with
   `@import("c")`.
4. Declare dependencies in `build.zig.zon` (`.dependencies`, url + hash, `fingerprint` on
   0.14+), fetch, and use `b.dependency("name", .{})` → `.module`/`.artifact`.
5. Add C integration at the module level: `.link_libc = true`,
   `root_module.addCSourceFile(.{ .file, .flags })`, `linkSystemLibrary("z", .{})`,
   `addIncludePath`, `addTranslateC`.
6. Expose user options via `b.option`; the generated `--help` documents them.

## What to verify

- `build.zig` compiles with the pinned Zig; no 0.14-era `root_source_file` at the
  `addExecutable` level.
- All artifacts reach the install step or a named step; `zig build` and `zig build test`
  do what the step graph says.
- No absolute paths or hardcoded output names in build.zig.
- `build.zig.zon` has `name`, `version`, `paths`, and for packages `fingerprint`;
  `minimum_zig_version` set for pinning.
- C sources linked with `link_libc`/`linkSystemLibrary` as needed; include paths present.
- `zig build --help` lists the project-specific options.

## How to verify

```
zig build --summary all            # install step + artifacts
zig build test --summary all       # test step runs unit tests
zig build -Dtarget=x86_64-windows -Doptimize=ReleaseSmall --summary all
zig build --help                   # project options listed
zig build --fuzz=10K               # fuzz the test suite (0.16+)
zig build --watch                  # rebuild on change
```

Researched — zig not installed on this host; commands are the recorded verification plan.

## Where the knowledge comes from

- zig-build-guide (Getting Started; The Basics: user options, standard options,
  conditional compilation; Static/Dynamic Library; Testing; Linking to System Libraries;
  Generating Files; Handy Examples).
- zig-langref §Zig Build System, §C (Exporting a C Library — build_c.zig example;
  Mixing Object Files — build_object.zig example).
- zig-release-notes 0.16.0 (Build System: Ability to Override Packages Locally, Fetch
  Packages Into Project-Local Directory, Unit Test Timeouts, --error-style,
  --multiline-errors, Temporary Files API).
- zig-std-source (std/Build.zig).

## Related skills

- `zig-cross-compilation-targets` — `standardTargetOptions`, `-Dtarget`, `-Dcpu`.
- `zig-ffi-c-interop` — `addTranslateC`, `addCSourceFile`, `linkSystemLibrary`.
- `zig-version-migration` — `minimum_zig_version`, pinned toolchain, 0.15 API changes.
- `zig-fuzzer-and-testing` — `zig build --fuzz` and `--test-timeout`.
- `zig-comptime-metaprogramming` — generating Zig source with `addAnonymousImport`.

## Evaluation

- Synthetic: 0.14-era build API, hardcoded paths, uninstalled artifacts, misnamed module
  imports, missing `link_libc` — must be caught; good build.zig (exe+test+option) and
  C-integration build must pass.
- False-positive: `b.createModule` + `root_module` wiring, `b.option`, `b.dependency`
  + `dep.module`, `addWriteFiles`, and `linkSystemLibrary` must NOT be flagged.
- Historical: the 0.15 root_module/`createModule` refactor and the 0.14 zon `fingerprint`
  requirement are regression targets.
- Adversarial: a build.zig that "passes" `zig build` but never installs or runs what it
  claims (dead steps), or a package pin with a doctored hash — the DAG inspection gate.
- Commands and recorded results: `evals/README.md`.
