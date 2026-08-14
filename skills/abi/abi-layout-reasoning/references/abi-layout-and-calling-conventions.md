# ABI Layout & Calling Convention — Reference

Sources: System V AMD64 ABI §3.2/§3.3/§4; AAPCS64 §6-8; RISC-V psABI. Layout claims
verified with GCC 16.1 (MSYS2/MinGW, x86-64). NOTE: MinGW uses the Windows x64 calling
convention, which differs from Linux SysV AMD64 for argument passing (see §2a) — layout
rules (§1) are identical.

## 1. Struct layout: alignment & padding (all ABIs)

- **RULE**: a member's offset is aligned to `_Alignof(member)`; struct alignment is the max
  member alignment; total size is padded to a multiple of struct alignment.
- **WHY AI GETS IT WRONG**: "size = sum of field sizes".
- **CORRECT REASONING**: always `offsetof`, never hand-sum; reordering fields can shrink the
  struct (largest-alignment first minimizes padding).
- **EXAMPLE**: `struct { char c; int i; }` → off_c=0, off_i=4, size=8, align=4 (verified below).
- **COUNTEREXAMPLE** (bad layout): `struct { char a; double d; char b; }` → size=24;
  reordered `{ double d; char a; char b; }` → size=16.
- **VERIFICATION**: compile the tiny program in SKILL.md; `-fdump-record-layouts`.
- **SOURCE**: SysV AMD64 §4; N1570 §6.2.8.

## 2. SysV AMD64 integer argument passing

- **RULE**: first six integer args in `%rdi,%rsi,%rdx,%rcx,%r8,%r9`; integer return in `%rax`;
  callee-saved `%rbx,%rbp,%r12-%r15`; stack args from 7th on. Pointers count as integers.
- **WHY AI GETS IT WRONG**: applies Windows x64 rules (rcx,rdx,r8,r9) to Linux/SysV.
- **CORRECT REASONING**: same code, different ABIs → 4 vs 6 register args. Never guess; verify
  with `-S` or a calling-convention reference.
- **EXAMPLE**: `long f(long a,long b,long c,long d,long e,long f,long g)` → a-f in regs, g on stack.
- **VERIFICATION**: `gcc -O0 -S` and read the prologue.
- **SOURCE**: SysV AMD64 §3.2.1, §3.2.3.

## 2a. Windows x64 vs SysV — argument passing differs (verified)

- **RULE**: Windows x64 uses 4 integer registers `%rcx,%rdx,%r8,%r9`; SysV AMD64 (Linux) uses
  6: `%rdi,%rsi,%rdx,%rcx,%r8,%r9`. A struct of two `int`s (8 bytes, all integer) is passed
  in TWO registers (`%edi,%esi`) on SysV, but packed into ONE register (`%rcx`) on Windows x64.
- **WHY AI GETS IT WRONG**: assumes one ABI everywhere; or writes assembly for Linux and runs
  it on MinGW/MSVC (or vice versa).
- **CORRECT REASONING**: ABI is per-OS+compiler, not per-architecture. Verified with GCC 16.1
  MinGW: `add_small(struct Small s)` → `movq %rcx, ...` (single-register packing). The same
  source on Linux SysV → `%edi` + `%esi`.
- **EXAMPLE**: `struct Small { int a; int b; }` passed by value.
- **VERIFICATION**: compile on each target and diff prologues: `gcc -S` (MinGW) vs
  `gcc -S` (Linux/WSL) vs `clang-cl -S`.
- **SOURCE**: SysV AMD64 §3.2.3 vs Microsoft x64 calling convention; empirical.

## 3. SysV AMD64 struct-by-value classification

- **RULE**: a struct ≤16 bytes whose fields are all integer/pointer types is passed in up to
  two integer registers; anything larger or with float/double members goes on the stack (with
  MEMORY class) — full details in the "class" algorithm §3.2.3.
- **WHY AI GETS IT WRONG**: "structs always go on the stack" OR "two ints always in two regs"
  — both wrong for the general case.
- **CORRECT REASONING**: run the classification: size ≤ 16 && each 8-byte word is INTEGER →
  registers; else MEMORY. A `{double,double}` (SSE class) also fits 16 bytes but uses `%xmm0,%xmm1`.
- **EXAMPLE**: `struct {int a; int b;}` passed by value → `%edi,%esi`; verified below.
- **VERIFICATION**: compile a caller and read `-S`.
- **SOURCE**: SysV AMD64 §3.2.3.

## 4. SysV AMD64 stack alignment (16-byte)

- **RULE**: at the point of `call`, `rsp % 16 == 0`; the called function sees
  `rsp % 16 == 8` on entry (return address). Local spills maintain this.
- **WHY AI GETS IT WRONG**: "alignment is cosmetic"; then a leaf that uses `movaps` on a
  16-byte-aligned local faults (misaligned vector load/store).
- **CORRECT REASONING**: with `-mpreferred-stack-boundary=4` the compiler guarantees it; hand
  asm must too. The classic exploit trap: add one `ret` gadget to fix alignment.
- **VERIFICATION**: `gcc -S` — check `sub $n, %rsp` values are multiples of 16 in functions
  with locals; or fault with a deliberately misaligned `movaps`.
- **SOURCE**: SysV AMD64 §3.2.5; zhaoxuya520 pwn-chain (movaps trap).

## 5. AAPCS64 (AArch64) argument passing

- **RULE**: first eight args in `x0-x7` (integer/pointer) and `v0-v7` (float/vector); return
  in `x0`/`v0`; indirect (large struct) result via `x8`; stack args from 9th; callee-saved
  `x19-x28`; `sp` must be 16-byte aligned at all times.
- **WHY AI GETS IT WRONG**: maps SysV "six regs" onto ARM; or forgets `x8` for big struct returns.
- **CORRECT REASONING**: AArch64 has 8 argument registers (not 6), and `x8` is the implicit
  sret pointer — a frequent FFI mistake.
- **EXAMPLE**: a `struct` >16 bytes returned by value → caller passes hidden `x8` pointer.
- **VERIFICATION**: `aarch64-linux-gnu-gcc -S` or Godbolt ARM.
- **SOURCE**: AAPCS64 §6.1-6.3, §7.

## 6. RISC-V psABI

- **RULE**: RV64I: args in `a0-a7`, return in `a0`; callee-saved `s0-s11`; `sp` 16-byte
  aligned (RV64); `ra` link register; varargs follow the same register order.
- **WHY AI GETS IT WRONG**: assumes a dedicated link register model is "like ARM but 16 regs".
- **CORRECT REASONING**: argument registers `a0-a7` map to `x10-x17`; no argument stack area
  exists before 8 regs are exhausted (unlike SysV's 6).
- **VERIFICATION**: `riscv64-linux-gnu-gcc -S`.
- **SOURCE**: RISC-V psABI riscv-cc.adoc.

## Verified layout examples (GCC 16.1, x86-64)

```
struct S1 { char c; int i; };            // off 0,4 | size 8, align 4
struct S2 { double d; char a; char b; }; // off 0,8,9 | size 16, align 8
struct S3 { char a; double d; char b; }; // off 0,8,16 | size 24, align 8
```
Run `tools/abi/abi_struct_layout.c` (see registry/tools.yaml) or the snippet in SKILL.md.
