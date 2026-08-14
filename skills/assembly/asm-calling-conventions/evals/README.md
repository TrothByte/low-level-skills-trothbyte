# Evaluation — asm-calling-conventions

Skill: `skills/assembly/asm-calling-conventions`. Stability target: `evaluated`.
Verification host: Windows x64, GCC 16.1 (MinGW, MSYS2). ARM/RISC-V: no
cross-compilers — documented as target (Godbolt / `*-linux-gnu-gcc -S`).

## Synthetic evals

- **easy**: classify the prologue — `win64_caller_shadow.s` must be identified as
  Windows x64 (rcx/rdx/r8/r9, `subq $32` shadow space, `.seh_*`).
- **medium**: predict arg registers and stack-arg offsets for
  `f(long long a..h)` on each ABI (SysV: 6 regs + 16(%rbp); Windows: 4 regs +
  48(%rbp); AArch64: x0..x7 + sp; RISC-V: a0..a7 + sp).
- **hard**: classify `__m128` args — Windows passes by hidden pointer, SysV in
  xmm0..xmm7; `movaps` vs `movups` in the prologue.
- **easy**: which registers may a callee clobber freely on each ABI (SysV rdi/rsi vs
  Windows-preserved rdi/rsi; AArch64 x19..x28; RISC-V s0..s11).

## Adversarial evals (each demonstrated at runtime on MinGW GCC 16.1)

- **AD-A (SysV-under-Windows)**: `examples/bad/sysv_args_on_windows.s` — a SysV-style
  function linked into a Windows binary. VERIFIED: `bad_sysv_add(40,2)` returned
  garbage (observed 88 and 96 across builds) instead of 42. The agent must flag the
  rdi/rsi use for a Windows target.
- **AD-B (missing shadow space)**: `examples/bad/missing_shadow_space.s` — caller
  with `subq $16`. VERIFIED: callee spill clobbered the caller's local
  (0x1122334455667788 became 0x1). Must flag the missing 32-byte reservation.
- **AD-C (misaligned call site)**: `examples/bad/misaligned_call.s` — `subq $40`.
  VERIFIED: SIGSEGV / 0xC0000005 inside `callee_movaps`. Must flag rsp % 16 != 0.
- **AD-D (LR/ra not saved)**: AArch64 non-leaf using `ret` without `stp x29,x30`
  (DOCUMENTED-AS-TARGET) and RISC-V `ret` without saving `ra`.

## False-positive evals

- `examples/good/win64_caller_shadow.s` — a correct Windows x64 prologue (shadow
  space + alignment + callee-saved set) must NOT be flagged.
- `examples/good/win64_callee_8args.s` — 8-arg callee with correct shadow spills and
  stack offsets must NOT be flagged.
- `examples/good/win64_callee_saved.s` — rbx/r12..r15 push/pop prologue (correct for
  Windows x64) must NOT be flagged.
- `examples/good/sysv_amd64.s` — a correct SysV leaf must NOT be flagged when the
  target is documented as SysV/ELF.

## Verified facts (GCC 16.1, MinGW — Windows x64 ABI)

- `caller(void){ return callee(1,2,3,4); }` → `pushq %rbp; movq %rsp,%rbp; subq
  $32,%rsp; ...; movl $4,%r9d; movl $3,%r8d; movl $2,%edx; movl $1,%ecx; call`.
- 8-arg callee spills reg args to 16/24/32/40(%rbp) (shadow space) and reads stack
  args 5..8 from 48/56/64/72(%rbp).
- Callee-saved usage produces `pushq %r15; pushq %r14; pushq %r13; pushq %r12;
  pushq %rbx` and reverse pops.
- `__m128 f(__m128,__m128,__m128,__m128)` at -O2 loads via pointers: `addps (%rcx)`,
  `addps (%r8)` — vector args are passed by reference on Windows x64.
- `long` = 32-bit on Windows: GCC spilled `movl %ecx, 16(%rbp)` for a `long` param
  vs `movq %rcx, 16(%rbp)` for `long long`.
- Runtime demos: wrong-result (garbage 88/96 vs 42), shadow-space clobber (0x1),
  misaligned-call SIGSEGV.

## Documented-as-target facts (not verified here)

- SysV AMD64: 6 int regs (rdi,rsi,rdx,rcx,r8,r9), 8 SSE regs (xmm0..7), 16-byte
  alignment, red zone, callee-saved rbx/rbp/r12..r15, varargs AL = vector-arg count.
- AAPCS64: x0..x7, v0..v7, x8 sret, x19..x28 + x29 callee-saved, x30 LR, sp
  16-byte aligned.
- RISC-V RV64: a0..a7, fa0..fa7, ra = x1, s0..s11 callee-saved, sp 16-byte aligned.

## Verification commands

```
# Windows x64 (this host):
gcc bad.c sysv_args_on_windows.s missing_shadow_space.s -o bad && ./bad
gcc misalign_driver.c misaligned_call.s ../good/win64_caller_aligned.s -o misalign && ./misalign
gcc -O0 -S -o - ../good/verify.c        # regenerate prologues; diff with examples

# SysV / AArch64 / RISC-V (documented as target):
aarch64-linux-gnu-gcc -S f.c            # or Godbolt: x86-64 gcc / aarch64 gcc / riscv64 gcc
riscv64-linux-gnu-gcc -S f.c
gcc -O0 -S f.c                          # Linux host, default SysV AMD64
```

## Source coverage

Rules in `references/calling-conventions.md` cite `sysv-amd64-abi`, `aapcs64`, and
`riscv-psabi` registry ids. Windows x64 facts cite Microsoft x64 calling convention
docs (not yet in the registry) and are double-checked empirically against MinGW GCC
16.1.
