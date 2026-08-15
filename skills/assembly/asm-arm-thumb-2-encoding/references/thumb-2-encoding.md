# Thumb-2 Encoding — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to
registry/sources.yaml.

## 1. Thumb-2 is a 16/32-bit mixed encoding, not ARM (A32)

- **RULE**: Thumb-2 instructions are either 16-bit (T1 encodings) or 32-bit
  (T2/T3 encodings), distinguished by the top 5 bits of the first
  halfword. A32 (ARM) is a separate, fixed 32-bit encoding with different
  opcode layout and conditional prefixes. State is per-execution-state, set by
  `.thumb`/`.arm` and switched at runtime by BX/BLX.
- **WHY AI GETS IT WRONG**: models trained mostly on A32 or on x86 assume one
  fixed instruction width and A32 bit layouts; they emit "ARM assembly" into a
  Thumb object or hand-patch bytes as if widths were uniform.
- **CORRECT REASONING**: with `.syntax unified` + `.thumb`, the assembler
  picks 16- vs 32-bit forms; `b.w` forces the 32-bit wide branch. Patching
  bytes into a Thumb stream requires knowing each instruction's width.
- **EXAMPLE** (bad): `mrs r0, cpsr` with a hand-assumed 32-bit A32 encoding.
- **COUNTEREXAMPLE** (good): `.thumb` `.syntax unified` then `mrs r0, psr` —
  the assembler emits the correct Thumb-2 T1/T2 form.
- **VERIFICATION**: `clang --target=armv7m-none-eabi -mthumb -c` +
  `llvm-objdump -d` shows `b5 30 00 04`-style 16/32-bit mixes
  (researched — toolchain not installed here).
- **SOURCE**: arm-arm (A-profile, Thumb instruction set encoding).

## 2. CBZ/CBNZ only take r0-r7

- **RULE**: Thumb-2 `cbz`/`cbnz` (encoding T1, opcode 0xB100/0xB900) encode Rt
  in bits 7:3, which gives only r0-r7. High registers (r8-r15) cannot be
  encoded and are rejected by the assembler.
- **WHY AI GETS IT WRONG**: a real merged PR used `cbz r9/r10` (HerraduraKEx
  PR#33); the register-range constraint of this specific encoding is not
  memorized.
- **CORRECT REASONING**: for a high register, use `cmp r9, #0` + `beq`/`bne`
  (two instructions) — the CBZ form is structurally incapable of high
  registers.
- **EXAMPLE** (bad): `cbz r9, .L1` → assembler error.
- **COUNTEREXAMPLE** (good): `cbz r3, .L1`; or `cmp r9, #0` + `beq .L1`.
- **VERIFICATION**: `clang --target=armv7m-none-eabi -mthumb -c`
  (researched — expected error on the bad form).
- **SOURCE**: arm-arm (CBZ/CBNZ encoding T1, Rt range).

## 3. CBZ/CBNZ branch range is ~+/-126 bytes

- **RULE**: the CBZ/CBNZ offset is a signed 6-bit immediate scaled by 2
  (imm6<<1), so targets must lie within about -126..+128 bytes of the
  instruction. Far targets need an inverted conditional branch plus a wide
  `b.w` (or a veneer).
- **WHY AI GETS IT WRONG**: the branch-range constraint of narrow Thumb
  encodings is rarely memorized; models emit `cbz r0, far_label` across
  hundreds of bytes and assume it resolves.
- **CORRECT REASONING**: for far targets, `cbnz r0, .near; b .far; .near: ...`
  or `cbz`+`b.w` pattern; reserve near branches for loop-local targets.
- **EXAMPLE** (bad): `cbz r0, .far_target` where `.far_target` is >126 bytes
  away.
- **COUNTEREXAMPLE** (good): invert to `cbnz r0, .Lskip; b.w .far_target;
  .Lskip:`.
- **VERIFICATION**: `clang --target=armv7m-none-eabi -mthumb -c` +
  `llvm-objdump -d` to check the displacement
  (researched — toolchain absent).
- **SOURCE**: arm-arm (CBZ/CBNZ imm6 offset).

## 4. Conditional execution requires an IT block

- **RULE**: in Thumb-2, conditional instructions (other than branches) must be
  placed inside an IT (if-then) block: `it eq` / `iteq` before one conditionally
  executed instruction, `itt`/`itte` for longer chains. A bare `moveq` outside
  an IT block is either rejected or mis-encoded as unconditional.
- **WHY AI GETS IT WRONG**: A32 permits `moveq` directly; models carry that
  habit into Thumb-2, or forget the IT block length letter count.
- **CORRECT REASONING**: count the conditional instructions: `it` + condition
  for one, `itt`/`itne` for two, `ite`/`iteq` for if-then-else pairs; the
  suffix letters mirror the chain.
- **EXAMPLE** (bad): `cmp r0,r1` then bare `moveq r0,#0`.
- **COUNTEREXAMPLE** (good): `cmp r0,r1` / `iteq` / `moveq r0,#0`.
- **VERIFICATION**: `clang --target=armv7m-none-eabi -mthumb -c` + objdump
  (researched — toolchain absent).
- **SOURCE**: arm-arm (IT instruction, Thumb conditional execution).

## 5. 16/32-bit choice is the assembler's, not yours

- **RULE**: with unified syntax the assembler selects narrow (16-bit) or wide
  (32-bit) encodings automatically; `w`/`n` suffixes (`b.w`, `movw`) force a
  width. Hand-written byte patches must match the assembler's choice or the
  stream desynchronizes.
- **WHY AI GETS IT WRONG**: byte-blindness again — models hand-encode Thumb
  bytes without width knowledge, or count instruction sizes wrong when
  computing branch displacements.
- **CORRECT REASONING**: let the assembler choose; force width explicitly only
  where required (`b.w` for far targets); always disassemble to confirm bytes.
- **EXAMPLE** (bad): patching 4 bytes for what the assembler encoded as 2.
- **COUNTEREXAMPLE** (good): disassemble after assemble; compare halfword counts.
- **VERIFICATION**: `llvm-objdump -d` after `clang --target=armv7m -mthumb -c`
  (researched — toolchain absent).
- **SOURCE**: arm-arm (Thumb-2 encoding overview).

## 6. Cortex-M is Thumb-only

- **RULE**: Cortex-M cores execute Thumb-2 (and Thumb) only; A32 is not
  available on M-profile. Any A32 instruction in an M-profile build is an
  error, and ISR/linkerscript expectations assume Thumb state (bit 0 of
  function addresses set).
- **WHY AI GETS IT WRONG**: models mix A32 and Thumb encodings and forget that
  M-profile cannot switch to A32.
- **CORRECT REASONING**: for `-mcpu=cortex-m*`, all code is Thumb-2; BLX
  targets must have bit 0 set (Thumb marker).
- **EXAMPLE** (bad): an A32 `mrs r0, cpsr` form inside a Cortex-M build.
- **COUNTEREXAMPLE** (good): `-mthumb` for the whole build, `.thumb` in asm.
- **VERIFICATION**: `clang --target=armv7m-none-eabi -mthumb -c`
  (researched — toolchain absent).
- **SOURCE**: arm-arm (M-profile execution state); arm-abi-aa (AAPCS,
  function pointer bit 0).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Encoding | Thumb-2 mixes 16-bit/32-bit; A32 is separate; M-profile is Thumb-only |
| CBZ/CBNZ registers | only r0-r7 (Rt in bits 7:3); use cmp+beq for high regs |
| CBZ/CBNZ range | signed 6-bit << 1 (~+/-126 bytes); far targets need b.w/veneer |
| Conditional exec | requires IT block: it/itt/ite + condition letters |
| Width selection | assembler's job; w/n suffixes force it; disassemble to confirm |
| Function pointers | Thumb marker bit 0 set on M-profile targets |
