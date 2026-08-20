---
name: binary-hardening-flags
description: Use when building or reviewing binaries for security — RELRO, BIND_NOW, PIE/ASLR, stack canaries, FORTIFY, CFI, CET, PAC/BTI, shadow stack. Teaches which compiler/linker flags produce which binary properties and how to verify them instead of assuming defaults.
---

# Binary Hardening Flags: Flag, Property, and Verification Triad

## When to use

- Building a release binary (ELF, PE) that must survive memory-corruption
  exploitation: which flags, on which command line, and how to prove the
  produced file actually has the property.
- Reviewing a build system, Makefile, CMakeLists, or CI script that claims
  hardening flags are applied.
- Answering "is this binary PIE / full RELRO / canaried / fortified?" for a
  binary you did not compile.
- Checking a claim that a protection is on because a distro "defaults to it".
- Porting a hardening policy between Linux ELF, MinGW PE, and Windows-native
  MSVC toolchains.
- Auditing a CVE report where the mitigation claim is "we compile with -fstack
  -protector-strong" — verify the binary, not the Makefile.

## When not to use

- Designing a full security policy or threat model — this skill is about the
  flag-to-property mapping and its verification, not policy selection.
- Kernel/embedded firmware where the toolchain does not target the standard
  userland ABI (e.g. bare-metal ARM with no dynamic loader) — PIE, RELRO and
  FORTIFY are dynamic-loading features.
- Reviewing source code for logic vulnerabilities (overflow *finding* is the
  domain of `c-string-and-buffer-safety`; this skill covers the mitigations).
- Interpreting a debug-symbol-stripped production binary when the goal is
  reverse engineering rather than mitigation auditing.
- Performance tuning where the security trade-off is already decided and
  documented.

## What the agent often gets wrong

1. Believing a protection is applied because the flag compiled — flags are
   silently ignored: unsupported target (e.g. `-fsanitize=cfi` on GCC), too-old
   linker (CET needs binutils >= 2.33), or the flag on the WRONG command line
   (compile-only flag passed to the linker and vice versa).
2. Claiming ASLR protects code that is not PIE. Only an `ET_DYN` executable
   gets a randomized load base; `ET_EXEC` runs at fixed addresses.
3. Claiming RELRO without checking BIND_NOW. Partial RELRO leaves `.got.plt`
   writable, which is exactly what GOT-overwrite exploits target.
4. Claiming canaries protect everything. Under `-fstack-protector-strong`
   only functions with arrays/addresses-taken locals are instrumented, and
   canaries do not protect the heap, globals, or intra-frame member overflow.
5. Writing `-fstack-protector` (weak) and reporting "canaries enabled" as if it
   were strong; or assuming a distro's default applies to a manually built
   binary.
6. Forgetting FORTIFY needs `-O1+` (ignored at `-O0`) and that
   `_FORTIFY_SOURCE` must be defined before any system-header include.
7. Assuming CET / PAC / BTI are enabled by default or supported by the
   toolchain. They need modern binutils/glibc AND CPU support, and produce no
   `.note.gnu.property` otherwise — without any error.
8. Verifying the build command instead of the binary. "The flag was in the
   build command" is not evidence; `readelf`/`objdump`/`checksec` output is.

## How to reason correctly

1. State the protection you want, then choose the exact flags AND where each
   goes (compile step vs link step). Use the table in
   `references/hardening-flags.md`.
2. Compile, then verify on the produced binary with `readelf`/`objdump` (or
   `checksec`) and record the evidence. Never skip the verify step.
3. Treat as baseline: PIE (`-fPIE -pie`) + full RELRO
   (`-Wl,-z,relro,-z,now`) + `-fstack-protector-strong` + `-D_FORTIFY_SOURCE=2`
   at `-O1+` + `-Wl,-z,noexecstack`. CFI / CET / PAC / BTI are layered on top
   where the toolchain and CPU support them.
4. On ARM64 use `-mbranch-protection=standard` (BTI + PAC) and verify the
   `.note.gnu.property` with `readelf -n`.
5. For libraries: `-fPIC` + full RELRO (`-z now`); PIE applies only to
   executables.
6. If `readelf`/`checksec` are unavailable (e.g. PE-only toolchain), say so and
   fall back to `objdump`-verifiable properties (canary symbol, `__*_chk`
   calls, `endbr64` markers), and document ELF-only checks as target commands.

## What to verify

- Binary-level properties: `Type: DYN` (PIE), `GNU_RELRO` segment + `BIND_NOW`
  (full RELRO), `__stack_chk_fail` symbol (canary), `__*_chk` calls
  (FORTIFY), `.note.gnu.property` IBT/SHSTK (CET), `GNU_STACK` without
  `E` (noexec), and no segment that is both writable and executable.
- That BOTH compile and link got their flags — two separate command lines to
  inspect, not one.
- FORTIFY compiled at `-O1+`; CFI built with LTO (it is LLVM-only).
- On a stripped binary, `__stack_chk_fail` may be a dynamic import/undefined
  symbol — still findable in the dynamic symbol table.

## How to verify

Target (Linux, ELF):

```
checksec --file=./app
readelf -l -d -h ./app      # GNU_RELRO + BIND_NOW + Type: DYN
readelf -n ./app            # .note.gnu.property: x86 feature: IBT, SHSTK
objdump -t ./app | grep __stack_chk_fail
objdump -d ./app | grep _chk    # __strcpy_chk etc. (FORTIFY)
objdump -d ./app | grep endbr64 # CET IBT landing pads
```

This host (Windows, MinGW gcc 16.1 + binutils 2.46; `readelf` rejects PE
files):

```
gcc -O2 -fstack-protector-strong -fcf-protection=full -D_FORTIFY_SOURCE=2 \
    -o hardened.exe examples/good/hardened.c
gcc -o plain.exe examples/bad/plain.c
objdump -t hardened.exe | grep stack_chk        # present (canary)
objdump -d hardened.exe | grep _chk             # __strcpy_chk (FORTIFY)
objdump -d hardened.exe | grep endbr64          # IBT marker at main
objdump -d plain.exe | grep -c endbr64          # 0 (no CET markers)
# canary detection demo (same source, with/without the flag):
gcc -O2 -fstack-protector-strong -o canary_on.exe examples/bad/canary_bypass.c
gcc -O2 -o canary_off.exe examples/bad/canary_bypass.c
./canary_on.exe   # "stack smashing detected", exit 0xC0000409
./canary_off.exe  # silent, exit 0
python examples/tools/hardening_audit.py examples/good/*.txt   # PASS, exit 0
python examples/tools/hardening_audit.py examples/bad/*.txt    # FAIL, exit 1
```

The audit script consumes saved `readelf`/`objdump`/`checksec` output text and
reports PIE, RELRO (full/partial), BIND_NOW, canary, FORTIFY, NX, CET and
PAC/BTI, flagging missing protections. Real host outputs and exit codes are
recorded in `evals/README.md`.

## Where the knowledge comes from

- GCC security options (https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)
- LLVM Control-Flow Integrity docs (https://clang.llvm.org/docs/ControlFlowIntegrity.html)
- CET (Intel) / Shadow Stack docs, glibc hardening docs (https://sourceware.org/glibc/manual/), 'checksec' tool (https://github.com/slimm609/checksec.sh)
- Arm BTI/PAC (https://developer.arm.com/documentation/109576/latest/)
- Linker options -z relro/-z now (https://sourceware.org/binutils/docs/ld/Options.html)

## Related skills

- `compiler-ub-assumptions` — the optimizer changes observable behavior, so
  security properties must be checked on object code, not source.
- `build-toolchain-version-drift` — which flag works depends on the exact
  compiler/linker; record the version like the CET/binutils 2.33 boundary.
- `c-string-and-buffer-safety` — the overflow bugs that canaries and FORTIFY
  mitigate, and what FORTIFY aborts on.
- `c-undefined-behavior` — why overflow and signed issues interact with
  optimizer decisions in hardened builds.
- `build-linker-error-diagnostics` — link-step flags (`-Wl,`) and diagnosing
  when a linker option is ignored or wrong.
- `meta-verification` — verify claims on the artifact, not the intent; the
  binary is the artifact.
- `secure-boot-chain` — how binary properties fit into a wider verified
  execution chain (signing, measured boot).

## Evaluation

- Synthetic: give the agent a build command list and ask which protections
  each produced binary has; or give it `readelf`/`objdump` text and ask to
  identify the missing protections. The checker script and sample outputs in
  `examples/` are the fixture.
- False-positive: a PIE executable with partial RELRO must be reported as
  "PIE yes, full RELRO no", NOT as fully hardened; a PE binary must not be
  reported as PIE/RELRO-enabled just because the flags compiled.
- Historical: CVE classes mitigated by each control (see
  `evals/README.md`), the 2016-era default "partial RELRO" builds, and
  BIND_NOW bypass scenarios.
- Adversarial: a build system that passes `-fstack-protector-strong` to the
  linker (silently ignored); `-fcf-protection=full` accepted but emitting no
  `.note.gnu.property` (as on this host's PE target); a Makefile that defines
  `_FORTIFY_SOURCE` after includes.
- Verified facts and commands actually run on this host:
  `evals/README.md`.
