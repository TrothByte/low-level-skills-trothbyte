# RISC-V Registers and Calling Conventions — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to
registry/sources.yaml.

## 1. Callee-saved vs caller-saved vs leaf functions

- **RULE**: RV64 psABI: caller-saved (argument/return/temp) = `a0-a7`, `t0-t6`
  (and `ra` only where the caller saves it); callee-saved = `s0-s11` (s0 may
  double as frame pointer). A leaf function (no calls) never needs to save
  `ra` or anything else. A non-leaf function must preserve callee-saved
  registers it uses and its return address across its own calls.
- **WHY AI GETS IT WRONG**: models treat s-registers as scratch (a recorded
  RISC-V failure used `s0` uninitialized), or over-save in leaves, or forget
  that `ra` is clobbered by any `call`.
- **CORRECT REASONING**: classify every register you touch by the psABI table;
  a function that calls out must save `ra` and any s-register it uses; a leaf
  uses only a/t registers and skips the prologue.
- **EXAMPLE** (bad): `helper` does `mv s0, a0` without saving the caller's s0.
- **COUNTEREXAMPLE** (good): caller saves s0 on its own frame; helper uses
  only `t0`.
- **VERIFICATION**: `clang --target=riscv64-unknown-elf -march=rv64gc -S` of a
  C reference; compare prologue/epilogue (researched — toolchain absent).
- **SOURCE**: riscv-psabi (register roles, table 2.1); riscv-isa-spec
  (RV64I base instructions).

## 2. Recursion: frame must hold s0 and ra in 8-byte slots

- **RULE**: on RV64 every stack slot is 8 bytes and sp stays 16-byte aligned.
  A frame for s0+ra needs 16 bytes (`addi sp,sp,-16`; `sd s0,0(sp)`; `sd
  ra,8(sp)`). A 4-byte frame (`-4`) misaligns sp, overlaps slots, and corrupts
  the recursion.
- **WHY AI GETS IT WRONG**: the recorded failure used a 4-byte frame and an
  uninitialized s0 — the sum was garbage; models transfer 32-bit (RV32) slot
  sizes to RV64 or use `sw` where `sd` is required.
- **CORRECT REASONING**: RV64 loads/stores are `ld`/`sd` (8 bytes); RV32 uses
  `lw`/`sw` (4 bytes). Frame size = number of saved slots × 8, rounded to
  maintain 16-byte sp alignment.
- **EXAMPLE** (bad): `addi sp,sp,-4` + `sw s0,0(sp)` + `sw ra,4(sp)`.
- **COUNTEREXAMPLE** (good): `addi sp,sp,-16` + `sd s0,0(sp)` + `sd ra,8(sp)`.
- **VERIFICATION**: `riscv64-linux-gnu-gcc`/`clang -target=riscv64` compile the
  recursion; inspect `objdump -d` prologue (researched).
- **SOURCE**: riscv-psabi (stack layout, 16-byte alignment); riscv-isa-spec
  (RV64 ld/sd).

## 3. s0 must be initialized and restored before recursion returns

- **RULE**: if a function uses s0, it must (a) save the caller's value, (b)
  use it, (c) restore it before `ret`. In recursion, each level has its own
  frame; restoring from the wrong frame (or never initializing) leaks garbage
  to the caller.
- **WHY AI GETS IT WRONG**: the recorded RISC-V failure saved s0 but never set
  it, then read garbage after the recursive call — the save was theater.
- **CORRECT REASONING**: trace each register's save/use/restore across the
  recursive call boundary; the restore must read the frame that the same
  prologue wrote.
- **EXAMPLE** (bad): `sw s0,0(sp)` with no init, then `add a0,a0,s0`.
- **COUNTEREXAMPLE** (good): save → (use) → `ld s0,0(sp)` → `ret`.
- **VERIFICATION**: run the recursion on a RISC-V host/emulator and compare
  with the scalar sum (researched).
- **SOURCE**: riscv-psabi (callee-saved rule); riscv-isa-spec (call/ret).

## 4. Argument and return registers

- **RULE**: first 8 integer args in `a0-a7`, return in `a0` (+`a1` for wide
  returns); varargs place extra args on the stack (per psABI). RV64 passes
  `long`/pointers in 64-bit registers regardless of C width.
- **WHY AI GETS IT WRONG**: models assume x86 SysV habits or 32-bit arg
  widths; a function reading args from the stack that never received them
  returns garbage.
- **CORRECT REASONING**: match arg count to a-registers first; only spill to
  the stack at >8 args. Return address is in `ra`, not on the stack.
- **EXAMPLE** (bad): reading arg 1 from `0(sp)` — `ra` is there, not the arg.
- **COUNTEREXAMPLE** (good): `add a0, a0, a1` using the a-registers.
- **VERIFICATION**: `clang --target=riscv64-unknown-elf -S` of a C function
  with 10 args shows a0-a7 + stack (researched).
- **SOURCE**: riscv-psabi (argument passing); sysv-amd64-abi (contrast).

## 5. Stack discipline: alignment and prologue/epilogue symmetry

- **RULE**: sp must stay 16-byte aligned at all times (ABI hard requirement on
  RV64); every `addi sp,sp,-N` must be matched by `addi sp,sp,N` on exit;
  `ret` uses `ra`, so ra must be restored before `ret`.
- **WHY AI GETS IT WRONG**: misaligned frames (4-byte) and asymmetric
  prologues silently corrupt memory or crash in ABI-sensitive code (libcalls,
  floating-point).
- **CORRECT REASONING**: compute frame size = saved slots × 8 rounded up to 16;
  write prologue and epilogue together.
- **EXAMPLE** (bad): `addi sp,sp,-4` ... `addi sp,sp,4` around sd pairs.
- **COUNTEREXAMPLE** (good): `-16` / `+16` with `sd`/`ld` in mirror order.
- **VERIFICATION**: `objdump -d` checks sp deltas and pairing (researched).
- **SOURCE**: riscv-psabi (stack alignment, frame layout); riscv-isa-spec.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Caller-saved | a0-a7, t0-t6 (and ra unless saved by caller) |
| Callee-saved | s0-s11; preserve them across your calls |
| Leaf function | no calls → no saves, no frame needed |
| RV64 slots | 8 bytes; sp stays 16-byte aligned |
| Recursion | save s0 + ra (16-byte frame), restore before ret |
| Args/return | a0-a7 in, a0 (+a1) out; >8 args spill to stack |
| Prologue | every -N matched by +N; ld/sd mirrored |
