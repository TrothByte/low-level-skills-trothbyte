# assembly — Skills

Assembly is the ground truth every higher language compiles to. These skills cover x86-64 registers and addressing, calling conventions, inline asm constraints, signed/unsigned branches, and optimizer artifacts in disassembly.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `asm-calling-conventions` | Use when writing or reviewing assembly, inline asm, or FFI code that calls or defines functions on x86-64 (SysV or Windows), AArch64, or RISC-V, or when reading disassembly and predicting argument registers, shadow space, stack alignment, callee-saved sets, and prologue shape. | source-backed | `skills/assembly/asm-calling-conventions` |
| `asm-inline-asm-constraints` | Use when writing, reviewing, or porting inline assembly in C, C++, or Rust — GCC extended asm constraints, memory and register clobbers, asm goto, and Rust asm! operand classes. Teaches declaring every asm side effect so the optimizer cannot miscompile around it. | source-backed | `skills/assembly/asm-inline-asm-constraints` |
| `asm-optimizer-artifacts` | Use when reading compiler-generated assembly (gcc/clang -O2 -S, objdump, Godbolt) and explaining why machine code diverges from C source — inlining, tail calls, dead-code elimination, constant folding, lea strength reduction, RIP-relative addressing. Teaches spotting optimizer artifacts without misreading them as missing code. | source-backed | `skills/assembly/asm-optimizer-artifacts` |
| `asm-signed-unsigned-branches` | Use when writing or reading x86-64 assembly, inline asm, or disassembly where signed vs unsigned semantics decide the instruction — jl/jg/jge/jle vs jb/ja/jae/jbe, sar vs shr, cdq/idiv vs xor/div, movsx vs movzx. Teaches flag semantics and how compilers select branch mnemonics from C types. | source-backed | `skills/assembly/asm-signed-unsigned-branches` |
| `asm-x86-64-registers-and-addressing` | Use when reading, writing, or reviewing x86-64 assembly: choosing register widths, operand-size suffixes, addressing modes, RIP-relative operands, REX encoding, canonical 48-bit addresses, or zero/sign extension. Prevents wrong-size moves, stale-flag branches, and non-canonical-address bugs in hand-written asm and inline asm. | source-backed | `skills/assembly/asm-x86-64-registers-and-addressing` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
