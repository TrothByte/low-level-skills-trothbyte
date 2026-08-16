# bootloader — Skills

Bootloaders own the first instructions of a machine.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `bootloader-stages` | Use when writing or debugging bootloaders and firmware — BIOS/UEFI stage-1 loading, MBR/GPT, x86 real to protected to long mode (A20, GDT, CR0/CR4/EFER, paging), AArch64 and RISC-V boot protocols, and link-address vs load-address relocation at stage-2 entry. | unique | source-backed | `skills/bootloader/bootloader-stages` |
| `bootloader-uefi-acpi-dtb` | Use when a UEFI bootloader must hand off system-description data: locating ACPI tables (RSDP/XSDT/MADT), SMBIOS structures, or a Flattened Device Tree (DTB), and deciding which interface the platform exposes. Teaches table parsing, checksum validation, and architecture-dependent firmware-handoff conventions. | improved | source-backed | `skills/bootloader/bootloader-uefi-acpi-dtb` |
| `bootloader-uefi-firmware` | Use when writing, reading, or debugging UEFI firmware and edk2 code: Boot vs Runtime Services, the PI spec boundary, ExitBootServices ordering, HII/VFR forms, ACPI/SMBIOS tables, and Secure Boot. Prevents boot-services-after-exit crashes, missing RUNTIME_ACCESS flags, and VFR bounds that the driver ignores. | improved | researched | `skills/bootloader/bootloader-uefi-firmware` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
