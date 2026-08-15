# build-systems — Skills

Low-level engineering skills for this domain.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `build-linker-error-diagnostics` | Use when linking fails: undefined references, symbol/ABI mismatches, archive pull-in and --whole-archive behavior, wrong-mangled or versioned symbols, or massive undefined-ref cascades. Teaches reading symbol tables (nm/objdump/readelf) instead of guessing which flag to add. | source-backed | `skills/build-systems/build-linker-error-diagnostics` |
| `build-process-signal-and-state-safety` | Use when a build may not have actually run or succeeded: signals killing ninja/make mid-write, corrupted .ninja_deps, sandbox no-ops that exit 0, ignored build exit codes. Teaches exit-code checks, output-change verification, ninja state inspection/repair, and write-only-after-success discipline. | researched | `skills/build-systems/build-process-signal-and-state-safety` |
| `build-system-cmake-diagnostics` | Use when a CMake build fails or a dependency is misdeclared: missing imported targets, find_package errors, hand-rolled include/link paths, wrong target_link_libraries semantics. Teaches diagnosing the target/dependency graph instead of inventing versions, paths, or home-cooked logic. | source-backed | `skills/build-systems/build-system-cmake-diagnostics` |
| `build-toolchain-version-drift` | Use when a build fails or "misbehaves" due to compiler, libstdc++/ABI, or -std= version drift, or when -O levels produce identical binaries. Teaches pinning the standard, dumping compiler macros, and proving which flags actually reached the compile command. | source-backed | `skills/build-systems/build-toolchain-version-drift` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
