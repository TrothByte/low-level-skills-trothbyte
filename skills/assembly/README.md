# assembly — Skills

Assembly is the ground truth every higher language compiles to. These skills cover x86-64 registers and addressing, calling conventions, inline asm constraints, signed/unsigned branches, and optimizer artifacts in disassembly.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `asm-aarch64-neon-simd-safety` | Use when writing or reviewing AArch64 NEON/SIMD loops — vector intrinsics or assembly. Covers per-lane overflow, saturation vs wrap semantics, element types, and tail handling. Prevents the documented SIMD failure where counters overflow every ~255 iterations because no horizontal-reduce guard exists. | researched | `skills/assembly/asm-aarch64-neon-simd-safety` |
| `asm-arm-thumb-2-encoding` | Use when writing or reviewing Thumb-2 assembly for Cortex-M and other Arm A/R-profile cores. Covers CBZ/CBNZ r0-r7 constraint and range, 16/32-bit mixed encoding, IT blocks for conditional execution, and A32-vs-Thumb differences. Prevents invalid hi-register branches, far-range branch bugs, and hand-encoded byte corruption. | researched | `skills/assembly/asm-arm-thumb-2-encoding` |
| `asm-calling-conventions` | Use when writing or reviewing assembly, inline asm, or FFI code that calls or defines functions on x86-64 (SysV or Windows), AArch64, or RISC-V, or when reading disassembly and predicting argument registers, shadow space, stack alignment, callee-saved sets, and prologue shape. | source-backed | `skills/assembly/asm-calling-conventions` |
| `asm-inline-asm-constraints` | Use when writing, reviewing, or porting inline assembly in C, C++, or Rust — GCC extended asm constraints, memory and register clobbers, asm goto, and Rust asm! operand classes. Teaches declaring every asm side effect so the optimizer cannot miscompile around it. | source-backed | `skills/assembly/asm-inline-asm-constraints` |
| `asm-optimizer-artifacts` | Use when reading compiler-generated assembly (gcc/clang -O2 -S, objdump, Godbolt) and explaining why machine code diverges from C source — inlining, tail calls, dead-code elimination, constant folding, lea strength reduction, RIP-relative addressing. Teaches spotting optimizer artifacts without misreading them as missing code. | source-backed | `skills/assembly/asm-optimizer-artifacts` |
| `asm-risc-v-registers-and-calling-conventions` | Use when writing or reviewing RISC-V assembly — recursion, stack frames, and register roles. Covers callee-saved s0-s11 vs caller-saved a0-a7/t0-t6, RV64 8-byte slots and 16-byte alignment, leaf functions, and argument passing. Prevents the recorded garbage-sum failures from uninitialized s0 and 4-byte frames. | researched | `skills/assembly/asm-risc-v-registers-and-calling-conventions` |
| `asm-signed-unsigned-branches` | Use when writing or reading x86-64 assembly, inline asm, or disassembly where signed vs unsigned semantics decide the instruction — jl/jg/jge/jle vs jb/ja/jae/jbe, sar vs shr, cdq/idiv vs xor/div, movsx vs movzx. Teaches flag semantics and how compilers select branch mnemonics from C types. | source-backed | `skills/assembly/asm-signed-unsigned-branches` |
| `asm-syntax-dialects-nasm-gas-att` | Use when writing or reviewing assembly where the dialect matters — NASM Intel-style versus GNU as AT&T versus GNU as Intel mode. Covers operand order, immediates, size hints, label case, and default rel. Prevents silent address-vs-content bugs, reversed operands, and the four documented NASM error classes. | source-backed | `skills/assembly/asm-syntax-dialects-nasm-gas-att` |
| `asm-verification-hallucination-gate` | Use when an agent produces or reviews assembly and claims an instruction or encoding is valid. Gate every mnemonic, operand, width, and stack offset through assemble + disassemble + byte compare against an ISA manual, because generated mnemonics are often invented. Prevents fabricated instructions, AT&T reversals, and byte-blind claims. | source-backed | `skills/assembly/asm-verification-hallucination-gate` |
| `asm-x86-64-registers-and-addressing` | Use when reading, writing, or reviewing x86-64 assembly: choosing register widths, operand-size suffixes, addressing modes, RIP-relative operands, REX encoding, canonical 48-bit addresses, or zero/sign extension. Prevents wrong-size moves, stale-flag branches, and non-canonical-address bugs in hand-written asm and inline asm. | source-backed | `skills/assembly/asm-x86-64-registers-and-addressing` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
