# bootloader — Skills

Bootloaders own the first instructions of a machine. This skill covers the boot stages — real mode to long mode, AArch64/RISC-V handoff, and relocation — where link addresses and load addresses diverge.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `bootloader-stages` | Use when writing or debugging bootloaders and firmware — BIOS/UEFI stage-1 loading, MBR/GPT, x86 real to protected to long mode (A20, GDT, CR0/CR4/EFER, paging), AArch64 and RISC-V boot protocols, and link-address vs load-address relocation at stage-2 entry. | source-backed | `skills/bootloader/bootloader-stages` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
