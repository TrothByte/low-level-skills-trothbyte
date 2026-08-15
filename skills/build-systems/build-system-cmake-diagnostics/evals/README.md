# Evaluation — build-system-cmake-diagnostics

Skill: `skills/build-systems/build-system-cmake-diagnostics`.
Toolchain: CMake 4.4.0, Ninja 1.13.2, GCC 16.1.0 (MSYS2 ucrt64, PE/COFF),
pkg-config 2.5.1, zlib 1.3.2. All commands recorded 2026-08-15.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/CMakeLists.txt` | configure fails, names missing target | exit 1 |
| medium/negative | `bad/hand_rolled.cmake` | rejected: absolute paths, invented version | review |
| medium/negative | `bad/` compile symptom | `greet.h` missing = missing dep target, not missing file | exit 1 |
| easy/positive | `good/CMakeLists.txt` | local target, configure+build+run | exit 0 |
| easy/positive | `good/with_find_package/CMakeLists.txt` | pkg-config imports zlib | exit 0 |

## Actual verification runs (recorded 2026-08-15)

```
cmake -G Ninja -S examples/bad -B bad/build
  CMake Error at CMakeLists.txt:15 (target_link_libraries):
    The link interface of target "dep" contains:
      greet::greet
    but the target was not found.  Possible reasons include:
      * There is a typo in the target name.
      * A find_package call is missing for an IMPORTED target.
      * An ALIAS target is missing.
  exit 1

cmake -G Ninja -S examples/bad -B bad/build   (bare-name variant)
  configure exit 0  (CMake treats "greet" as -lgreet)
cmake --build bad/build
  C:/.../app.c:1:10: fatal error: greet.h: No such file or directory
  ninja: build stopped: subcommand failed.
  exit 1    # same root cause, different symptom layer

cmake -G Ninja -S examples/good -B good/build   exit 0
cmake --build good/build
  [1/4] Building C object CMakeFiles/greet.dir/greet.c.obj
  [2/4] Building C object CMakeFiles/app.dir/app.c.obj
  [3/4] Linking C static library libgreet.a
  [4/4] Linking C executable app.exe
  exit 0
good/build/app.exe  ->  hello, troth   (exit 0)

cmake -G Ninja -S examples/good/with_find_package -B fp/build
  -- Found PkgConfig: C:/msys64/ucrt64/bin/pkg-config.exe (found version "2.5.1")
  -- Checking for module 'zlib'
  --   Found zlib, version 1.3.2
  exit 0
cmake --build fp/build
  [1/2] Building C object CMakeFiles/app.dir/app.c.obj
  [2/2] Linking C executable app.exe
  exit 0
fp/build/app.exe  ->  compressed 5 bytes into 13   (exit 0)

cmake --graphviz=graph.dot examples/good
  graph.dot contains nodes: "app" (egg) and "greet" (octagon)

cmake --trace-expand -S examples/good -B trace
  .../CMakeLists.txt(6):  add_library(greet STATIC greet.c )
  .../CMakeLists.txt(14): target_link_libraries(app PRIVATE greet )

ninja -C good/build -t deps
  CMakeFiles/greet.dir/greet.c.obj: #deps 12, deps mtime ... (VALID)
      .../greet.c
      .../greet.h
```

## Verified facts

- A namespaced name in a PUBLIC link interface produces the canonical
  target-not-found error at configure time (exit 1). KNOWN.
- A bare name does NOT produce it: CMake 4.4 silently emits `-lgreet`, and the
  first real symptom is the missing header at compile time. KNOWN (recorded).
- `find_package(ZLIB)` fails in MSYS2 ucrt64 with default paths
  (`Could NOT find ZLIB (missing: ZLIB_LIBRARY ZLIB_INCLUDE_DIR)`, exit 1) while
  the same environment's pkg-config finds zlib 1.3.2 — the failure is
  environment search paths, not a missing package. KNOWN.
- `--trace-expand` prints post-expansion commands; `--graphviz` writes real
  target edges; `ninja -t deps` shows the real dep store. KNOWN.
- pkg-config EXE path returned by CMake was `C:/msys64/ucrt64/bin/pkg-config.exe`
  — platform-specific, do not hardcode in any recipe.

## False-positive evals (correct code must not be flagged)

- `good/CMakeLists.txt` — valid local target with PUBLIC include dir: must pass.
- `good/with_find_package/CMakeLists.txt` — valid imported-target usage: must pass.
- A `find_package(ZLIB)` that fails on a foreign prefix must be diagnosed as a
  toolchain-env issue, NOT as "the package is missing" or "CMake is broken".

## Historical evals

- bnnet/bsder 2025: agent "fixed" a dependency by inventing versions instead of
  reading the configure error — replay with `bad/CMakeLists.txt`: the correct
  output is the error text verbatim, then one find/define step.
- TensorRT/minimaxir mock-benchmark 2025: agent declared success without a real
  configure+link — gate on `ninja -t commands` showing real `cc` lines + run.

## Adversarial evals

- `bad/hand_rolled.cmake`: looks like a plausible fix (include_directories +
  target_link_libraries) but hardcodes invented absolute paths. Must be rejected
  even if it compiles on this machine, because it is not reproducible.

## Scoring (for routing eval)

- precision: every flag maps to a named reference rule (1-7).
- recall: bad CMakeLists, hand-rolled logic, env-path find_package failure, and
  fake-success claims are all caught.
- FP-rate: valid local-target and pkg-config builds produce zero flags.

## Target toolchains (absent, documented)

- `clang`/`lld`: not installed; the target-graph rules are generator-agnostic and
  hold for Make/VS generators without rerunning.
