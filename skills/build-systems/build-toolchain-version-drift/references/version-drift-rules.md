# Toolchain Version Drift — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. One effective standard per compile; find it, don't assume it

- **RULE**: every translation unit is compiled with exactly one effective
  language standard. Its value comes from the flags that actually ran
  (`-std=`), the compiler driver default, or the build system's pin
  (`CXX_STANDARD`, `add_compile_options`). You must be able to name it.
- **WHY AI GETS IT WRONG**: blames GCC for rejecting code that needs a newer
  standard; or assumes "no -std flag means latest"; or edits the source instead
  of the pin.
- **CORRECT REASONING**: read the error — GCC names the required flag
  (`'non-type template parameters of class type only available with
  -std=c++20 or -std=gnu++20'`). Then find the *actual* compile command
  (`ninja -t commands`, verbose make, `cmake --build --verbose`) and the active
  macro dump (`-dM -E` → `__cplusplus`). Fix the pin, not the code.
- **EXAMPLE** (bad):
  ```
  gcc -std=c++17 bad/nttp.cpp
    error: non-type template parameters of class type only available
           with '-std=c++20' or '-std=gnu++20'
    (agent rewrites the template; the pin was the bug)
  ```
- **COUNTEREXAMPLE** (good):
  ```
  gcc -std=c++20 good/nttp.cpp     # exit 0; program runs, exit 0
  gcc -x c++ -std=c++20 -dM -E - | grep __cplusplus   # 202002L
  ```
- **VERIFICATION**: recorded 2026-08-15 — `-std=c++17` exit 1 with the
  flag-naming error; `-std=c++20` exit 0; `__cplusplus` 201703L vs 202002L.
- **SOURCE**: gcc-manual (-std=, preprocessor options), clang-docs (same flags).

## 2. Defaults drift; silent extension mode is the dangerous case

- **RULE**: GCC's default C++ standard changed across major versions and can be
  gnu++17 or gnu++20 (on this toolchain GCC 16.1.0 defaults to gnu++20:
  `__cplusplus` is 202002L with no -std flag). Features newer than the default
  may be accepted as extensions with a warning — "compiles" does not mean
  "compiles per the standard you intended".
- **WHY AI GETS IT WRONG**: sees a warning-free build and assumes the standard
  matches intent; or sees a warning and dismisses it; or ports a recipe written
  for an older default.
- **CORRECT REASONING**: `if consteval` (C++23) compiled in gnu++20 mode is
  accepted only via `-Wc++23-extensions` warning — recorded on this toolchain.
  Pin `-std=` explicitly so the mode cannot drift when the toolchain changes.
- **EXAMPLE** (bad): `gcc bad/c23.cpp` (no -std) → warning, exit 0, semantics
  silently downgraded.
- **COUNTEREXAMPLE** (good): `gcc -std=c++23 good/c23.cpp` → exit 0, semantics
  as intended.
- **VERIFICATION**: recorded 2026-08-15 — `if consteval` at default gnu++20
  produces `warning: 'if consteval' only available with '-std=c++23'` (exit 0);
  with explicit `-std=c++23` exit 0, no warning.
- **SOURCE**: gcc-manual (-std= defaults, -Wc++NN-extensions), clang-docs.

## 3. Identical -O binaries mean the flag did not apply

- **RULE**: different `-O` levels produce different code for non-trivial
  sources. If `-O0` and `-O3` outputs are identical, the optimization flag
  never reached the compiler (stale object, wrong recipe, no rebuild ran).
- **WHY AI GETS IT WRONG**: concludes "the compiler can't optimize this" or
  "optimization doesn't matter", then ships the unoptimized binary; or reports
  a perf "fix" as successful while producing a byte-identical artifact.
- **CORRECT REASONING**: compare the code-bearing section, not the file:
  `objcopy -O binary --only-section=.text o0.exe o0.text`, then `cmp`.
  Recorded: the demo source's `.text` is 6784 B at -O0 vs 6800 B at -O2/-O3
  and `fc/cmp` reports them different; `-O2` vs `-O3` are identical here (the
  loop already optimized the same way) — so O2==O3 alone is NOT a bug, but
  O0==O3 IS.
- **EXAMPLE** (bad): `bad/stale_opt.sh` — `gcc -O0 opt.c -o app.exe` then
  `echo "built with -O3: OK"`.
- **COUNTEREXAMPLE** (good): `good/verify_opt.sh` — builds all levels, dumps
  `.text`, compares, prints "OK: -O0 and -O3 .text differ".
- **VERIFICATION**: recorded 2026-08-15 — `cmp o0.text o2.text` exit 1
  (differ); `cmp o2.text o3.text` exit 0; full `.exe` `cmp` exit 1 even for
  two identical-flag builds (PE header noise). O0 main body: `push %rbp / mov
  %rsp,%rbp / sub $0x30,%rsp / ...` vs O2: `sub $0x38,%rsp / ... / xor
  %eax,%eax`.
- **SOURCE**: gcc-manual (-O levels), binutils-docs (objcopy --only-section).

## 4. Never `cmp` whole binaries; compare code sections

- **RULE**: executables carry non-code metadata that differs between builds of
  the same source: PE `TimeDateStamp` in the header, paths, timestamps.
  `cmp file1 file2` on executables reports differences that have nothing to do
  with the code.
- **WHY AI GETS IT WRONG**: "identical optimization" conclusions are drawn from
  whole-file hashes; or "the build changed" is proven by a hash that only the
  header explains.
- **CORRECT REASONING**: compare what you are reasoning about — `.text`
  (`objcopy --only-section=.text`), `.rodata`, or normalized disassembly
  (`objdump -d`). Recorded: two `-O3` builds of the same source from the same
  flags produced different whole `.exe` files (cmp exit 1) but identical
  `.text` (cmp exit 0).
- **EXAMPLE** (bad): `cmp o3.exe o3b.exe` → "binaries differ, the rebuild
  changed something" — the change is the PE header timestamp.
- **COUNTEREXAMPLE** (good): `cmp o3.text o3b.text` → identical → the code is
  unchanged, the build is a no-op.
- **VERIFICATION**: recorded 2026-08-15 — full-exe cmp exit 1, .text cmp exit
  0 for same-flag rebuilds.
- **SOURCE**: binutils-docs (objcopy, objdump), gcc-manual (PE target notes).

## 5. Check the toolchain pair: compiler, runtime, headers

- **RULE**: a build is a tuple (compiler version, standard, runtime/ABI,
  headers, libraries). Drift in any element breaks the build in ways that look
  like code bugs: stale headers vs newer runtime, ABI symbol version
  mismatches, API moves between library releases.
- **WHY AI GETS IT WRONG**: "fixes" the symptom (a call that no longer exists,
  a symbol that fails to link) by rewriting code or pinning a random version,
  instead of recording the toolchain tuple.
- **CORRECT REASONING**: record `gcc --version` and
  `gcc --print-file-name=libstdc++.a` (resolved runtime path — recorded:
  `C:/msys64/ucrt64/.../lib/libstdc++.a`), then check ABI macros. On Linux,
  GLIBCXX symbol versions (`GLIBCXX_3.4.3x`) are stamped into linked objects;
  on this MinGW toolchain there are no versioned GLIBCXX symbols — a platform
  difference the agent must not paper over.
- **EXAMPLE** (bad): linking against headers from a newer libstdc++ than the
  runtime provides, then "fixing" the resulting crash by changing application
  code.
- **COUNTEREXAMPLE** (good): `gcc --print-file-name` reveals the resolved
  library; the header/runtime pair is verified before blaming user code.
- **VERIFICATION**: recorded 2026-08-15 — `gcc --print-file-name=libstdc++.a`
  resolved to the ucrt64 toolchain library path; `gcc --version` = 16.1.0.
- **SOURCE**: gcc-manual (--print-file-name), glibc-abi (GLIBCXX versioning,
  dual ABI), clang-docs.

## 6. Record the compiler identity before diagnosing

- **RULE**: start any toolchain diagnosis by recording `gcc --version` (exact
  build string) and the preprocessor identity macros (`__GNUC__`,
  `__GNUC_MINOR__`, `__GNUC_PATCHLEVEL__`, `__VERSION__`, `__cplusplus`).
- **WHY AI GETS IT WRONG**: states "GCC is broken" or "GCC 12 does X" from
  memory; a fix that works on one exact build may fail on another.
- **CORRECT REASONING**: a `-dM -E` dump is ground truth for what the
  compiler thinks it is and which standard is active. Recorded on this
  toolchain: `__GNUC__ 16`, `__GNUC_MINOR__ 1`, `__GNUC_PATCHLEVEL__ 0`,
  `__VERSION__ "16.1.0"`, `__x86_64__ 1`, default `__cplusplus 202002L`.
- **EXAMPLE** (bad): "GCC won't accept my NTTP — GCC is broken."
- **COUNTEREXAMPLE** (good): "gcc 16.1.0 with -std=c++17 gives __cplusplus
  201703L; class-type NTTP requires -std=c++20." Then fix the pin.
- **VERIFICATION**: recorded 2026-08-15 — full macro dump in evals/README.md.
- **SOURCE**: gcc-manual (-dM, -E, identity macros), glibc-abi.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Standard | one effective `-std=` per TU; find it, never assume it |
| Errors | GCC names the required flag; fix the pin, not the code |
| Defaults | GCC default drifts (gnu++17/gnu++20); pin explicitly |
| Extensions | newer features compile as warnings; silent semantic downgrade |
| -O identical | `-O0`==`-O3` .text means the flag never applied |
| Compare | `.text` sections, never whole executables (PE timestamp) |
| Toolchain tuple | compiler + standard + runtime + headers must agree |
| Ground truth | `gcc -dM -E`, `gcc --version`, `--print-file-name` |
