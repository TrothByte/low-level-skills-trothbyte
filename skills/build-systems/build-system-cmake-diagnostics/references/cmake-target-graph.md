# CMake Dependency Diagnostics — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. A dependency is a target, and it must exist in the graph

- **RULE**: `target_link_libraries(app PRIVATE X)` declares an edge from `app` to
  the target `X`. `X` must be a real target: defined by `add_library`, imported by
  `find_package`, or provided by `pkg_check_modules(IMPORTED_TARGET)`. A namespaced
  name (`greet::greet`) is never a raw library; CMake resolves it as a target name.
- **WHY AI GETS IT WRONG**: writes the declaration first and reasons about the
  dependency afterwards; "fixes" the symptom by adding a version or a path; assumes
  a bare name in the link interface is a real library target.
- **CORRECT REASONING**: read the error. For a missing namespaced target CMake
  prints `The link interface of target "dep" contains: greet::greet but the target
  was not found`, listing causes: typo, missing `find_package`, missing ALIAS. The
  fix is to make the target exist — define it or find it — never to mask it.
- **EXAMPLE** (bad):
  ```cmake
  add_library(dep STATIC dep.c)
  target_link_libraries(dep PUBLIC greet::greet)   # greet::greet does not exist
  add_executable(app app.c)
  target_link_libraries(app PRIVATE dep)
  # configure: CMake Error ... but the target was not found ... exit 1
  ```
- **COUNTEREXAMPLE** (good):
  ```cmake
  add_library(greet STATIC greet.c)                 # define it, or:
  find_package(ZLIB REQUIRED)                       # find_package imports it
  target_link_libraries(app PRIVATE greet ZLIB::ZLIB)
  ```
- **VERIFICATION**: `cmake -G Ninja -S bad -B bad/build` → exit 1 with the
  canonical error (recorded 2026-08-15); good project exits 0.
- **SOURCE**: cmake-docs (target_link_libraries, imported targets, error text).

## 2. Bare names vs targets: what CMake actually links

- **RULE**: a bare name in `target_link_libraries` (no `::`) that is not a known
  target is treated as a plain library flag (`greet` → `-lgreet`). The failure
  then surfaces at link time, not configure time, and often as a confusing
  `cannot find -lgreet` or a missing-header compile error.
- **WHY AI GETS IT WRONG**: sees `target_link_libraries(app foo)` and reads it as
  "link the foo target", then cannot explain why configure passed but the build
  broke; blames the compiler or the library instead of the missing target.
- **CORRECT REASONING**: if no target named `greet` exists, CMake emits the raw
  linker flag. The include directory also comes from the target — with no
  `target_include_directories(greet PUBLIC ...)`, `#include "greet.h"` fails at
  compile time. Both symptoms trace to one cause: the dependency target is missing.
- **EXAMPLE** (bad):
  ```cmake
  add_executable(app app.c)
  target_link_libraries(app PRIVATE greet)   # no add_library(greet) -> -lgreet
  ```
- **COUNTEREXAMPLE** (good):
  ```cmake
  add_library(greet STATIC greet.c)
  target_include_directories(greet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  add_executable(app app.c)
  target_link_libraries(app PRIVATE greet)   # real target, usage requirements flow
  ```
- **VERIFICATION**: `ninja -C bad/build` → `greet.h: No such file or directory`
  (recorded 2026-08-15); good build exit 0, app prints `hello, troth`.
- **SOURCE**: cmake-docs (target_link_libraries semantics, usage requirements).

## 3. Diagnose with the graph, not by editing text

- **RULE**: CMake exposes its own state: `cmake --trace-expand` prints every
  command with variables already expanded; `cmake --graphviz=graph.dot` writes the
  target dependency graph; `ninja -t deps` and `ninja -t commands` show the real
  dependency store and the real command lines.
- **WHY AI GETS IT WRONG**: rewrites CMakeLists after one error (the "escape
  loop"), guessing what the next error will be, instead of asking the build
  system what its graph and commands actually are.
- **CORRECT REASONING**: one `--trace-expand` run shows exactly which commands ran
  and with which values; the graphviz file shows which targets exist and who links
  whom. Compare the error's missing name to the graph's node list.
- **EXAMPLE** (bad): editing `target_link_libraries(dep PUBLIC greet::greet)` to
  `target_link_libraries(dep PUBLIC greet)` and re-running, without checking
  whether `greet` exists.
- **COUNTEREXAMPLE** (good):
  ```
  cmake --graphviz=graph.dot .
  cmake --trace-expand .   # shows: add_library(greet STATIC greet.c) etc.
  ```
- **VERIFICATION**: recorded 2026-08-15 — graphviz emits nodes `app`, `greet`;
  `--trace-expand` shows `add_library(greet STATIC greet.c)` and
  `target_link_libraries(app PRIVATE greet)` with expanded values; `ninja -t deps`
  lists `greet.c`, `greet.h` as real deps of the object file.
- **SOURCE**: cmake-docs (--trace-expand, --graphviz), ninja-manual (-t deps).

## 4. find_package + pkg-config, never invented versions

- **RULE**: for external packages use `find_package(PkgConfig REQUIRED)` +
  `pkg_check_modules(FOO REQUIRED IMPORTED_TARGET foo)` or a package's own
  `find_package(Foo)` module, then consume the imported target (`PkgConfig::FOO`).
  The found package reports the actual installed version.
- **WHY AI GETS IT WRONG**: answers "I need zlib" by inventing
  `find_package(ZLIB 9.9.9)` or hardcoding a path; when `find_package` fails it
  concludes the package is missing instead of checking the search paths.
- **CORRECT REASONING**: `find_package(ZLIB)` failing here is an environment
  problem — the MSYS2 ucrt64 prefix is not in CMake's default search path. Fix
  `CMAKE_PREFIX_PATH`/`PKG_CONFIG_PATH` (toolchain env), not the CMakeLists
  logic. pkg-config reports `zlib 1.3.2` from the same environment and the
  `IMPORTED_TARGET` form gives a first-class target with usage requirements.
- **EXAMPLE** (bad):
  ```cmake
  find_package(ZLIB 9.9.9 REQUIRED)                    # invented version
  target_link_libraries(app PRIVATE ZLIB::ZLIB)        # never found
  ```
- **COUNTEREXAMPLE** (good):
  ```cmake
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)
  target_link_libraries(app PRIVATE PkgConfig::ZLIB)
  ```
- **VERIFICATION**: recorded 2026-08-15 — `pkg_check_modules` → `Found zlib,
  version 1.3.2`, build exit 0, app prints `compressed 5 bytes into 13`.
- **SOURCE**: cmake-docs (find_package, imported targets), pkgconfig-docs
  (pkg_check_modules, IMPORTED_TARGET, --cflags/--libs).

## 5. target_link_libraries visibility: PRIVATE vs PUBLIC

- **RULE**: PRIVATE adds the dependency only to this target's own link line;
  PUBLIC adds it to this target's link line AND re-exports it in the target's
  `INTERFACE_LINK_LIBRARIES` for consumers. A PUBLIC dependency on a missing
  target poisons every consumer.
- **WHY AI GETS IT WRONG**: uses PUBLIC everywhere "to be safe", then a consumer
  fails with a target-not-found error whose origin is one level up; or uses
  PRIVATE where consumers need the include dirs and the build breaks with
  missing headers.
- **CORRECT REASONING**: include directories and link deps follow the same
  visibility keyword. `target_include_directories(greet PUBLIC ...)` is how
  `#include "greet.h"` works in `app`; PRIVATE there hides it. PUBLIC link deps
  must exist — check the graph before propagating.
- **EXAMPLE** (bad):
  ```cmake
  add_library(dep STATIC dep.c)
  target_include_directories(dep PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(dep PUBLIC greet::greet)   # missing -> poisons consumers
  add_executable(app app.c)
  target_link_libraries(app PRIVATE dep)
  ```
- **COUNTEREXAMPLE** (good):
  ```cmake
  target_link_libraries(dep PRIVATE greet)   # internal-only edge
  target_link_libraries(app PRIVATE dep)     # PRIVATE for app's own deps
  ```
- **VERIFICATION**: recorded 2026-08-15 — PUBLIC-missing-target configure fails
  with the canonical error; the equivalent PRIVATE graph configures and builds.
- **SOURCE**: cmake-docs (target_link_libraries, INTERFACE_LINK_LIBRARIES,
  usage requirements).

## 6. Reproducibility: no absolute paths, no machine-specific hacks

- **RULE**: CMakeLists must be relocatable: no absolute include/lib paths, no
  hardcoded versions, no `include_directories`/`link_directories` re-created by
  hand. All of it belongs to the toolchain environment or imported targets.
- **WHY AI GETS IT WRONG**: the "escape loop" fix feels productive — it pastes
  an absolute path, configure succeeds on this machine, and the agent declares
  success without ever testing a clean machine; the build is unreproducible.
- **CORRECT REASONING**: an absolute path is a lie that happens to work here.
  Reproducibility = every input to the build is either in the repo or reported by
  a finder (pkg-config/CMake module) from the environment.
- **EXAMPLE** (bad): `bad/hand_rolled.cmake` — `set(GREET_LIBRARY
  "C:/Program Files/greet-1.2/lib/greet.dll")` with a fabricated version.
- **COUNTEREXAMPLE** (good): the pkg-config example above — zero absolute paths,
  version reported by the finder.
- **VERIFICATION**: grep the final CMakeLists for `/` and `C:`-style literals;
  re-configure in a fresh build dir succeeds without edits.
- **SOURCE**: cmake-docs (policy on paths, find_package), pkgconfig-docs.

## 7. A "success" that was never built is not a success

- **RULE**: a dependency is only proven when configure succeeded, the object and
  link commands actually ran, and the artifact exists and runs. "Declared" is not
  "built".
- **WHY AI GETS IT WRONG**: reports a benchmark/patch as passing because the
  intended build was stubbed, skipped, or never executed; the mock declares
  success (TensorRT/mock-benchmark case).
- **CORRECT REASONING**: enumerate the evidence chain: exit 0 from configure,
  exit 0 from ninja, artifact mtime updated, `ninja -t commands` shows real
  flags, runtime output matches. Any link missing → not verified.
- **EXAMPLE** (bad): an agent that returns "build OK" after `cmake` printed the
  error but the runner swallowed the exit code.
- **COUNTEREXAMPLE** (good): recorded sequence in evals/README.md — each step
  with its exit code and output.
- **VERIFICATION**: `ninja -C build -t commands` shows the actual `cc` lines;
  compare to what the agent claims was built.
- **SOURCE**: ninja-manual (-t commands), cmake-docs.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Namespaced names | `X::Y` is always a target; it must exist (defined/imported/ALIAS) |
| Bare names | treated as `-lX`; failures surface at compile/link, not configure |
| Diagnose | `cmake --trace-expand`, `cmake --graphviz`, `ninja -t deps/-t commands` |
| External deps | `find_package` + imported targets; pkg-config for toolchain env |
| Versions | reported by the finder — never invented in CMakeLists |
| PRIVATE/PUBLIC | PRIVATE internal, PUBLIC re-export; both must exist in the graph |
| Paths | no absolute paths; environment via CMAKE_PREFIX_PATH/PKG_CONFIG_PATH |
| Success | configure+build+run with real exit codes, not "declared" |
