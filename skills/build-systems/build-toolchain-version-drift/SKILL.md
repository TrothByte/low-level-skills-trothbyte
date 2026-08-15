---
name: build-toolchain-version-drift
description: Use when a build fails or "misbehaves" due to compiler, libstdc++/ABI, or -std= version drift, or when -O levels produce identical binaries. Teaches pinning the standard, dumping compiler macros, and proving which flags actually reached the compile command.
---

# Toolchain Version Drift: Check the Compiler and the -std Before Blaming the Code

## When to use

- A C++ feature fails with an error that names a `-std=` flag you never passed.
- "GCC is broken" reports about code that actually needs a newer/older standard.
- `-O0/-O2/-O3` appear to produce identical binaries and the agent concludes
  optimization is pointless.
- ABI/library mismatches: stale headers vs newer runtime, GLIBCXX symbol
  versions, `__cplusplus` / `__GNUC__` disagreements.
- You need to prove which compiler, standard, and flags actually ran.

## When not to use

- Link-stage `undefined reference` / symbol-version problems — use
  `build-linker-error-diagnostics`.
- CMake target/dependency misdeclaration — use `build-system-cmake-diagnostics`.
- Corrupted build state after a signal — use `build-process-signal-and-state-safety`.

## What the agent often gets wrong

- Blaming GCC for rejecting code when the real cause is `-std=c++17` pinned
  somewhere in the build (C++20 NTTP case: the error names `-std=c++20`).
- Assuming the default standard — GCC's default changes across versions and can
  be `gnu++17` or `gnu++20`; code silently compiled as an extension mode is the
  dangerous middle ground (warnings, not errors).
- Treating identical `-O0/-O2/-O3` binaries as "the compiler can't optimize";
  identical output means the flag never reached the compiler (stale object,
  wrong recipe, no rebuild).
- `cmp`-ing whole executables: PE/ELF headers carry timestamps and paths, so
  two identical-code builds compare unequal — the wrong evidence tool.
- Assuming the installed header matches the installed runtime (stale API after
  a toolchain upgrade, e.g. a TensorRT API that moved between releases).

## How to reason correctly

1. Pin it: every compile has exactly one effective standard. Find it in the real
   command (`ninja -t commands`, `cmake --build` verbose, Makefile, `gcc -v`),
   never in memory.
2. Confirm with a macro dump: `gcc -dM -E` prints `__cplusplus`, `__GNUC__`,
   `__GNUC_MINOR__`, `__VERSION__`. Compare the dump against what the error
   text requires.
3. Blame the configuration: an error naming `-std=c++23` is a pin problem.
   Change `CXX_STANDARD`/the flag, not the source.
4. For optimization claims, compare `.text` sections (`objcopy --only-section`
   / `objdump -d`), never whole files. Identical `.text` at different `-O` =
   the flag did not apply.
5. Check the toolchain pair: `gcc --version`, `gcc --print-file-name` for the
   resolved library/header paths, and the ABI macros — stale headers against a
   newer runtime produce link or runtime failures that look like code bugs.

## What to verify

- The effective `-std=` in the real compile command and the `__cplusplus` value
  from `-dM -E` agree with what the code needs.
- NTTP/class-type template code compiles at `-std=c++20` and fails at `-std=c++17`
  with the error naming the required flag.
- `-O0` and `-O2/-O3` `.text` sections differ for the demo source; full-exe
  `cmp` differences are explained by headers, not code.
- `gcc --version` and `--print-file-name` outputs recorded for the toolchain.
- The ABI-version macros (`__cplusplus`, `__GNUC__*`) match the toolchain.

## How to verify

```
gcc --version                                  # exact compiler, record it
gcc --print-file-name=libstdc++.a              # resolved runtime path
printf 'int main(){}' | gcc -x c++ -std=c++20 -dM -E - | grep __cplusplus
gcc -std=c++17 bad/nttp.cpp                    # error names -std=c++20
gcc -std=c++20 good/nttp.cpp                   # exit 0
gcc -O0 good/opt.c -o o0.exe                   # then -O2, -O3
objcopy -O binary --only-section=.text o0.exe o0.text   # compare .text only
cmp o0.text o2.text                            # differ => flag took effect
```

## Where the knowledge comes from

- `gcc-manual` — -std=, -O levels, -dM/-E preprocessor dumps, --print-file-name.
- `clang-docs` — the same flags for the other major compiler; cross-check claims.
- `glibc-abi` — libstdc++ ABI versioning (GLIBCXX symbols, dual ABI); note this
  toolchain is MinGW (ucrt64), so no GLIBCXX versioned symbols appear here.

## Related skills

- `build-linker-error-diagnostics` — symbol/ABI failures at link time
- `build-system-cmake-diagnostics` — `CXX_STANDARD` and dependency declaration
- `build-process-signal-and-state-safety` — why a rebuild may not actually run
- `rust-api-evolution-and-drift` — the same drift problem for Rust editions

## Evaluation

- Synthetic: bad `-std=c++17` NTTP build must fail with the error naming
  `-std=c++20`; good build must succeed and run; `stale_opt.sh` must be caught.
- False-positive: valid `-std=c++20` code and a real `-O3` build (differing
  `.text`) must NOT be flagged as drift.
- Historical: replay the C++20-NTTP "blamed on GCC" case: the correct output is
  the pin diagnosis, not a code rewrite.
- Adversarial: `stale_opt.sh` claims `-O3` while compiling at `-O0` — detect via
  `.text` comparison, reject the "optimization did nothing" conclusion.
- Verified facts (actual runs recorded 2026-08-15): `evals/README.md`.
