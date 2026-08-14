# qemu — Skills

QEMU is the universal verification host for code you cannot yet run on real hardware. This skill covers machine models, kernels, firmware, and gdb stubs for every emulation workflow.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `qemu-system-setup` | Use when setting up QEMU system emulation to boot a Linux kernel, firmware, or bare-metal ELF for x86-64, ARM Cortex-M, or AArch64 — machine model selection, -kernel/-nographic/-drive/netdev, serial console, and gdb remote debugging. | researched | `skills/qemu/qemu-system-setup` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
