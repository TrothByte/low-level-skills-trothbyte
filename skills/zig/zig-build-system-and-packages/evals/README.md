# Evaluation — zig-build-system-and-packages

Skill: `skills/zig/zig-build-system-and-packages`.
Stability target: `researched`. Toolchain: zig is NOT installed on this host; the build
code targets the 0.15–0.17 API surface (verified against zig-build-guide and the langref
C-section build examples). Verification commands below are the recorded plan, not run
results.

## Synthetic evals

| Case | Fixture | Expected | Command |
|---|---|---|---|
| easy/negative | `bad/build_old_api.zig` | fails on 0.15+: `root_source_file` field gone | `zig build` |
| medium/negative | `bad/build_hardcoded_path.zig` | compiles but violates the LazyPath rule (review) | `zig build` + review |
| medium/negative | review | uninstalled artifact: `zig build` does nothing | `zig build --summary all` |
| medium/negative | review | dependency wired without `dep.module(...)` | `zig build` |
| positive | `good/build.zig` | `zig build`, `zig build test`, `zig build run` all work | `zig build --summary all` |
| positive | `good/build_with_c.zig` | C source + libc + linkLibrary + linkSystemLibrary | `zig build --summary all` |

## False-positive evals (correct code must not be flagged)

- `good/build.zig` — `b.createModule` + `root_module`, `standardTargetOptions`/
  `standardOptimizeOption`, `addRunArtifact`, `addTest` + named test step: the documented
  0.15+ shape.
- `good/build_with_c.zig` — `addCSourceFile`, `link_libc`, `linkLibrary`,
  `linkSystemLibrary` on the root module.
- `zig build --fuzz` / `--test-timeout` / `-j<N>` usage — documented 0.16+ flags.

## Historical evals

- 0.15 refactor to `b.createModule`/`root_module` — `bad/build_old_api.zig` reproduces
  the pre-0.15 pattern.
- 0.14+ `fingerprint` requirement in `build.zig.zon`; 0.16 fetch-into-project-directory
  and `--fork` package overrides.
- 0.16.0 unit-test timeouts and `--error-style`/`--multiline-errors` flags — a regression
  target for docs that predate them.

## Adversarial evals

- A build.zig whose step graph "passes" `zig build` but never installs or runs the claimed
  artifacts (dead steps) — the DAG inspection gate.
- A package pin with a doctored hash/fingerprint that builds only because the cache is
  stale — must be caught by the zon `fingerprint`/hash rules.
- A `build.zig.zon` whose `minimum_zig_version` is newer than the toolchain the agent
  uses, silently masked by a downgraded pin.

## Verified facts

- KNOWN (from zig-build-guide and langref C-section examples; not run on this host):
  - 0.15+ artifacts take `.root_module = b.createModule(...)`.
  - Install step starts empty; `installArtifact`/`getInstallStep().dependOn` connect output.
  - Build scripts must not hardcode output paths (breaks caching/composability).
  - `--fuzz[=limit]` exists on `zig build` (0.16 help text); `-j<N>` controls parallelism.
  - 0.16.0: packages fetched into a project-local directory; `--fork` overrides packages
    locally.
- INFERRED: exact `build.zig.zon` `fingerprint` requirement semantics across 0.14/0.15.
- UNVERIFIED (needs zig on this host): actual step summaries and error output.

## Target toolchains (absent, documented)

- zig 0.15.2 / 0.16.0 / 0.17.0-dev: not installed. A C compiler is needed to exercise the
  C-integration build. First execution plan: install zig, then run the commands in
  SKILL.md §How to verify.
