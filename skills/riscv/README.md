# riscv — Skills

RISC-V is the ascending open-source ISA for edge through HPC.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `riscv-cheri-capability-safety` | Use when writing or reviewing capability-aware C/C++ for CHERI (Morello, CHERI-RISC-V): capability pointers, bounds and tags, purecap vs hybrid ABI, sealing/unsealing, CheriBSD, and CHERI-enabled sanitizers. Prevents out-of-bounds dereferences, tag loss, and false positives that flag correct bounded code. | unique | researched | `skills/riscv/riscv-cheri-capability-safety` |
| `riscv-isa-and-rvv-intrinsics` | Use when writing or reviewing RISC-V code with the vector extension: vsetvl/vsetvli AVL-VL semantics, LMUL/SEW ratios, VLEN dependence, tail and mask policies, strip-mining loops, strided and segmented loads, and RVV C intrinsics. Prevents wrong VL, policy misuse, and portability bugs across VLEN implementations. | unique | researched | `skills/riscv/riscv-isa-and-rvv-intrinsics` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
