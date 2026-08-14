---
name: bootloader-stages
description: Use when writing or debugging bootloaders and firmware — BIOS/UEFI stage-1 loading, MBR/GPT, x86 real to protected to long mode (A20, GDT, CR0/CR4/EFER, paging), AArch64 and RISC-V boot protocols, and link-address vs load-address relocation at stage-2 entry.
---

# Bootloader Stages: Firmware, Stage-1, Stage-2, Kernel

## When to use

- Writing stage-1 (MBR/VBR) or stage-2 bootloader code, or the firmware entry glue that hands off to them.
- Debugging "boots to a blank screen / triple fault / #GP at reset" by reasoning about which stage the CPU is actually in.
- Implementing the x86 real → protected → long mode transition (A20, GDT, CR0/CR4/EFER, paging) or AArch64/RISC-V early boot.
- Reviewing relocation/PIC logic where the load address differs from the link address.
- Verifying handoff contracts (BIOS register state, DTB in x0, hartid in a0) with QEMU, U-Boot, or a bare-metal emulator.

## When not to use

- Kernel internals after early boot — use `kernel-uaccess-safety`, `kernel-rcu-memory-barriers`.
- Driver/device bring-up once an OS is running — use the `embedded-*` skills.
- Pure userspace assembly with no boot semantics — use the `assembly/asm-*` skills.
- General AArch64/RISC-V calling-convention rules in C — use `asm-calling-conventions`.

## What the agent often gets wrong

- "The CPU is in long mode once CR0.PG is set." It is still compatibility mode until CS is reloaded with an L=1 descriptor via a far jump.
- "Just set EFER.LME and enable paging." Order matters: CR4.PAE → EFER.LME → CR3 → CR0.PG → far jump. PG=1 with LME=1 and PAE=0 raises #GP(0).
- "Any GDT will do." lgdt takes the address of a 6-byte pseudo-descriptor; a wrong base silently corrupts every segment-relative address after CR0.PE.
- "A20 is always on." Until the A20 gate is enabled, memory above 1 MiB wraps to below 1 MiB; verify with a probe, do not assume.
- "The loader places the image at its link address." The BIOS loads stage-1 at 0x7C00; the stage-2 load address is chosen by the loader, and non-PIC code must be relocated or written RIP-relative.
- "MBR is the partition table." MBR is 512 bytes containing both boot code and four 16-byte partition entries; GPT keeps a protective MBR in LBA0, header in LBA1, entries in LBA2+.
- "AArch64 boot is just 'jump to _start'." The Linux boot protocol fixes x0 = DTB physical address, EL requirements, MMU off, and 2 MiB image alignment.
- "RISC-V jumps into S-mode with no setup." Firmware must pass a0 = hartid, a1 = DTB and establish S-mode (PMP, trap delegation) first.

## How to reason correctly

1. Determine the current stage: firmware (BIOS/UEFI) → stage-1 → stage-2 → kernel. Every stage changes the entry contract, privilege, addressing model, and who provides the next entry point.
2. On x86, treat the mode transition as an ordered state machine: A20 → GDT → CR0.PE → far jump → (CR4.PAE → EFER.LME → CR3 → CR0.PG → far jump to an L=1 code segment).
3. For every memory reference ask: link address vs load address. If the code depends on where the linker placed it, make it position-independent (RIP-relative in long mode) or run a relocation pass with delta = load − link.
4. Read the entry contract before writing any stage: BIOS (DL = boot drive, DS:SI = partition entry, CS:IP = 0x7C00, 512-byte sector), AArch64 (DTB in x0, EL per protocol, MMU off), RISC-V (a0 = hartid, a1 = DTB, M-mode → S-mode).
5. Debug backward from the failure class: triple fault (decode/CS/stack), #GP(0) (mode-change register order), page fault (CR3/tables/alignment), memory wrap (A20 off), stale data (relocation).

## What to verify

- A20 is enabled AND verified by a wrap probe before CR0.PE.
- The GDT pseudo-descriptor base points at the real GDT; descriptors have base 0 with correct G/D/L/P/S bits.
- After CR0.PE, a far jump reloads CS; DS/ES/SS are reloaded with the flat data selector.
- Long mode order: CR4.PAE → EFER.LME → CR3 → CR0.PG, then a far jump into the L=1 code descriptor.
- Page tables: 4-KiB-aligned CR3, canonical addresses, P/RW/US flags, identity mapping where expected.
- Relocation: no absolute references in loaded stage-2 code, or a relocation pass applied with the correct delta.
- QEMU: `-kernel` for Linux-protocol kernels vs a raw floppy/disk for legacy stage-1.

## How to verify

```
# host-verified (gcc/as 16.1, x86-64 native): every example assembles clean
gcc -c examples/good/correct_gdt.s         -o /tmp/good_gdt.o
gcc -c examples/good/a20_enabled.s         -o /tmp/good_a20.o
gcc -c examples/good/mode_switch_order.s   -o /tmp/good_mode.o
gcc -c examples/good/pic_relocation.s      -o /tmp/good_pic.o
gcc -c examples/bad/*.s                    # bad examples assemble too (logic bugs)

# documented-as-target (run on a host with QEMU):
qemu-system-i386 -drive format=raw,if=floppy,file=stage1.img -nographic
qemu-system-x86_64 -kernel stage2.bin -nographic
qemu-system-aarch64 -machine virt -cpu cortex-a53 -kernel Image \
  -append "console=ttyAMA0" -nographic
qemu-system-riscv64 -machine virt -bios u-boot.bin -nographic
```

## Where the knowledge comes from

- `intel-sdm` — Vol.1 addressing (A20 wrap), Vol.3A CR0/CR4/EFER, mode switching (§9.8.5), paging (§4.3), segment descriptors (§3.4)
- `aarch64-boot-protocol` — Linux kernel AArch64 booting requirements (DTB in x0, EL, MMU off)
- `riscv-isa-spec` — privileged spec: M-mode/S-mode, satp, PMP, trap delegation
- `uboot-docs` — SPL → U-Boot → OS flow, relocation, booti/bootm handoff
- `qemu-docs` — `-kernel`/`-drive`/`-nographic`/`-d in_asm` verification environment
- `gnu-ld-manual`, `sysv-elf`, `binutils-docs` — link vs load addresses, relocations, objdump/readelf

## Related skills

- `qemu-system-setup` — machine models, `-kernel` vs bare-metal ELF, serial verification (require of)
- `elf-layout-and-relocations` — link vs load address and relocation semantics
- `asm-calling-conventions` — AArch64/RISC-V register contracts at handoff
- `asm-x86-64-registers-and-addressing` — long-mode registers, canonical addresses
- `c-undefined-behavior` — what the C code receiving the handoff must avoid

## Evaluation

Synthetic: (a) fix a GDT whose pseudo-descriptor base is wrong, (b) insert the missing A20 enable, (c) correct the long-mode enable order, (d) relocate code after a link/load mismatch.
False-positive: a correct flat-GDT + A20 + ordered long-mode sequence must NOT be flagged; stage-1 code loaded exactly at 0x7C00 with no relocation must NOT be flagged as "missing relocation"; an identity-mapped low 2 MiB must NOT be flagged as "higher-half missing".
Adversarial: code that boots in an emulator but hangs on real BIOS (A20 off, 512-byte limit); long-mode code that "runs" as compatibility mode because the far jump was skipped.
Historical: document the #GP(0) and triple-fault classes from the SDM mode-switch rules and verify each bad example's fault with the QEMU `-d` log.
