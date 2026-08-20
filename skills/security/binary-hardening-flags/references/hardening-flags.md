# Hardening flags — flag → property → verification map

The systematic map. For every protection: which flags, WHERE they go (compile
vs link), which binary property results, and how to verify it on the produced
binary. The final section lists the failure modes (flags silently ignored).

## 1. RELRO and BIND_NOW (full RELRO)

- Partial RELRO (default on many distros): `-Wl,-z,relro`.
  Read-only after relocation: the linker sorts `.got`/`.data.rel.ro` etc. into
  the `GNU_RELRO` PT_LOAD mapping, which the loader remaps read-only before
  transferring control. `.got.plt` (lazy PLT stubs) stays writable, so a
  partial-RELRO binary can still be hit by GOT overwrite.
- Full RELRO: `-Wl,-z,relro,-z,now` (i.e. add BIND_NOW).
  Immediate binding resolves all `PLT` slots at startup and `.got.plt` is
  folded into the read-only `GNU_RELRO` range.
- Verify:
  - `readelf -l ./app` shows a `GNU_RELRO` segment and `readelf -d ./app`
    shows `(BIND_NOW)` / `FLAGS: BIND_NOW`.
  - `.got` and `.got.plt` sections both inside the GNU_RELRO mapped sections.
  - `checksec --file=./app` column RELRO reads `Full RELRO`.
- Position: both are LINK flags (passed via `-Wl,`). The compile step is
  irrelevant to RELRO.

## 2. PIE (ASLR for code)

- Flags: `-fPIE -pie` — `-fPIE` on the compile step, `-pie` on the link step.
  Both are required; only-linking with `-pie` (or only compiling with `-fPIE`)
  does not produce a PIE. On gcc, `-pie` alone at link forces `-fPIE` compile
  semantics only if no non-PIC object conflicts, so pass both explicitly.
- Property: an ET_DYN executable. ASLR only randomizes the load base of
  ET_DYN files; an ET_EXEC (non-PIE) binary is loaded at fixed addresses, so
  ASLR is defeated for code.
- Verify: `readelf -h ./app` → `Type: DYN (Position-Independent Executable
  file)`. ET_EXEC means no PIE.
- Libraries: use `-fPIC` (shared objects are position-independent by nature);
  PIE applies to executables only.

## 3. Stack canaries

- Flags: `-fstack-protector` (weak), `-fstack-protector-strong` (default on
  many distros), `-fstack-protector-all` (all functions). "Strong" instruments
  functions with arrays or local addresses taken; "weak" only functions that
  call `alloca` or have a local `char[]` > 8 bytes (approximately).
- Property: functions with vulnerable locals load the canary from
  `__stack_chk_guard` at entry and compare before return; mismatch calls
  `__stack_chk_fail` (abort, exit code 0xC0000409 STATUS_STACK_BUFFER_OVERRUN
  on Windows / SIGABRT on Linux).
- Verify:
  - `objdump -t ./app | grep __stack_chk_fail` — symbol present (also
    `__stack_chk_guard`).
  - `objdump -d ./app` shows `fs:0x28` (x86-64 Linux) load and a
    `call __stack_chk_fail` after the mismatch branch.
- Scope: canaries do NOT protect the heap, globals, or structure members
  overflowed within the frame; they detect smashing of the return path only.

## 4. FORTIFY (_FORTIFY_SOURCE)

- Flags: `-D_FORTIFY_SOURCE=2` together with `-O1` or higher (it is ignored
  at `-O0`; GCC prints "`_FORTIFY_SOURCE` requires compiling with
  optimization"). It must be defined before the first system-header include
  (define it on the command line). Level 2 also adds checks on
  `%s`-format specifiers etc.
- Property: calls to `strcpy`/`strcat`/`sprintf`/`memcpy`/`read`/`write` with
  compiler-known destination sizes are redirected to `__*_chk` variants that
  receive the destination capacity and abort on overflow (and on valid
  glibc>=2.39, may not abort on small underflows — report them).
- Verify: `objdump -d ./app | grep _chk` shows `__strcpy_chk`,
  `__sprintf_chk`, etc.
- Known on MinGW: `gcc -O2 -D_FORTIFY_SOURCE=2` redirects `strcpy` to
  `__strcpy_chk` (verified on this host, gcc 16.1); not every libc function is
  fortified.

## 5. CFI (Clang control-flow integrity)

- Flags: `-fsanitize=cfi` together with `-flto` and `-fvisibility=hidden`
  (all three required; CFI works on the merged LTO IR, so all objects AND the
  link step need `-flto`). LLVM-only — GCC has no `-fsanitize=cfi`.
- Property: indirect-call/indirect-branch targets are checked against the
  class of the callee; invalid targets trap instead of executing.
- Verify: `clang -fsanitize=cfi -flto -fvisibility=hidden` compiles and links;
  the produced binary aborts on a forged indirect call. Binary-level markers
  are subtle; the build command and a negative test (bad function pointer call
  traps) are the evidence.
- Alternative for function entry guards: `-fsanitize=cfi-icall`.

## 6. CET (x86: IBT + shadow stack)

- Flags (GCC/Clang, x86-64): `-fcf-protection=full` = IBT (indirect branch
  tracking) + SHSTK (shadow stack). `-fcf-protection=branch` = IBT only.
- Property (ELF): a `.note.gnu.property` with `x86 feature: IBT, SHSTK`;
  function entries carry `endbr64` (IBT landing pads).
- Requirements: assembler/linker with GNU-property support (binutils >= 2.33),
  a kernel that enables CET (Linux 5.18+ / Windows 10 20H1+) and CPU with CET.
  Older toolchains silently accept the flag and emit nothing, or error on the
  `note` directive. Without the CPU feature the binary still runs (CET is
  opt-in per-feature via the note).
- Verify:
  - `readelf -n ./app` → `.note.gnu.property` with `x86 feature: IBT, SHSTK`.
  - `objdump -d ./app | grep endbr64` — landing pads at function entries.
  - Windows native: `/CETCOMPAT` MSVC linker option marks the PE binary
    CET-compatible; verify with `dumpbin /headers` ("CET
    Compatible") or `readelf` equivalent on the PE's load-config directory.
- Host finding (this repo's Windows/MinGW host): `-fcf-protection=full`
  emitted `endbr64` at the exported entry (`main`) of a PE build, but NO
  `.note.gnu.property` section appeared in the PE output, and `readelf`
  cannot read PE files. So CET is only PARTIALLY verifiable here; full
  note-based verification is a Linux/ELF-target step.

## 7. ARM64 BTI and PAC

- Flags: `-mbranch-protection=standard` = BTI + PAC-RET (+ leaf PAC).
  Also `-mbranch-protection=bti`, `=pac-ret`, `=pac-ret+b-key` variants.
- Property: `.note.gnu.property` with `aarch64 feature: BTI, PAC` and BTI
  landing pads (`bti c`/`bti j`) at indirect-branch targets.
- Requirements: binutils >= 2.34 for the property note, and the OS must
  initialize PAC keys / enable BTI (Linux 6.1+ arm64).
- Verify: `readelf -n ./app` → `Properties: aarch64 feature: BTI, PAC`.

## 8. Windows native shadow stack

- Flag: MSVC linker `/CETCOMPAT`; binaries linked with it have the
  `IMAGE_DLLCHARACTERISTICS_CET_COMPAT` bit in the load-config
  characteristics. This is the PE equivalent of CET SHSTK — the compiler side
  is `/guard:cf`-adjacent and needs `/CETCOMPAT` at link for shadow stack.
- Verify: `dumpbin /headers app.exe` shows `CET Compatible` (or use
  `llvm-readobj --coff-load-config`).

## 9. Noexec stack (NX)

- Flag: `-Wl,-z,noexecstack`. GNU_STACK segment flags without Execute.
  On modern toolchains (gcc/binutils >= 2015-era defaults, and PE builds) NX is
  the default; `-z execstack` is what explicitly disables it.
- Verify: `readelf -l ./app` → `GNU_STACK ... RW` (no `E`). `RWE` means the
  stack is executable. A GNU_STACK with `E` plus `mprotect(PROT_EXEC)` is the
  classic shellcode injection path.
- Also verify no single segment is both writable and executable
  (`readelf -l`; a `LOAD` with `RWX` flags).

## Where the flags go (summary)

| Protection      | Compile step               | Link step                          |
|-----------------|----------------------------|------------------------------------|
| PIE             | `-fPIE`                    | `-pie`                             |
| Full RELRO      | —                          | `-Wl,-z,relro,-z,now`              |
| Canary          | `-fstack-protector-strong` | —                                  |
| FORTIFY         | `-O1+ -D_FORTIFY_SOURCE=2` | —                                  |
| CFI (Clang)     | `-fsanitize=cfi -flto -fvisibility=hidden` | `-flto` (link)     |
| CET             | `-fcf-protection=full`     | (note emitted by assembler/linker) |
| BTI/PAC (ARM64) | `-mbranch-protection=standard` | —                              |
| Noexec stack    | —                          | `-Wl,-z,noexecstack`               |

## Failure modes (flags silently ignored)

- Flag on the wrong command line: compile-only flag given to the linker (or
  vice versa) is accepted and does nothing — e.g. `-pie` only at compile, or
  `-fstack-protector-strong` only at link.
- Target/toolchain unsupported: `-fsanitize=cfi` (LLVM-only) on GCC, CET on
  binutils < 2.33, BTI/PAC on old aarch64 binutils — accepted, no property
  note emitted, no error raised.
- `_FORTIFY_SOURCE` without `-O1+`, or defined after the first system header.
- Optimization level: canaries are inserted at any `-O`; FORTIFY needs `-O1+`.
- Defaults differing by distro/toolchain: a distro may default
  `-fstack-protector-strong`, another not — never assume, verify the binary.
- "Compiled with the flag" is not evidence. The binary is the evidence.

## Host verification limits (this repo, Windows + MinGW gcc 16.1)

- `objdump -t/-d` reads PE fine → canary symbol, `__*_chk` calls, `endbr64`
  markers are verifiable here.
- `readelf -h/-l/-d/-n` requires ELF → PIE/RELRO/BIND_NOW/NX/GNU-property
  checks are Linux-target commands, recorded here with synthetic-but-realistic
  sample outputs for the checker.
- `checksec` is a Linux/POSIX shell script — not runnable on this host.
- The automation `examples/tools/hardening_audit.py` consumes the saved output
  of all of these tools and prints PASS/FAIL per protection.
