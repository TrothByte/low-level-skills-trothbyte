# Evaluation — build-toolchain-version-drift

Skill: `skills/build-systems/build-toolchain-version-drift`.
Toolchain: GCC 16.1.0 (MSYS2 ucrt64, x86_64-w64-mingw32, PE/COFF), GNU binutils
2.46 (objcopy/nm/objdump), no glibc (MinGW runtime — GLIBCXX versioned symbols
do not appear). All commands recorded 2026-08-15.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/nttp.cpp` + `-std=c++17` | error names `-std=c++20` | exit 1 |
| easy/positive | `good/nttp.cpp` + `-std=c++20` | builds and runs | exit 0 |
| medium/negative | `bad/c23.cpp` no `-std` | silent extension downgrade warning | exit 0, must flag |
| medium/negative | `bad/stale_opt.sh` | claims `-O3`, compiles `-O0` | detected via .text cmp |
| easy/positive | `good/verify_opt.sh` | O0 vs O3 .text differ, macros dumped | O0 6784B vs O2/O3 6800B |

## Actual verification runs (recorded 2026-08-15)

```
gcc --version
  gcc.exe (Rev5, Built by MSYS2 project) 16.1.0

gcc --print-file-name=libstdc++.a
  C:/msys64/ucrt64/bin/../lib/gcc/x86_64-w64-mingw32/16.1.0/../../../../lib/libstdc++.a

gcc -std=c++17 bad/nttp.cpp
  error: non-type template parameters of class type only available
         with '-std=c++20' or '-std=gnu++20'
  exit 1

gcc -std=c++20 good/nttp.cpp
  exit 0; run exit 0

gcc -x c++ -std=c++17 -dM -E ... | grep __cplusplus   -> 201703L
gcc -x c++ -std=c++20 -dM -E ... | grep __cplusplus   -> 202002L
default (no -std) C++ macro dump                      -> 202002L (gnu++20)

gcc -dM -E (C, no -std): __GNUC__ 16, __GNUC_MINOR__ 1, __GNUC_PATCHLEVEL__ 0,
  __VERSION__ "16.1.0", __x86_64__ 1

gcc bad/c23.cpp (no -std)
  warning: 'if consteval' only available with '-std=c++23' or '-std=gnu++23'
           [-Wc++23-extensions]
  exit 0   # silent downgrade; semantics differ from intent

gcc -std=c++23 good/c23.cpp   exit 0

gcc -O0 opt.c -o o0.exe   (138510 B)   .text 6784 B
gcc -O2 opt.c -o o2.exe   (138546 B)   .text 6800 B
gcc -O3 opt.c -o o3.exe   (138546 B)   .text 6800 B

cmp o0.text o2.text    differ (exit 1)
cmp o2.text o3.text    identical (exit 0)   # normal for this loop
cmp o3.exe o3b.exe     differ (exit 1)      # two -O3 builds, same code:
                                             # PE header timestamp/paths
cmp o3.text o3b.text   identical (exit 0)

-O0 main:  push %rbp; mov %rsp,%rbp; sub $0x30,%rsp; call __main;
          movl $0x0,-0x8(%rbp); movl $0x0,-0x4(%rbp); ...
-O2 main:  sub $0x38,%rsp; call __main; movl $0x0,0x2c(%rsp); xor %eax,%eax; ...
```

## Verified facts

- GCC 16.1.0 default C++ standard is gnu++20 (`__cplusplus` 202002L with no
  flag). KNOWN (recorded).
- Class-type NTTP with `-std=c++17` produces the flag-naming error; with
  `-std=c++20` it builds. KNOWN (recorded).
- `if consteval` in default gnu++20 mode is a `-Wc++23-extensions` WARNING with
  exit 0, not an error — silent semantic downgrade. KNOWN (recorded).
- `-O0` vs `-O2/-O3` `.text` differ for the demo source; `-O2` vs `-O3` are
  identical. Full `.exe` `cmp` differs even for identical-flag builds (PE
  header noise). KNOWN (recorded).
- MSYS2 ucrt64 has no glibc; GLIBCXX versioned symbols (the glibc-abi model)
  are absent here. The dual-ABI/versioning rules apply on Linux hosts. KNOWN.

## False-positive evals (correct builds must not be flagged)

- `good/nttp.cpp` with `-std=c++20` — valid, must pass.
- `-O2` == `-O3` `.text` for a loop that folds identically — NORMAL, must not
  be reported as a build-config bug (only `-O0` == `-O3` is suspicious).
- Explicit `-std=c++23` build of `c23.cpp` — valid, must pass.

## Historical evals

- r/cpp_questions C++20 NTTP "blamed on GCC": the correct diagnosis is the
  `-std=` pin (error text names the flag), not a rewrite of the template.
- CCC "identical `-O0/-O2/-O3` binaries": the correct finding is "the flag
  never reached the compiler", proven by `.text` comparison, not "optimization
  doesn't matter".
- Stale TensorRT API after toolchain upgrade: replay as a toolchain-tuple check
  (`--version`, `--print-file-name`, headers vs runtime) instead of an API
  rewrite.

## Adversarial evals

- `bad/stale_opt.sh` is a plausible "build" that echoes success. Detect: run
  the recipe, dump `.text`, compare with a real `-O3` build — the difference is
  the smoking gun. Any "verified" perf result built on a misapplied flag is
  rejected.

## Scoring (for routing eval)

- precision: every flag maps to a reference rule (1-6).
- recall: std pin errors, silent extension mode, stale-flag builds, whole-exe
  `cmp` misuse, and toolchain-tuple drift are all caught.
- FP-rate: valid pinned builds and normal O2==O3 equality produce zero flags.

## Target toolchains (absent, documented)

- `clang`: not installed; the same flags (`-std=`, `-dM -E`, `-O`) apply and
  are the planned second pass; rules are cross-checked against clang-docs.
- Linux/glibc: GLIBCXX symbol-version verification (`nm -D`, `objdump -T`)
  requires a glibc host — documented as target verification.
