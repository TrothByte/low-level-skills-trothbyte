# Zig Build System and Packages — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.
Version markers: KNOWN / INFERRED / UNVERIFIED.

## 1. Modules are created with b.createModule (0.15+)

- **RULE**: since 0.15, `addExecutable`/`addLibrary`/`addTest`/`addObject` take
  `.root_module = b.createModule(.{ .root_source_file = b.path("src/main.zig"), .target,
  .optimize })`. The 0.14-era `.root_source_file` directly on the artifact is gone.
- **WHY AI GETS IT WRONG**: writes the 0.14 pattern from memory; the compiler errors with
  "no field named root_source_file" and the agent "fixes" it by guessing fields.
- **CORRECT REASONING**: a module is the compilation unit; artifacts reference exactly one
  root module, and modules can be shared/reused (e.g. same module for exe and test).
- **EXAMPLE** (bad, fails on 0.15+):
  ```zig
  const exe = b.addExecutable(.{
      .name = "hello",
      .root_source_file = b.path("hello.zig"),
      .target = target,
      .optimize = optimize,
  });
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  const exe = b.addExecutable(.{
      .name = "hello",
      .root_module = b.createModule(.{
          .root_source_file = b.path("hello.zig"),
          .target = target,
          .optimize = optimize,
      }),
  });
  b.installArtifact(exe);
  ```
- **VERIFICATION**: `zig build --summary all` succeeds for the good build.zig and fails
  for the bad one (0.15+).
- **SOURCE**: zig-build-guide (Getting Started — Simple Executable); zig-langref §Zig
  Build System.

## 2. Steps form a DAG; installArtifact is how output escapes

- **RULE**: `zig build` runs the install step, which starts empty. Artifacts reach it via
  `b.installArtifact(exe)` (or `b.getInstallStep().dependOn(...)` for generated files).
  Unconnected compile steps are never built. `b.step("run", ...)` + `.dependOn(&run.step)`
  names steps for `zig build run`.
- **WHY AI GETS IT WRONG**: adds `b.addExecutable` and expects `zig build` to produce
  binaries; or wires a step to another step instead of to a run/install step and sees
  "nothing to do".
- **CORRECT REASONING**: the build is a DAG; a step runs only if reachable from the
  requested step. Install artifacts explicitly; run steps connect compile→run.
- **EXAMPLE** (bad):
  ```zig
  _ = b.addExecutable(.{ .name = "hello", .root_module = ... });
  // no installArtifact: `zig build` does nothing
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  const exe = b.addExecutable(.{ .name = "hello", .root_module = ... });
  b.installArtifact(exe);
  const run_exe = b.addRunArtifact(exe);
  const run_step = b.step("run", "Run the application");
  run_step.dependOn(&run_exe.step);
  ```
- **VERIFICATION**: `zig build --summary all` shows `install hello` in the good case and
  an empty graph in the bad case.
- **SOURCE**: zig-build-guide (Installing Build Artifacts; Adding a Convenience Step for
  Running the Application).

## 3. Paths are LazyPath, never hardcoded

- **RULE**: all file references go through `b.path("...")`, `tool_step.addOutputFileArg`,
  `b.addWriteFiles()`, or `LazyPath`s. The build script cannot hardcode output paths
  because that breaks caching, concurrency, and composability. `zig-out` is chosen by the
  user via `--prefix`; `.zig-cache` is disposable.
- **WHY AI GETS IT WRONG**: writes `"zig-out/bin/hello"` or absolute paths; or reasons
  about `.zig-cache` contents as if they were source.
- **CORRECT REASONING**: outputs of tool steps are `LazyPath`s (`addOutputFileArg`),
  inputs are `b.path`. Install to `.prefix` with `b.addInstallFileWithDir`.
- **EXAMPLE** (bad):
  ```zig
  tool_run.addArg("--output");
  tool_run.addArg("word.txt"); // relative to CWD — breaks caching
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  tool_run.addArg("--output");
  const output = tool_run.addOutputFileArg("word.txt");
  b.getInstallStep().dependOn(&b.addInstallFileWithDir(output, .prefix, "word.txt").step);
  ```
- **VERIFICATION**: `zig build --summary all` — good case installs the generated file;
  the bad case's artifact is not tracked.
- **SOURCE**: zig-build-guide (Installing Build Artifacts; Generating Files; Running the
  Project's Tools).

## 4. Dependency wiring: build.zig.zon + b.dependency

- **RULE**: dependencies are declared in `build.zig.zon` (`.name`, `.version`,
  `.fingerprint` on 0.14+, `.dependencies` with url+hash, `.paths`). In build.zig use
  `const dep = b.dependency("name", .{});` then `dep.module("libname")` or
  `dep.artifact("exe")`; expose modules via `.imports` in `createModule`.
- **WHY AI GETS IT WRONG**: invents hash/fingerprint values; wires
  `b.dependency(...).module` directly as an artifact; forgets to list dependency module
  names in `.imports` and then `@import`s an undeclared name.
- **CORRECT REASONING**: the zon pins the exact source; `b.dependency` gives handles to
  its public modules/artifacts. Module imports are declared by name in the consumer's
  `createModule(.imports = &.{...})`.
- **EXAMPLE** (bad):
  ```zig
  const dep = b.dependency("foo", .{});
  const exe = b.addExecutable(.{ .name, .root_module = b.createModule(.{
      .root_source_file = b.path("main.zig"),
      .imports = &.{.{ .name = "foo", .module = dep }}, // dep is not a module
  })});
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  const dep = b.dependency("foo", .{});
  const exe = b.addExecutable(.{ .name, .root_module = b.createModule(.{
      .root_source_file = b.path("main.zig"),
      .imports = &.{.{ .name = "foo", .module = dep.module("foo") }},
  })});
  ```
- **VERIFICATION**: `zig build` (fetches), then `zig build --help` and the import
  resolves; wrong wiring fails at module analysis.
- **SOURCE**: zig-build-guide (package management via .zon; 0.16.0 notes: Ability to
  Override Packages Locally with `--fork`, Fetch Packages Into Project-Local Directory).

## 5. C integration at the module level

- **RULE**: `createModule(.{ .link_libc = true })` links libc; `root_module.addCSourceFile(
  .{ .file = b.path("test.c"), .flags = &.{"-std=c99"} })` compiles C into the module;
  `linkSystemLibrary("z", .{})` links system libs; `linkLibrary(lib)` links a Zig-built
  library; `addIncludePath` adds headers.
- **WHY AI GETS IT WRONG**: adds C files without `link_libc`; uses `addCSourceFile` on the
  artifact instead of the root module; skips include paths and blames translate-c.
- **CORRECT REASONING**: the module owns its compilation inputs: Zig sources, C sources,
  headers, and libc linkage all configure the same root module.
- **EXAMPLE** (bad):
  ```zig
  const exe = b.addExecutable(.{ .name = "test", .root_module = b.createModule(.{}) });
  exe.addCSourceFile(...);       // no such method on exe in 0.15+
  // and no link_libc
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  const exe = b.addExecutable(.{ .name = "test", .root_module = b.createModule(.{
      .link_libc = true,
  })});
  exe.root_module.addCSourceFile(.{ .file = b.path("test.c"), .flags = &.{"-std=c99"} });
  exe.root_module.linkLibrary(lib);
  ```
- **VERIFICATION**: `zig build test` (langref's Exporting a C Library example) and
  `zig build` for the object-mixing example.
- **SOURCE**: zig-langref §C (Exporting a C Library, Mixing Object Files); zig-build-guide
  (Linking to System Libraries).

## 6. User options and standard options

- **RULE**: `b.option(bool, "windows", "Target Microsoft Windows")` creates `-Dwindows`
  options; `b.standardTargetOptions(.{})` and `b.standardOptimizeOption(.{})` create the
  standard `-Dtarget`, `-Dcpu`, `-Doptimize`. Options flow into `b.resolveTargetQuery(...)`
  or module config.
- **WHY AI GETS IT WRONG**: hardcodes targets/optimize in build.zig; forgets that
  `standardOptimizeOption` leaves the choice to the user; names options inconsistently.
- **CORRECT REASONING**: `zig build --help` is auto-generated from the build script; the
  standard options keep conventions across projects.
- **EXAMPLE** (bad):
  ```zig
  const exe = b.addExecutable(.{ .name, .root_module = b.createModule(.{
      .root_source_file = b.path("hello.zig"),
      .target = b.graph.host,     // host only — cross-compilation impossible
  })});
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  const target = b.standardTargetOptions(.{});
  const optimize = b.standardOptimizeOption(.{});
  const exe = b.addExecutable(.{ .name, .root_module = b.createModule(.{
      .root_source_file = b.path("hello.zig"),
      .target = target,
      .optimize = optimize,
  })});
  ```
- **VERIFICATION**: `zig build -Dtarget=x86_64-windows -Doptimize=ReleaseSmall
  --summary all` succeeds for the good case; the bad case ignores the flag.
- **SOURCE**: zig-build-guide (The Basics — User-Provided Options; Standard Configuration
  Options).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Artifact creation | `addExecutable`/`addLibrary`/`addTest` take `.root_module = b.createModule(...)` |
| Install | `b.installArtifact(exe)` or `b.getInstallStep().dependOn(...)` — else nothing runs |
| Paths | `b.path()` / `LazyPath` / `addOutputFileArg`; never hardcode; `.zig-cache` disposable |
| Run steps | `b.addRunArtifact(exe)` + `b.step("run", ...)` + `dependOn` |
| Dependencies | zon `.dependencies` + `b.dependency("n", .{})` + `dep.module("m")` → `.imports` |
| C integration | `link_libc`, `addCSourceFile`, `linkSystemLibrary`, `addIncludePath`, `linkLibrary` |
| Options | `b.option(...)`; `standardTargetOptions`/`standardOptimizeOption` for -Dtarget/-Doptimize |
| Fuzz | `zig build --fuzz[=limit]`; `--test-timeout`; `-j<N>` multiprocess |
