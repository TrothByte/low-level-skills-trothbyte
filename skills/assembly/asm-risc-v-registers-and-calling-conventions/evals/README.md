# Evaluation — asm-risc-v-registers-and-calling-conventions

Skill: `skills/assembly/asm-risc-v-registers-and-calling-conventions`.
Stability target: `researched`. Toolchain status: `clang`, `qemu-riscv64`,
`riscv64-linux-gnu-gcc` are NOT installed on this host (MSYS2 ucrt64: gcc
16.1.0/as/objdump target x86_64-w64-mingw32 only). **No RISC-V command was
run.** All cases are documented with the exact verification command;
"expected" labels are predictions from `riscv-psabi`/`riscv-isa-spec`, marked
UNVERIFIED until run on a RISC-V toolchain host.

## Synthetic evals

| Case | Fixture | Expected (UNVERIFIED) | Verification command |
|---|---|---|---|
| easy/negative | `bad/recursion_s0.s` | garbage sum (s0 uninitialized) | `clang --target=riscv64-unknown-elf -march=rv64gc -c` |
| easy/negative | `bad/frame_size.s` | 4-byte frame misaligns sp | `clang --target=riscv64-unknown-elf -march=rv64gc -c` |
| medium/negative | `bad/callee_saved.s` | s0 clobbered across call | `clang --target=riscv64-unknown-elf -march=rv64gc -c` |
| easy/positive | `good/recursion_s0.s` | s0+ra saved in 16-byte frame | `clang --target=riscv64-unknown-elf -march=rv64gc -c` |
| easy/positive | `good/leaf_fn.s` | leaf, no prologue, correct | `clang --target=riscv64-unknown-elf -march=rv64gc -c` |
| easy/positive | `good/callee_saved.s` | s0 saved/restored around call | `clang --target=riscv64-unknown-elf -march=rv64gc -c` |

## False-positive evals (correct code must not be flagged)

- `good/leaf_fn.s` — a leaf that uses only `t0` and `a0` with no prologue is
  CORRECT; must not be flagged for "not saving ra".
- `good/callee_saved.s` — the caller saves/restores s0 across `call helper`,
  and helper touches only caller-saved registers: correct.
- `good/recursion_s0.s` — 16-byte aligned frame with `sd s0,0(sp)` / `sd
  ra,8(sp)`: correct RV64 prologue.
- Deliberate use of `a0` as both arg and return register is normal, not a bug.

## Historical evals (real incident)

- **RISC-V recursion failure (r/RISCV, ASM-7)** — `s0` uninitialized and a
  4-byte frame instead of 8 for (s0+ra) produced a garbage sum. Reproduced as
  `bad/recursion_s0.s` and `bad/frame_size.s`.

## Adversarial evals

- `bad/recursion_s0.s` and `bad/frame_size.s` — assemble cleanly and run,
  returning wrong sums. The "compiles fine, wrong result" class: a reviewer
  must trace register save/restore and frame size rather than trusting that a
  green build means correct ABI usage.

## Verified facts

None — researched skill. No commands executed. This is stated honestly: the
host lacks a RISC-V toolchain and emulator, so register-role and frame-layout
consequences are KNOWN from `riscv-psabi`/`riscv-isa-spec` but their
run-level outcomes are UNVERIFIED here. The verification commands above are
complete and ready to run on any RISC-V-capable host.

## Scoring (for routing eval)

- precision: each flagged case maps to a named reference rule (1-5).
- recall: all bad snippets detected by review (s0 save/restore, frame size,
  callee-saved misuse, leaf classification, arg registers).
- FP-rate: good snippets produce zero flags.
- confidence: every claim marked UNVERIFIED until the RISC-V run records
  actual exit codes and sums.

## Target toolchains (absent, documented)

- `clang --target=riscv64-unknown-elf -march=rv64gc -c` /
  `riscv64-linux-gnu-gcc -c`: verification commands for all cases.
- `qemu-riscv64`: runtime sum check on a host that has it.
