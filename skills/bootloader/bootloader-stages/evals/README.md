# Evaluation — bootloader-stages

Skill: `skills/bootloader/bootloader-stages`. Stability: `source-backed`, `verified`
for the assembly layer (toolchain present), `researched` for the boot-runtime layer
(no QEMU on this host — boot execution is **documented-as-target**).

## Toolchain status

- `gcc 16.1.0` (MSYS2/MinGW) and GNU `as 2.46` on PATH, x86-64 native. Used for
  every verification command below.
- No QEMU binary on this host (`qemu-system-x86_64 --version` and
  `qemu-system-aarch64 --version` both fail, 2026-08-14). Real boot verification
  (floppy `-kernel`, U-Boot) is therefore **documented-as-target**, exactly as the
  `qemu-system-setup` skill records its own runtime layer.

## Verified facts (this environment, 2026-08-14)

| Claim | Status | Evidence |
|---|---|---|
| All 4 bad examples assemble cleanly (logic bugs, not syntax) | VERIFIED | `gcc -c examples/bad/*.s` |
| All 4 good examples assemble cleanly | VERIFIED | `gcc -c examples/good/*.s` |
| 16-bit `.code16`, 32-bit `.code32`, 64-bit `.code64` sections in one object | VERIFIED | gcc -c accepts mixed code sizes |
| 4-level page tables (`.balign 4096`, `.quad` chaining) assemble in `.data` | VERIFIED | gcc -c |
| Far-immediate jumps (`ljmp $sel, $label`) in 16/32-bit mode assemble | VERIFIED | gcc -c |
| QEMU floppy/`-kernel`/U-Boot boot behaviour | documented-as-target | no QEMU on host; commands per `qemu-docs`, `uboot-docs` |

## Verified facts (from cited sources, KNOWN)

- Flat code descriptor `0x00cf9a00`, flat data `0x00cf9200`, 64-bit code
  `0x00af9a00` encode G/D/L/P/S/type as claimed (KNOWN, `intel-sdm` Vol.3A
  §3.4.5, Figure 3-8 layout).
- Long-mode enable order CR4.PAE → EFER.LME → CR3 → CR0.PG → far jump to L=1 CS;
  PG=1 with LME=1 and PAE=0 → #GP(0) (KNOWN, `intel-sdm` Vol.3A §9.8.5, §2.5).
- AArch64: x0 = DTB physical address, 8-byte aligned DTB, 2 MiB-aligned Image,
  MMU off, entered at supported EL (KNOWN, `aarch64-boot-protocol`).
- RISC-V: M-mode firmware → S-mode with a0 = hartid, a1 = DTB, satp bare
  (KNOWN, `riscv-isa-spec` privileged spec + `uboot-docs`).
- U-Boot relocates itself and fixes up the DTB before handoff (KNOWN,
  `uboot-docs`).
- MBR: 512 bytes, 0x55AA at 510, four 16-byte entries at 446; GPT: protective
  MBR in LBA0, header in LBA1, entries from LBA2 (KNOWN, UEFI spec; no registry
  entry — cited via `qemu-docs` for the emulated disk model).
- BIOS 0x7C00 / DL / DS:SI / 512-byte contract and 8042 A20 protocol are chipset/
  firmware conventions (KNOWN as convention, OSDev wiki; no registry entry).

## Synthetic evals

Each case: DETECT the broken piece → EXPLAIN the rule → FIX → VERIFY (assemble
with gcc -c; boot-test documented-as-target).

| Level | Fixture | Defect to detect | Rule |
|---|---|---|---|
| easy | `bad/forgot_a20.s` | CR0.PE set, A20 never enabled | 3 |
| easy | `bad/wrong_gdt_base.s` BAD 1 | lgdt pseudo-descriptor base is not the GDT | 4 |
| medium | `bad/wrong_gdt_base.s` BAD 2 | code descriptor base = 0x00010000 (non-flat) | 4 |
| medium | `bad/wrong_mode_order.s` | PG set while PAE=0 after LME → #GP(0) | 5 |
| hard | `bad/link_load_mismatch.s` | absolute `mov $bad_msg` after load at 0x9000 vs link 0x100000 | 9 |
| adversarial | same fixture booted "successfully" because the loader zeroed memory | fix must be relocation or PIC, not padding | 9 |
| adversarial | "no serial output" with a wrong mode-switch order | fix must be the transition order, not more RAM | 10 |

## False-positive evals (correct code must NOT be flagged)

- `good/correct_gdt.s` — flat GDT + `ljmp $0x08` + DS/ES/SS reload: correct; must
  NOT be "fixed" to add a 64-bit descriptor before entering protected mode.
- `good/a20_enabled.s` — 8042 sequence + wrap probe: correct; the probe's
  temporary writes to 0x000000/0x100000 must NOT be flagged as corruption.
- `good/mode_switch_order.s` — two separate code descriptors (0x08 32-bit, 0x18
  L=1) and the ordered long-mode sequence: correct; must NOT be flagged as
  "missing PAE" or "redundant far jump".
- `good/pic_relocation.s` — RIP-relative data access and a relocation loop: both
  correct; must NOT be flagged as "missing relocation" for the RIP-relative path.
- Stage-1 code loaded exactly at 0x7C00 with no relocation — correct, because
  the BIOS loads stage-1 AT its link address; must NOT be flagged as "missing
  relocation".
- An identity-mapped low 2 MiB after long mode entry — correct early setup; must
  NOT be flagged as "missing higher-half mapping".

## Verification commands

```
# host-verified (gcc/as 16.1, x86-64 native)
gcc -c examples/good/correct_gdt.s       -o /tmp/good_gdt.o
gcc -c examples/good/a20_enabled.s       -o /tmp/good_a20.o
gcc -c examples/good/mode_switch_order.s -o /tmp/good_mode.o
gcc -c examples/good/pic_relocation.s    -o /tmp/good_pic.o
gcc -c examples/bad/wrong_gdt_base.s     -o /tmp/bad_gdt.o
gcc -c examples/bad/forgot_a20.s         -o /tmp/bad_a20.o
gcc -c examples/bad/wrong_mode_order.s   -o /tmp/bad_mode.o
gcc -c examples/bad/link_load_mismatch.s -o /tmp/bad_link.o
# expected: every command succeeds; bad examples are logic bugs, not syntax.

# documented-as-target (run on a host with QEMU):
# 1. build stage-1 floppy image (link at 0x7C00, first 512 bytes, 0x55AA) and boot:
qemu-system-i386 -drive format=raw,if=floppy,file=stage1.img -nographic
# 2. verify the mode transition logs (expect 64-bit decode after the L=1 far jump):
qemu-system-x86_64 -kernel stage2.bin -nographic -d in_asm,cpu_reset -D boot.log
# 3. AArch64 Linux-protocol boot (expect "console [ttyAMA0] enabled"):
qemu-system-aarch64 -machine virt -cpu cortex-a53 -kernel Image \
  -append "console=ttyAMA0" -nographic
# 4. RISC-V via U-Boot (expect U-Boot banner + handoff, a0=hartid, a1=DTB):
qemu-system-riscv64 -machine virt -bios u-boot.bin -nographic
# 5. relocation mismatch demo: link bad/link_load_mismatch.s at 0x100000, load at
#    0x9000 in QEMU -> COM1 output is garbage; good/pic_relocation.s works at both.
```

## Scoring (for routing eval)

- precision: every flagged defect maps to a reference rule (1–12).
- recall: the easy/medium/hard fixtures above must all be detected.
- FP-rate: the false-positive fixtures must produce zero flags.
- routing: trigger must fire on "bootloader", "stage-1", "MBR", "GPT", "A20",
  "GDT", "long mode", "protected mode", "CR0", "EFER", "CR4.PAE", "paging",
  "DTB in x0", "hartid", "relocation", "link address", "load address",
  "position-independent", "boot protocol"; must NOT fire on plain userspace asm
  (use `assembly/asm-*`), QEMU command-line setup alone (`qemu-system-setup`),
  or kernel internals after early boot (`kernel-*`).
