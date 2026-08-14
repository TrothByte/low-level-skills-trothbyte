---
name: asm-calling-conventions
description: Use when writing or reviewing assembly, inline asm, or FFI code that calls or defines functions on x86-64 (SysV or Windows), AArch64, or RISC-V, or when reading disassembly and predicting argument registers, shadow space, stack alignment, callee-saved sets, and prologue shape.
---

# Assembly Calling Conventions (SysV, Windows x64, AAPCS64, RISC-V)

## When to use

- Writing hand-written `.s` files or inline asm that calls C functions, or is called
  from C (argument placement must match the target ABI).
- Reading disassembly and needing to identify which calling convention applies and
  whether a prologue is correct.
- FFI or cross-ABI boundaries (native library calls, `__attribute__((ms_abi))`,
  callbacks) where a mismatch silently corrupts arguments or crashes.
- Predicting shadow space, stack alignment, red zone use, and callee-saved sets before
  writing or reviewing a function's prologue/epilogue.

## When not to use

- Struct layout, struct-by-value classification, padding/alignment of data (use
  `abi-layout-reasoning`; that skill requires these per-arch tables).
- Pure C where the compiler generates all calls on one platform — the compiler already
  follows the ABI; you only need this skill at a boundary you control.
- Linker/loader/ELF details (use `elf-linker-loader-debugger`).
- ARM32 (AAPCS32) — this skill covers AAPCS64 only.

## What the agent often gets wrong

- "Args go in rdi, rsi, rdx, rcx, r8, r9." That is SysV. On Windows x64 they go in
  `rcx, rdx, r8, r9`; a SysV-style function on Windows returns garbage (verified:
  e.g. 88/96 instead of 42 — the value is compiler-dependent).
- "Windows is SysV plus a few extras." Windows has no red zone, has a 32-byte shadow
  space every caller must reserve, preserves `rdi`/`rsi` (SysV does not), and passes
  only 4 args in registers.
- "A call just needs the arguments." Missing the 32-byte shadow space lets the callee's
  spills overwrite the caller's own stack (verified: local became `0x1`).
- "Stack alignment is optional." `rsp % 16 == 0` at the call site is mandatory; a
  misaligned call faults on `movaps` (verified SIGSEGV).
- "`long` is 64-bit everywhere." On Windows (LLP64) `long` is 32-bit; SysV/AAPCS64/
  RISC-V (LP64) it is 64-bit.
- "There's a red zone." Only SysV AMD64 has the 128-byte red zone.
- Copying a correct Linux prologue into a Windows target, or vice versa, and changing
  nothing else.

## How to reason correctly

1. Identify the platform and object format first: PE/COFF → Windows x64; ELF on
   Linux/macOS/BSD → SysV AMD64; AArch64 ELF → AAPCS64; RISC-V ELF → psABI. Then check
   what the toolchain actually defaults to (`gcc -v`, `objdump -f`).
2. Classify every argument: integer/pointer vs floating/vector. Assign registers in
   declaration order from the target's integer list and FP list.
3. Args that do not fit in registers go on the stack — compute their offset from the
   callee's entry `rsp`: SysV `+8` per slot; Windows `+40` for the 5th arg (32 shadow
   + return address).
4. Check the call site: SysV and Windows both require `rsp % 16 == 0` before `call`;
   AAPCS64/RISC-V require `sp` 16-byte aligned. Count bytes allocated in the prologue
   (pushed registers are 8 bytes each).
5. Determine which registers your code clobbers and preserve exactly the target's
   callee-saved set; save/restore them in the prologue/epilogue.
6. When uncertain, generate the function with the real compiler and diff against your
   hand-written version — the compiler is the executable reference for the ABI your
   toolchain targets.

## What to verify

- Argument registers in the prologue match the target ABI (Windows `rcx,rdx,r8,r9` vs
  SysV `rdi,rsi,rdx,rcx` vs AArch64 `x0-x7` vs RISC-V `a0-a7`).
- Windows caller reserves 32 bytes of shadow space and 5th+ args start at `rsp+40`
  (at the call site).
- `rsp % 16 == 0` immediately before every `call`; `sp` 16-aligned on AAPCS64/RISC-V.
- Callee-saved registers used by the body are saved and restored (`rbx,rbp,r12-r15` +
  `rdi,rsi` on Windows; `x19-x28`+`fp` on AArch64; `s0-s11` on RISC-V).
- Return address handling: `x30`/`ra` is caller-set and must be saved if the callee
  calls anything (AArch64/RISC-V); x86-64 uses the stack.
- The struct-return register if any (SysV/Windows `rax`, AAPCS64 `x8`, RISC-V `a0`).
- The object format matches the intended ABI (PE vs ELF) and the code actually runs on
  that OS.

## How to verify

```
gcc -O0 -S -o - file.c          # compiler-generated prologue for the target
gcc -c file.s && objdump -d file.o   # inspect your hand-written asm
objdump -f file.o                # file format: pe-x86-64 vs elf64-*
gcc driver.c file.s -o t && ./t # runtime check of the cross-ABI demos
```

- Windows x64 (MinGW gcc on PATH): compile and run `examples/bad` and `examples/good`
  drivers; bad cases must produce the documented wrong results/crash.
- SysV, AArch64, RISC-V: no cross-compilers here — generate with `gcc -S` on a Linux
  host or Godbolt (`-target`/`-march`/`-mabi`) and compare with the documented-as-target
  examples. Marked DOCUMENTED-AS-TARGET, not VERIFIED.
- See `references/calling-conventions.md` for per-rule commands.

## Where the knowledge comes from

- `sysv-amd64-abi` — System V AMD64 psABI §3.2 (registers, stack frame, red zone), §3.3
  (varargs, AL).
- `aapcs64` — AAPCS64 §6.1 GP registers, §6.2 parameter passing, §6.3 result handling,
  §7 stack (16-byte alignment), x8 sret, x19-x28 callee-saved.
- `riscv-psabi` — RISC-V psABI `riscv-cc.adoc` (a0-a7, fa0-fa7, ra, s0-s11, gp/tp,
  16-byte stack alignment).
- Windows x64 (Microsoft x64 calling convention) — primary docs
  (learn.microsoft.com/cpp/build/x64-calling-convention); not yet in the source
  registry; facts are additionally verified empirically against MinGW GCC 16.1
  (see `examples/good/win64_*.s` and `evals/README.md`).

## Related skills

- `abi-layout-reasoning` — struct layout and by-value classification (requires these
  tables).
- `ffi-boundary-cross-language` — using conventions across languages (require).
- `elf-linker-loader-debugger` — binary-level consequences of ABI mismatch (verify).

## Evaluation

Synthetic: sysv_amd64.s and win64_add.s prologue classification; predict arg registers
for `f(long long,long long,long long,long long)` on each ABI.
False-positive: a correct Windows x64 prologue (32-byte shadow space, rsp 16-aligned)
must NOT be flagged.
Adversarial: (1) SysV-style `.s` function linked into a Windows x64 binary (verified
wrong result), (2) caller missing shadow space (verified local clobber), (3) misaligned
call site (verified SIGSEGV on movaps), (4) Windows ABI applied to SysV vector args
(xmm0-7 vs xmm0-3 + pointer-passed `__m128`).
