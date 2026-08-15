---
name: build-system-cmake-diagnostics
description: Use when a CMake build fails or a dependency is misdeclared: missing imported targets, find_package errors, hand-rolled include/link paths, wrong target_link_libraries semantics. Teaches diagnosing the target/dependency graph instead of inventing versions, paths, or home-cooked logic.
---

# CMake Diagnostics: Fix the Target Graph, Not the Symptom

## When to use

- Configure fails with "target was not found" / "link interface of target contains".
- An agent "fixes" a build by inventing a library version, hardcoding an absolute
  include/lib path, or pasting detection logic into CMakeLists.
- A dependency was declared (namespaced or bare name) but never defined or found.
- You need to prove which targets actually exist and who links whom.

## When not to use

- Runtime/link error in C/C++ symbols (`undefined reference`) — use
  `build-linker-error-diagnostics`.
- Compiler/`-std=`/ABI version mismatch — use `build-toolchain-version-drift`.
- Ninja state corruption or signal-killed builds — use
  `build-process-signal-and-state-safety`.
- Package manager / dependency supply-chain selection — different domain.

## What the agent often gets wrong

- "Fixing" the dependency by writing `find_package(foo 9.9.9)` or downloading a
  version, without checking whether the target exists in the graph.
- Linking a bare name (`target_link_libraries(app greet)`) and treating it like a
  defined target; CMake treats bare names as `-lgreet`.
- Confusing `find_package` failure with "the package is not installed" — often it
  is the toolchain env (prefix / pkg-config path) that CMake cannot see.
- Recreating CMake logic by hand (`link_directories`, absolute paths) instead of
  letting `target_link_libraries` propagate usage requirements.
- Rewriting the whole CMakeLists after one error ("escape loop") instead of
  running `cmake --trace-expand` / `--graphviz` once.
- Declaring a "success" for a mock benchmark that never actually configured.

## How to reason correctly

1. Read the error: CMake names the missing target and lists causes (typo, missing
   `find_package`, missing ALIAS). Trust it over intuition.
2. Diagnose the graph, not the text: `cmake --trace-expand` shows every command
   with variables expanded; `cmake --graphviz` dumps target edges.
3. A dependency is either (a) a target in this project, (b) an imported target
   from `find_package`, or (c) a system library via pkg-config. Never invent (d).
4. Use `find_package` + imported targets (`ZLIB::ZLIB`, `PkgConfig::ZLIB`); the
   found package reports the real version — you never supply one.
5. `target_link_libraries(app PRIVATE target)` propagates usage requirements
   (include dirs, link flags). PRIVATE for internal deps, PUBLIC to re-export.
6. If `find_package` fails, fix the toolchain environment
   (`CMAKE_PREFIX_PATH`, `PKG_CONFIG_PATH`) — not the CMakeLists logic.

## What to verify

- Configure succeeds and `ninja` completes with the intended link command
  (`ninja -t commands` shows the real flags, no invented ones).
- The declared target actually exists in `cmake --graphviz` output.
- No absolute paths or `link_directories`/`include_directories` hacks remain.
- The app runs and produces the expected output.
- Re-configure is reproducible on a clean machine (no fabricated versions).

## How to verify

```
cmake -G Ninja -S bad -B bad/build      # expect: target was not found, exit 1
cmake -G Ninja -S good -B good/build    # exit 0
ninja -C good/build                     # exit 0; app runs
cmake --graphviz=graph.dot good         # inspect target edges
cmake --trace-expand -S good -B trace   # see expanded commands
ninja -C good/build -t deps             # real dependency store
```

Compare the error line to the graph: the missing name in the error must be a
missing node in the graph.

## Where the knowledge comes from

- `cmake-docs` — find_package, target_link_libraries, imported targets,
  CMakeError semantics, graphviz/trace options.
- `pkgconfig-docs` — pkg_check_modules, IMPORTED_TARGET, `--cflags`/`--libs`.
- `ninja-manual` — `-t deps`/`-t commands` for verifying the real build graph.
- `gcc-manual` — interpretation of the toolchain environment flags.

## Related skills

- `build-linker-error-diagnostics` — link-stage failures after configure succeeds
- `build-toolchain-version-drift` — compiler/std/ABI drift misread as build bugs
- `build-process-signal-and-state-safety` — build state corruption, exit codes
- `embedded-hil-ci-testing` — same discipline applied to hardware builds

## Evaluation

- Synthetic: bad example must fail configure with the canonical "target was not
  found" error; good examples must configure, build, and run with exit 0.
- False-positive: a valid local-target and a valid pkg-config build must NOT be
  flagged for missing dependencies; `find_package(ZLIB)` failing due to env paths
  must be diagnosed as an environment issue, not a CMake bug.
- Historical: re-diagnose the bnnet-style "mock benchmark declared success" case:
  no configure+link happened, so no dependency was ever proven.
- Adversarial: `hand_rolled.cmake` "fixes" the error by hardcoding paths — it
  must be rejected as non-reproducible even if it happens to compile.
- Verified facts (actual runs recorded 2026-08-15): `evals/README.md`.
