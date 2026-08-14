# Boot Stages — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml. Claims marked
KNOWN are verified against a cited source; INFERRED/KNOWN-firmware-convention marks
chipset/BIOS behavior that is not covered by a normative doc in the registry.

## 1. Boot sequence: firmware → stage-1 → stage-2 → kernel

- **RULE**: Legacy BIOS POST selects a boot device, loads its first 512-byte
  sector (MBR) to physical 0x7C00 and jumps there with CS:IP = 0x0000:0x7C00,
  DL = boot drive number, and (for VBR boots) DS:SI pointing at the partition
  entry. Only 512 bytes are guaranteed to be resident. UEFI instead runs a
  PE32+ application from the ESP with UEFI Boot Services; there is no 0x7C00
  contract.
- **WHY AI GETS IT WRONG**: assumes "the OS starts at a fixed address" and merges
  the legacy BIOS flow with UEFI; assumes the MBR is the whole disk image.
- **CORRECT REASONING**: each handoff is a contract. The BIOS contract is
  register- and address-based (0x7C00, DL, DS:SI, 512-byte limit); anything
  larger must be loaded by stage-1 itself. UEFI is a different loader model
  (PE/COFF entry point, EFI System Partition, Boot Services).
- **EXAMPLE** (bad): MBR code that assumes more than 512 bytes are present, or
  ignores DL/DS:SI, then "works" only because the emulator happened to zero them.
- **COUNTEREXAMPLE** (good): stage-1 saves DL/DS:SI, loads stage-2 into high
  memory with INT 13h, then jumps to the stage-2 entry with its own documented
  register contract.
- **VERIFICATION**: boot the image as a floppy in QEMU
  (`-drive format=raw,if=floppy`), pause with the gdb stub and inspect CS:IP,
  DL, DS:SI at 0x7C00.
- **SOURCE**: qemu-docs (boot from raw disk/floppy images); KNOWN BIOS firmware
  convention (0x7C00/DL/DS:SI contract, OSDev wiki — no registry entry).

## 2. MBR vs GPT partitioning

- **RULE**: MBR is one 512-byte sector: bytes 0–445 boot code, 446–509 four
  16-byte partition entries, 510–511 the 0x55 0xAA signature. GPT keeps a
  protective MBR in LBA0 (partition type 0xEE), the primary GPT header in LBA1
  (signature "EFI PART", CRC32-protected), and partition entries from LBA2;
  addresses are 64-bit LBAs.
- **WHY AI GETS IT WRONG**: "MBR is the partition table" conflates the boot code
  with the table; GPT is treated as "another MBR format".
- **CORRECT REASONING**: an MBR is code PLUS a partition table; the 0x55AA
  signature is what marks it valid. GPT disks still begin with an MBR whose
  0xEE entry protects the GPT from legacy tools.
- **EXAMPLE** (bad): reading GPT partition entries from LBA0 — that is the
  protective MBR, not the GPT.
- **COUNTEREXAMPLE** (good): validate the protective MBR signature, then read the
  GPT header at LBA1, verify "EFI PART" and the CRC32s, then entries at LBA2+.
- **VERIFICATION**: `xxd -l 512 disk.img` shows 0x55 0xAA at offset 510; compare
  a raw parse against `parted`/`gdisk` output on the same image.
- **SOURCE**: qemu-docs (disk/`-drive` emulation of MBR/GPT images); KNOWN GPT
  layout per the UEFI spec (no registry entry).

## 3. x86 A20 gate: memory wraps at 1 MiB until enabled

- **RULE**: until the A20 address line is enabled, accesses above 1 MiB wrap:
  physical address 0x100000 aliases 0x000000. A20 is enabled through the 8042
  keyboard controller (port 0x64 command 0xD1, then 0xDF to port 0x60), the
  FAST_A20 register (port 0x92 bit 1), or BIOS INT 15h AX=2401.
- **WHY AI GETS IT WRONG**: "modern chipsets always have A20 on", or "it booted
  in the emulator", or "A20 is a legacy thing no one needs".
- **CORRECT REASONING**: the CPU only masks the line when A20M# is asserted, and
  emulators/bare-metal setups differ from real BIOS state. Treat A20 as OFF until
  a wrap probe proves it ON. Enable it BEFORE setting CR0.PE if any code or data
  sits above 1 MiB.
- **EXAMPLE** (bad): `examples/bad/forgot_a20.s` — CR0.PE set with no A20
  sequence; a store to 0x100000 silently corrupts physical 0x000000.
- **COUNTEREXAMPLE** (good): `examples/good/a20_enabled.s` — 0xD1 → 0xDF to the
  8042, then the probe: write 0x00 to 0x000000 and 0xFF to 0x100000; if reading
  back 0x000000 still yields 0x00, A20 is on.
- **VERIFICATION**: run the probe in QEMU with the gdb stub and compare bytes at
  0x000000 and 0x100000 (`x/b 0x000000`, `x/b 0x100000`).
- **SOURCE**: intel-sdm (A20M# mask behavior, Vol.1 §3.3.7.1); KNOWN 8042
  protocol is chipset-specific (OSDev wiki — no registry entry); qemu-docs
  (default A20 state differs per machine).

## 4. Real → protected mode: GDT first, then CR0.PE, then a far jump

- **RULE**: before setting CR0.PE, load a valid GDT; after CR0.PE the CPU
  immediately interprets segments through the GDT. A far jump (ljmp) must reload
  CS from a flat code descriptor, and DS/ES/SS must be reloaded with the flat
  data descriptor.
- **WHY AI GETS IT WRONG**: "lgdt is optional", "setting CR0.PE is enough", "any
  GDT works because I use a flat model".
- **CORRECT REASONING**: in real mode segments are base*16+offset; after PE=1
  every segment reference goes through the GDT. A wrong GDT base (lgdt operand)
  or a descriptor with a nonzero base silently offsets all addresses; skipping
  the far jump leaves CS using stale descriptor semantics → #GP/triple fault.
  Flat code = `0x00cf9a00` (G=1 D=1 P=1 S=1, base 0), data = `0x00cf9200`.
- **EXAMPLE** (bad): `examples/bad/wrong_gdt_base.s` — pseudo-descriptor base
  points at 0x00012345 instead of the real GDT; the first protected-mode access
  faults or reads garbage.
- **COUNTEREXAMPLE** (good): `examples/good/correct_gdt.s` — GDT with null + flat
  code + flat data, `lgdt gdt_desc` with the correct base, CR0.PE, `ljmp $0x08`
  to a `.code32` label, then DS/ES/SS reload and a stack set in RAM.
- **VERIFICATION**: single-step in QEMU; after the far jump `info registers`
  shows CS selector 0x08 and DS/ES/SS = 0x10; QEMU `-d in_asm` shows 32-bit
  decode after the jump.
- **SOURCE**: intel-sdm Vol.3A §3.4 (segment descriptors), §9.9.1 (mode
  switching); amd64-apm Vol.2 §9.1 (protected-mode initialization).

## 5. Protected → long mode: the enable order is enforced

- **RULE**: the required order is CR4.PAE=1 → EFER.LME=1 → CR3 = PML4 base →
  CR0.PG=1 → a far jump into an L=1 (64-bit) code descriptor. Setting CR0.PG=1
  while EFER.LME=1 and CR4.PAE=0 raises #GP(0). Changing CR4.PAE while paging is
  enabled is also #GP(0). Without the final far jump the CPU executes
  compatibility mode (32-bit decode), not long mode.
- **WHY AI GETS IT WRONG**: "enable paging and you are in long mode"; "the order
  is just style"; "PAE is optional on a 64-bit kernel".
- **CORRECT REASONING**: IA-32e mode is entered only when paging is enabled while
  EFER.LME=1 AND CR4.PAE=1. The processor is then in compatibility mode until CS
  is loaded with a code descriptor whose L bit is 1 (encoded `0x00af9a00`). Use a
  separate 64-bit code descriptor (e.g. 0x18) for that far jump; keep a 32-bit
  code descriptor (0x08) for the protected-mode entry.
- **EXAMPLE** (bad): `examples/bad/wrong_mode_order.s` — LME is set but CR0.PG is
  turned on with PAE still 0 (#GP(0) on the MOV CR0), and the long-mode far jump
  to an L=1 descriptor is never taken.
- **COUNTEREXAMPLE** (good): `examples/good/mode_switch_order.s` — PM entry via
  0x08, then CR4.PAE, EFER.LME, CR3, CR0.PG, then `ljmp $0x18` to a `.code64`
  label; DS/ES/SS reloaded from 0x10.
- **VERIFICATION**: QEMU `-d in_asm,cpu_reset -D log` — a wrong order logs #GP or
  a reset/triple fault; the good sequence logs 64-bit decode after the second far
  jump. GDB stub: `p/x $cr4` PAE bit, `p/x $cr0` PG bit, CS.L after the jump.
- **SOURCE**: intel-sdm Vol.3A §2.5 (CR0/CR4 #GP conditions), §9.8.5
  (initializing IA-32e mode); amd64-apm Vol.2 §7.2 (long mode enable).

## 6. Long-mode paging: 4-level translation and canonical addresses

- **RULE**: long mode uses 4-level paging: CR3 → PML4 → PDPT → PD → PT. Every
  entry has P/RW/US bits; setting PS=1 at the PD level maps a 2 MiB page.
  Virtual addresses must be canonical: bits 63:48 equal bit 47, else #GP.
- **WHY AI GETS IT WRONG**: "one-level page tables are fine"; "any 32-bit address
  is automatically valid"; "the table must live at the load address".
- **CORRECT REASONING**: translation walks four levels before any reference
  succeeds, and CR3 must point to a 4-KiB-aligned table. The standard early
  setup is an identity map of the low 1–4 GiB; later stages build a higher-half
  map. Entry flags 0x007 = present | writable | user.
- **EXAMPLE** (bad): loading CR3 with a misaligned table, or a PDPT entry whose
  address is not 2 MiB-aligned for a PS=1 PD entry, or using a non-canonical
  target — the first long-mode instruction page-faults or raises #GP.
- **COUNTEREXAMPLE** (good): `examples/good/mode_switch_order.s` page tables —
  pml4/pdpt/pd/pt each `.balign 4096`, `.quad pdpt + 0x007` chaining, and a final
  `.quad 0x0000000000000003` identity page.
- **VERIFICATION**: gdb `x/8gx $cr3` and walk each level; QEMU `-d page` logs the
  translation of every executed instruction.
- **SOURCE**: intel-sdm Vol.3A §4.3 (4-level paging), §2.5 (CR3); amd64-apm Vol.2
  §5.3 (page translation).

## 7. AArch64 boot protocol: DTB in x0, EL requirements, MMU off

- **RULE**: per the Linux kernel AArch64 booting protocol (`booting.rst`): the
  kernel image is loaded at a 2 MiB-aligned base address; the boot loader enters
  the kernel at the supported exception level (EL2 or EL1 depending on kernel
  config); x0 must contain the physical address of the DTB, the DTB must be
  8-byte aligned; the MMU must be off and the caches in the state the protocol
  allows; all code/data the kernel will touch must be in memory as the protocol
  describes.
- **WHY AI GETS IT WRONG**: "DTB is found by scanning memory"; "any register will
  do"; "I entered at EL3 because that is the highest".
- **CORRECT REASONING**: the protocol is a fixed contract — x0 = DTB physical
  address, entered at the kernel's supported EL (EL2 when the kernel is built
  with the hyp mode entry, else EL1), MMU off. Firmware (U-Boot, UEFI, SCP) is
  what sets up the DTB (memory, stdout-path, chosen) and passes it in x0; the
  kernel reads nothing else at entry.
- **EXAMPLE** (bad): a bootloader that enters the kernel with the DTB in x1, or
  at EL3 with the MMU on, or with the Image loaded at a non-2 MiB-aligned
  address — the kernel hangs before the first console output.
- **COUNTEREXAMPLE** (good): U-Boot `booti $kernel_addr - $fdt_addr` — the FDT is
  placed, fixed up, and its address passed in x0; entry uses the EL and state
  the protocol requires.
- **VERIFICATION**: QEMU `-machine virt -cpu cortex-a53 -kernel Image -append
  "console=ttyAMA0" -nographic` reaches the serial banner (documented-as-target
  on this host; QEMU implements the Linux protocol for `-kernel`).
- **SOURCE**: aarch64-boot-protocol (kernel Documentation/arm64/booting.rst);
  qemu-docs (virt machine and Linux-protocol `-kernel` load).

## 8. RISC-V boot: M-mode firmware hands off to S-mode

- **RULE**: a hart starts in M-mode at the reset vector. Firmware (OpenSBI,
  U-Boot SPL) sets up PMP, mstatus/mie, trap delegation, and then enters the
  kernel in S-mode with a0 = hartid, a1 = DTB physical address, a2 = 0; paging
  is off (satp = bare) at entry.
- **WHY AI GETS IT WRONG**: "kernels boot directly at _start in S-mode"; "a0 is a
  scratch register, anything goes"; "PMP is optional if I do not use it".
- **CORRECT REASONING**: only M-mode code can program the machine CSRs (PMP,
  mstatus, medeleg/mideleg). The S-mode entry contract is a0 = hartid, a1 = DTB
  (the "RISC-V Linux boot" convention followed by U-Boot and OpenSBI); if PMP
  denies S-mode access to RAM, the first kernel fetch faults.
- **EXAMPLE** (bad): jumping to the kernel without setting a0/a1, or leaving PMP
  configured to deny S-mode access to DRAM — silent hang or S-mode access fault.
- **COUNTEREXAMPLE** (good): firmware configures mstatus (SPP=1, SPIE=1), keeps
  satp bare, executes `mret` into S-mode with a0 = hartid and a1 = DTB.
- **VERIFICATION**: QEMU `-machine virt -cpu rv64 -bios u-boot.bin -nographic`;
  with the gdb stub, `p/x $a0` and `p/x $a1` at the S-mode entry must hold the
  hartid and a DTB address (documented-as-target on this host).
- **SOURCE**: riscv-isa-spec (privileged spec: M/S modes, satp, PMP, trap
  delegation); uboot-docs (booti/OpenSBI flow); qemu-docs (virt machine).

## 9. Relocation: link addresses vs load addresses, PIC

- **RULE**: the linker bakes link-time addresses into the image; the loader may
  place the image at any aligned load address. Non-PIC code must be relocated by
  adding delta = load_addr − link_addr to every absolute reference, or written
  position-independently (RIP-relative on x86-64, PC-relative on AArch64,
  GOT-based elsewhere).
- **WHY AI GETS IT WRONG**: "the linker already fixed the addresses"; "load
  address always equals link address"; "a relative call is enough".
- **CORRECT REASONING**: absolute `mov $symbol` / `mov symbol, %eax` references
  are resolved at link time; if the loader copies the image elsewhere, those
  references are stale unless a relocation pass adjusts them. On x86-64,
  `lea symbol(%rip)` is load-address independent. U-Boot is the reference
  implementation: it copies itself to the end of RAM and applies a relocation
  pass.
- **EXAMPLE** (bad): `examples/bad/link_load_mismatch.s` — linked for 0x100000,
  loaded at 0x9000; `movl $bad_msg, %eax` loads a stale address.
- **COUNTEREXAMPLE** (good): `examples/good/pic_relocation.s` — a `.code64`
  RIP-relative data access plus the classic relocation loop that adds the delta
  (`addl %edi, (%edx)`) to each pointer listed in a relocation table.
- **VERIFICATION**: link the bad image at 0x100000 and load it at 0x9000 in QEMU
  — it reads garbage; the good image prints the message from either address.
  `objdump -d` shows absolute vs RIP-relative operands; `readelf -r` lists the
  relocations the pass must cover.
- **SOURCE**: gnu-ld-manual (linker-script address assignment); sysv-elf
  (relocation records); binutils-docs (objdump/readelf); uboot-docs
  (self-relocation reference).

## 10. Failure classes: what can go wrong at each stage

- **RULE**: map symptom → stage → cause:
  - immediate reset / triple fault → CS/GDT/mode-switch order (x86), wrong EL or
    DTB placement (AArch64), M-mode handoff never completed (RISC-V)
  - #GP(0) on MOV CR0/CR4 → mode-change register order (PAE/LME/PG ordering)
  - page fault at the first long-mode instruction → CR3 alignment, missing
    levels, non-canonical addresses
  - code reads its own data corrupted → A20 off (1 MiB wrap) or link/load
    mismatch
  - boots in an emulator but hangs on real hardware → A20, 512-byte BIOS limit,
    uninitialized state assumptions
- **WHY AI GETS IT WRONG**: debugs the last instruction executed instead of the
  stage transition that precedes it.
- **CORRECT REASONING**: classify the failure by the CPU's visible state (gdb
  registers, QEMU `-d int,cpu_reset`, serial progress markers) and work backward
  through the stage contract until the first violated rule is found.
- **EXAMPLE** (bad): "no serial output" → add more RAM instead of checking the
  mode-switch order.
- **COUNTEREXAMPLE** (good): put a serial marker after each stage boundary (BIOS
  handoff → GDT → PE → A20 → PM → paging → long mode) and bisect where output
  stops.
- **VERIFICATION**: QEMU `-d in_asm,int,cpu_reset -D boot.log`; compare where the
  log ends against the stage boundaries.
- **SOURCE**: qemu-docs (debug/log options); intel-sdm (fault classes);
  aarch64-boot-protocol (protocol violations hang at entry).

## 11. U-Boot as the reference stage-1 → stage-2 → OS implementation

- **RULE**: U-Boot's SPL → full U-Boot → OS flow is the canonical implementation
  of the stage chain on ARM/RISC-V: SPL (tiny, runs from SRAM) loads U-Boot,
  U-Boot relocates itself to the end of RAM and fixes up the DTB, then hands off
  with booti/bootm/bootefi.
- **WHY AI GETS IT WRONG**: treats U-Boot as monolithic firmware that is too
  complex to learn boot stages from.
- **CORRECT REASONING**: every U-Boot stage demonstrates a real rule: SPL is
  stage-1 (size-constrained, runs from on-chip SRAM), the relocation pass is the
  link-vs-load solution (rule 9), and `booti $kernel_addr - $fdt_addr` shows the
  AArch64 DTB-in-x0 handoff (rule 7).
- **EXAMPLE** (bad): using U-Boot's "jump to the kernel address" without passing
  or fixing up the FDT — the kernel has no device tree and dies silently.
- **COUNTEREXAMPLE** (good): `booti $kernel_addr - $fdt_addr` — U-Boot fixes up
  memory/chosen nodes in the FDT and passes its address in x0.
- **VERIFICATION**: QEMU `qemu-system-arm -machine virt -bios u-boot.bin
  -nographic`; the U-Boot banner and the relocation/handoff messages are the
  visible stage markers (documented-as-target on this host).
- **SOURCE**: uboot-docs; qemu-docs.

## 12. Entry contract quick reference

| Stage | Contract |
|---|---|
| BIOS → stage-1 | CS:IP = 0x0000:0x7C00, DL = boot drive, DS:SI = partition entry, 512-byte sector, 0x55AA at offset 510 |
| UEFI → app | PE32+ application from the ESP, UEFI Boot Services, no 0x7C00 |
| stage-1 → stage-2 | loader-defined: registers + entry address + load/link delta |
| x86 PM entry | flat GDT, CR0.PE, far jump, DS/ES/SS reload |
| x86 long mode | CR4.PAE → EFER.LME → CR3 → CR0.PG → far jump to L=1 CS |
| AArch64 → kernel | x0 = DTB physical address (8-byte aligned), 2 MiB-aligned Image, MMU off, supported EL |
| RISC-V → kernel | M-mode firmware → S-mode, a0 = hartid, a1 = DTB, satp bare, PMP permissive |
| Kernel load | link address must equal the loader's target, or relocate |

- **VERIFICATION**: any stage in this table can be checked at the QEMU gdb stub
  (`info registers`, `x/` memory) or from the U-Boot/`-d in_asm` logs.
- **SOURCE**: intel-sdm (x86 rows); aarch64-boot-protocol (AArch64 row);
  riscv-isa-spec + uboot-docs (RISC-V row); qemu-docs (verification rows).
