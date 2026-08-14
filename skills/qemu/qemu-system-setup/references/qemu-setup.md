# QEMU System Setup Rules

Rule format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE. Sources cite registry ids:
`qemu-docs`, `aarch64-boot-protocol`, `binutils-docs`, `gdb-manual`.
All command-line syntax below was verified against the QEMU docs
(2026-08-14); QEMU itself is not installed on this host, so every invocation
is documented-as-target and the skill is marked `researched` (NOT host-verified).

## 1. User-mode vs system-mode emulation

- **RULE**: `qemu-system-*` binaries emulate a full machine (CPU + MMIO + devices
  + firmware/boot path); `qemu-<arch>` user-mode binaries run one user-space
  ELF, translating syscalls to the host, with no devices, no kernel, and no
  MMU/privileged execution. Pick the binary that matches the task.
- **WHY AI GETS IT WRONG**: "I need to run an ARM binary, so I use
  `qemu-system-arm`" even for a bare user-space test, or the reverse — running
  a kernel image under user-mode emulation.
- **CORRECT REASONING**: if the input is a kernel, firmware, or anything that
  touches MMIO/privileged state, use system mode. If it is a standalone
  user-space ELF (e.g. a unit test), use user mode: `qemu-aarch64 -L /usr/aarch64-linux-gnu ./test`.
- **EXAMPLE** (bad): `qemu-aarch64 bzImage` — a kernel is not a user-space ELF;
  user mode cannot boot it.
- **COUNTEREXAMPLE** (good): `qemu-system-x86_64 -machine q35 -kernel bzImage -append "console=ttyS0" -nographic`
  for a kernel; `qemu-aarch64 ./test` for a user-space test.
- **VERIFICATION**: `qemu-<arch> --help` says "user mode emulation"; system
  binaries list `-machine` options. Reading the binary's ELF header
  (`readelf -h`) tells you what it is before you pick a mode.
- **SOURCE**: qemu-docs (user mode vs system emulation sections).

## 2. Machine models: q35, virt, mps2-an385, mps2-an505

- **RULE**: x86 has machine types `pc` (i440fx; default) and `q35` (modern
  PCIe chipset); use `-machine q35` for new x86 guests. ARM system emulation
  has NO default machine — `-machine` is mandatory. Linux guests on ARM use
  `virt` (generic, no real hardware); Cortex-M guests use the MPS2/MPS3
  boards: `mps2-an385` = Cortex-M3 (ARMv7-M), `mps2-an505` = Cortex-M33
  (ARMv8-M, TrustZone).
- **WHY AI GETS IT WRONG**: agents omit `-machine` on ARM (it "works on x86"),
  or pick a random board from a tutorial that does not match the image's
  memory map and CPU.
- **CORRECT REASONING**: choose the board from what the guest expects: a Linux
  kernel built for the `virt` platform runs only on `virt`; a Cortex-M ELF
  linked for the AN385 map (ZBT SSRAM at 0, 16 KB remap) needs `mps2-an385`.
  Machine and image must agree; the CPU on most ARM boards is fixed by the
  board (only `virt` lets you choose freely).
- **EXAMPLE** (bad): `qemu-system-arm -nographic -kernel app.elf` — fails:
  "No machine specified, and there is no default".
- **COUNTEREXAMPLE** (good): `qemu-system-arm -machine mps2-an385 -nographic -kernel app.elf`
  and `qemu-system-aarch64 -machine virt -cpu cortex-a53 -m 512M -kernel Image -nographic`.
- **VERIFICATION**: `qemu-system-aarch64 -machine help | grep -E 'virt|mps2'`;
  `readelf -h`/`readelf -l` on the ELF and compare PT_LOAD addresses with the
  board memory map.
- **SOURCE**: qemu-docs (target-arm: "there is no default"; virt; mps2).

## 3. CPU model selection (-cpu)

- **RULE**: `-cpu model` selects the CPU. On x86 TCG, `qemu64`/`max` are safe
  portable defaults; `host` works only with KVM. On ARM `virt`, the default is
  32-bit `cortex-a15`, so a 64-bit (AArch64) guest REQUIRES an explicit
  64-bit model such as `cortex-a53` or `max`. On most other ARM boards the CPU
  is fixed by the machine and `-cpu` is unnecessary.
- **WHY AI GETS IT WRONG**: agents boot an AArch64 `Image` on `virt` without
  `-cpu`; the 32-bit default CPU cannot fetch AArch64 instructions and the
  guest does nothing. Or they pass `-cpu host` expecting acceleration that
  TCG does not provide.
- **CORRECT REASONING**: the CPU model must be able to execute the guest code:
  AArch64 guest → 64-bit model; ARMv7-M ELF → Cortex-M board with matching
  CPU; x86-64 kernel → x86-64 CPU. `-cpu help` lists every supported model;
  use it instead of guessing.
- **EXAMPLE** (bad): `qemu-system-aarch64 -machine virt -m 512M -kernel Image -nographic`
  — boots the 32-bit `cortex-a15`; an AArch64 `Image` will not run.
- **COUNTEREXAMPLE** (good): `qemu-system-aarch64 -machine virt -cpu cortex-a53 -m 512M -kernel Image -nographic`.
- **VERIFICATION**: `qemu-system-aarch64 -cpu help` and the man page note that
  the virt default is `cortex-a15`; check the guest kernel's architecture with
  `readelf -h Image` (Machine: AArch64).
- **SOURCE**: qemu-docs (virt: "the default is cortex-a15"; `-cpu help`).

## 4. Memory (-m)

- **RULE**: `-m <size>` sets guest RAM; default is 128 MiB; accept "M"/"G"
  suffixes (e.g. `-m 256M`, `-m 1G`). `-m 256M,slots=3,maxmem=4G` enables
  hotplug.
- **WHY AI GETS IT WRONG**: agents pass sizes without suffixes or absurd values
  ("`-m 1000000`"), or assume more RAM fixes boot problems that are actually
  serial/machine issues.
- **CORRECT REASONING**: pick RAM for the guest workload, not for symptoms.
  "No output" is a serial problem (rule 9), not a memory problem.
- **EXAMPLE** (bad): `-m 256` means 256 MiB on most builds but is ambiguous
  and wrong for guests needing gigabytes.
- **COUNTEREXAMPLE** (good): `-m 1G` for a Linux guest; `-m 16M` is plenty for
  a Cortex-M ELF (the board, not `-m`, sets the SRAM size).
- **VERIFICATION**: `-m help`/man page; the x86 `-m` section documents the
  default and suffixes.
- **SOURCE**: qemu-docs (man page `-m`).

## 5. Loading images (-kernel)

- **RULE**: `-kernel file` loads an executable image. On x86 it must be a
  bzImage (Linux or multiboot); `-append` gives the kernel command line,
  `-initrd` the initrd. On ARM, a non-ELF `-kernel` file is treated as
  following the Linux boot protocol (for AArch64: `Image`, DTB passed in x0);
  an ELF passed to `-kernel` is loaded as bare-metal at its link addresses.
- **WHY AI GETS IT WRONG**: agents feed `Image` to x86, an ELF to a machine
  whose map does not contain its load addresses, or `-append` to a bare-metal
  ELF (it is ignored for bare-metal).
- **CORRECT REASONING**: match the image format to the machine's boot path.
  The AArch64 boot protocol specifies where the kernel `Image` is loaded and
  that the DTB address is passed in x0 (MMU off, EL2/EL1). For bare-metal the
  ELF PT_LOAD segments must land in the board's RAM/flash; otherwise the reset
  vector executes garbage.
- **EXAMPLE** (bad): `-kernel app.elf` on `mps2-an505` where `app.elf` is
  linked for the AN385 memory map — the image loads at addresses the AN505
  does not map.
- **COUNTEREXAMPLE** (good): `-kernel bzImage -append "console=ttyS0"` on
  x86/q35; `-kernel Image -append "console=ttyAMA0"` on virt (Linux protocol);
  `-kernel app.elf` on `mps2-an385` with the ELF linked to that board's map.
- **VERIFICATION**: `readelf -l <elf>` and compare PT_LOAD `p_paddr` against
  the board map; `readelf -h` confirms the architecture; on AArch64, check the
  DTB is passed in x0 per the boot protocol.
- **SOURCE**: qemu-docs (man page `-kernel`/`-append`/`-initrd`); aarch64-boot-protocol; binutils-docs (`readelf`).

## 6. -nographic

- **RULE**: `-nographic` disables graphical output and redirects the emulated
  serial port to the console, muxed with the QEMU monitor (C-a h for help).
  `-display none` only suppresses the video window and does NOT touch serial.
- **WHY AI GETS IT WRONG**: agents skip `-nographic` and then report "guest
  produces no output" — the serial went to a windowed virtual console (`vc`)
  on a headless/CI host where nobody looks.
- **CORRECT REASONING**: for any automated or headless boot, add `-nographic`
  AND a kernel `console=` that matches the emulated UART (rules 9). Without
  `-nographic`, the default serial device is `vc` in graphical mode.
- **EXAMPLE** (bad): `qemu-system-x86_64 -machine q35 -m 256M -kernel bzImage -append "console=ttyS0"`
  — opens a GUI window; stdout captures nothing.
- **COUNTEREXAMPLE** (good): same command with `-nographic` added; kernel
  banner appears on stdout.
- **VERIFICATION**: run headless; grep the captured output for the kernel
  banner. `-nographic` is documented to "totally disable graphical output so
  that QEMU is a simple command line application".
- **SOURCE**: qemu-docs (man page `-nographic` vs `-display none`).

## 7. Disks (-drive)

- **RULE**: `-drive file=img,format=qcow2,if=virtio` attaches a disk (or
  `if=ide`/`if=none` for use with `-device`). `-drive` is a convenience
  wrapper over `-blockdev`+`-device`; the block layer is not a guaranteed
  stable interface. `-snapshot` makes all disk writes transient.
- **WHY AI GETS IT WRONG**: agents attach a raw image without `format=`, or
  use `-hda`/`-cdrom` and then wonder why the block device does not appear in
  the guest; or attach disks on machines that have no controller of that if=
  type.
- **CORRECT REASONING**: pick `if=` per the guest and machine (IDE for legacy,
  virtio for modern q35/virt Linux), specify `format=` to avoid auto-probing
  surprises, and give the guest the matching driver (`virtio_blk`, `ata_piix`).
  For firmware-on-flash, use `-pflash file` (on non-x86 the file must be sized
  for the machine's flash device).
- **EXAMPLE** (bad): `-drive file=rootfs.qcow2` on `mps2-an385` — an M-profile
  board with no disk controller at all.
- **COUNTEREXAMPLE** (good): `-drive file=disk.qcow2,format=qcow2,if=virtio`
  on q35 with a kernel that has `CONFIG_VIRTIO_BLK=y`.
- **VERIFICATION**: `qemu-system-x86_64 -machine q35 -drive file=disk.qcow2,format=qcow2,if=virtio ...`
  and check the guest `dmesg` lists `virtio_blk`; `qemu-img info` on the image.
- **SOURCE**: qemu-docs (man page `-drive`, `-blockdev`, `-pflash`, `-snapshot`).

## 8. Networking (-netdev user)

- **RULE**: `-netdev user,id=net0` creates a slirp (user-mode) backend that
  needs no host privileges: guest net 10.0.2.0/24, host alias 10.0.2.2, DNS
  10.0.2.3, DHCP pool .15-.31. It must be attached to a guest NIC device:
  `-device e1000,netdev=net0` (or virtio). `-nic user` is the one-line
  shortcut for both.
- **WHY AI GETS IT WRONG**: agents pass `-netdev user,id=net0` alone — the
  guest then has no NIC, or they write `-net user` (the deprecated net layer).
- **CORRECT REASONING**: `-netdev` creates the host backend only; `-device`
  (or `-nic`) creates and wires the guest-side NIC. Guest needs a driver for
  the chosen model (e1000/virtio-net on x86; `virtio-net-device` over
  virtio-mmio, or `virtio-net-pci`, on ARM virt).
- **EXAMPLE** (bad): `-netdev user,id=net0 -append "..."` with no `-device`;
  `ip link` in the guest shows nothing.
- **COUNTEREXAMPLE** (good): `-netdev user,id=net0 -device e1000,netdev=net0`
  or `-nic user,model=virtio-net-pci`; guest DHCP-gets 10.0.2.15 and pings
  10.0.2.2.
- **VERIFICATION**: boot the guest, run `ip addr`/`ping 10.0.2.2`; `-netdev
  user` is documented as requiring no admin privilege, defaulting to the
  10.0.2.0/24 network.
- **SOURCE**: qemu-docs (man page `-netdev user`, `-nic`, `-device`).

## 9. Serial console

- **RULE**: the emulated UART must be routed to a readable backend and match
  the guest's console driver. x86: 16550 COM1 (0x3f8) → Linux `ttyS0`. ARM
  `virt`: PL011 → Linux `ttyAMA0`. MPS2 boards: CMSDK UART0 is the primary
  console. `-serial dev` overrides the default (which is `vc` in graphical
  mode, `stdio` in non-graphical); `-serial none` removes all UARTs;
  `-serial mon:stdio` muxes serial with the monitor.
- **WHY AI GETS IT WRONG**: agents write `console=ttyS0` on ARM (PL011 is
  `ttyAMA0`, so the kernel prints nothing), or pass `-serial none` while
  debugging a UART problem.
- **CORRECT REASONING**: close the loop in three places: (1) `-nographic` (or
  an explicit `-serial`) so the backend is reachable; (2) the kernel
  `console=` names the driver for THIS machine's UART; (3) no `-serial none`.
  For bare-metal, the firmware/firmware code must actually write to the
  board's UART base (PL011 on virt, CMSDK UART on mps2).
- **EXAMPLE** (bad): `qemu-system-aarch64 -machine virt -cpu cortex-a53 -kernel Image -append "console=ttyS0" -nographic`
  — `ttyS0` does not exist on virt; the kernel finds no console.
- **COUNTEREXAMPLE** (good): same command with `console=ttyAMA0`; boot banner
  appears on stdout.
- **VERIFICATION**: grep boot log for `Kernel command line: ... console=ttyAMA0`
  and `console [ttyAMA0] enabled`; on x86 expect `ttyS0`.
- **SOURCE**: qemu-docs (man page `-serial`; virt board PL011); kernel serial
  driver naming (8250=ttyS0, amba-pl011=ttyAMA0).

## 10. Remote debugging (-gdb tcp::1234)

- **RULE**: `-gdb tcp::1234` starts a gdb stub listening on TCP 1234
  (`-s` is the shorthand). It does NOT pause the guest; combine with `-S`
  ("do not start CPU at startup") so you can attach before execution. In gdb:
  `target remote :1234`, then set breakpoints / `continue`.
- **WHY AI GETS IT WRONG**: agents use `-gdb` alone and the guest boots and
  panics before the attach; or they forget `-S` and then "the breakpoint never
  fires".
- **CORRECT REASONING**: the stub lets gdb control execution from the first
  instruction only if `-S` paused it. Use `-s -S` together for deterministic
  attach at reset. The stub speaks the gdb remote protocol, so
  `target remote` (not `target extended-remote` unless supported) connects.
- **EXAMPLE** (bad): `qemu-system-x86_64 -machine q35 -kernel bzImage -append "console=ttyS0" -nographic -gdb tcp::1234`
  then `gdb ... -ex "target remote :1234"` — the kernel already moved on.
- **COUNTEREXAMPLE** (good): add `-S`; `gdb -ex "target remote :1234" -ex "hbreak start_kernel" -ex continue vmlinux`.
- **VERIFICATION**: `-gdb` docs state it "does not pause QEMU execution" and
  that `-S` is needed; observe the stub refusing/losing the connection if the
  guest already ran.
- **SOURCE**: qemu-docs (man page `-gdb`, `-s`, `-S`); gdb-manual (remote protocol, `target remote`).

## 11. Booting a kernel vs firmware vs bare-metal ELF

- **RULE**: three different boot paths. Kernel: `-kernel` follows the Linux
  boot protocol (bzImage on x86; `Image` with DTB in r2/x0 on ARM). Firmware:
  `-bios file` (x86 does the right thing for most files) or `-pflash file`
  (must be flash-sized for the machine). Bare-metal: an ELF given to `-kernel`
  is loaded directly at its link addresses, and the reset vector executes it —
  no firmware, no DTB passed in registers (on virt an ELF boot finds the DTB
  at the start of RAM, 0x4000_0000).
- **WHY AI GETS IT WRONG**: agents mix the paths — e.g. `-bios Image`, or
  `-append` to a bare-metal ELF, or a bare-metal ELF whose reset vector never
  gets loaded because it was linked for an address the machine does not map.
- **CORRECT REASONING**: decide what runs at reset: firmware sets up and jumps
  to the kernel; `-kernel` on ARM distinguishes the protocol by file type
  (non-ELF → Linux protocol; ELF → bare-metal load). For bare-metal, the
  guest's link script and the machine memory map must agree (see rules 2, 5).
- **EXAMPLE** (bad): `qemu-system-aarch64 -machine virt -cpu cortex-a53 -bios Image -nographic`
  — a kernel `Image` is not a BIOS image.
- **COUNTEREXAMPLE** (good): kernel: `-kernel Image -append "console=ttyAMA0"`;
  firmware: `-bios OVMF.fd` (x86 UEFI) or `-pflash <flash>` on virt;
  bare-metal: `-kernel app.elf` with ELF PT_LOAD segments inside the board map.
- **VERIFICATION**: `readelf -h`/`readelf -l` to classify the file; the boot
  protocol doc for the DTB register; the QEMU man page text that describes
  each method.
- **SOURCE**: qemu-docs (man page boot methods: `-kernel`/`-bios`/`-pflash`;
  virt: ELF boot finds DTB at start of RAM); aarch64-boot-protocol; binutils-docs.

## 12. GIC and PL011 peripherals

- **RULE**: the ARM `virt` board provides a GIC (Generic Interrupt Controller,
  v2/v3/v4 selectable via `gic-version`) and one or two PL011 UARTs for the
  NonSecure world (plus Secure-world-only PL011 and PL061 when
  `-machine virt,secure=on`). Linux names the PL011 `ttyAMA0`. The MPS2 boards
  provide CMSDK UARTs and a different interrupt controller. Device addresses
  on `virt` (other than flash at 0x0 and RAM at 0x4000_0000) are only
  guaranteed via the generated DTB.
- **WHY AI GETS IT WRONG**: agents hard-code PL011/GIC register addresses from
  a tutorial instead of reading the DTB, or assume every ARM board has a PL011.
- **CORRECT REASONING**: bare-metal on `virt` should consume the DTB (boot ELF
  finds it at 0x4000_0000) to locate GIC and PL011; hard-coding addresses is
  only safe for flash (0x0) and RAM (0x4000_0000). On mps2-an385/an505 the
  UART is a CMSDK APB UART, not a PL011.
- **EXAMPLE** (bad): writing to PL011 at 0x9000000 on `mps2-an385` — no PL011
  there; output never appears.
- **COUNTEREXAMPLE** (good): bare-metal virt guest walks the DTB for the PL011
  node, or at minimum uses the CMSDK UART base on mps2 and
  `console=ttyAMA0` for a virt Linux kernel.
- **VERIFICATION**: on virt Linux, `dmesg | grep -i uart` shows
  `serial0: ttyAMA0 at MMIO 0x9000000`; the DTB node is
  `/pl011@9000000` (address may change between QEMU versions — read the DTB).
- **SOURCE**: qemu-docs (virt board devices, GIC, PL011); aarch64-boot-protocol (DTB passed in x0).

## 13. Snapshots and migration basics

- **RULE**: `-snapshot` makes every disk write transient (nothing persists on
  host exit). `-loadvm <file>` starts from a saved VM state (the `loadvm`
  monitor command). Incoming migration uses `-incoming tcp:[host]:port`
  (listen) with the `migrate` monitor command on the source; machine types are
  versioned precisely so live migration works across QEMU releases
  (`pc-q35-2.8`, `virt-5.0`), and migration is NOT guaranteed with `-cpu max`.
- **WHY AI GETS IT WRONG**: agents expect `-snapshot` to save state, or expect
  `migrate` between arbitrary machine types / QEMU versions to succeed.
- **CORRECT REASONING**: `-snapshot` discards writes; snapshots and migration
  need matching machine types, device configs, and (for `virt`) a fixed,
  non-`max` CPU. Use versioned machines (`-machine virt-5.0`) when the VM must
  migrate between QEMU versions.
- **EXAMPLE** (bad): `-machine virt -cpu max` and expecting migration to a
  different QEMU build to work — the docs say it is not guaranteed.
- **COUNTEREXAMPLE** (good): `-machine q35` plus `-incoming tcp:0:4444` on the
  target and the `migrate tcp:host:4444` monitor command on the source, with
  identical machine and device options on both ends.
- **VERIFICATION**: QEMU docs state that each release introduces versioned
  machine types "to allow live migration of guests from QEMU version X to Y";
  test with `-loadvm`/`savevm` on a disposable image.
- **SOURCE**: qemu-docs (man page `-snapshot`, `-loadvm`, `-incoming`, machine
  versioning; virt migration note for `-cpu max`).

## 14. Common mistakes (quick table)

| Symptom | Likely cause | Fix | Rule |
|---|---|---|---|
| "No machine specified" on ARM | `-machine` missing (no default) | add `-machine virt` (or mps2) | 2 |
| AArch64 Image does nothing on virt | default CPU is 32-bit cortex-a15 | `-cpu cortex-a53`/`max` | 3 |
| Headless boot, stdout empty | `-nographic` missing; serial in a window | add `-nographic` | 6 |
| Kernel boots, no messages | `console=` names the wrong UART | `ttyS0` (x86) / `ttyAMA0` (virt) | 9 |
| Bare-metal prints nothing | `-serial none`, or writing a PL011 address on an MPS2 board | drop `-serial none`; use the board's UART | 9, 12 |
| Guest has no NIC | `-netdev` without `-device` | add `-device e1000,netdev=net0` or use `-nic` | 8 |
| gdb attach finds guest already dead | `-gdb` without `-S` | add `-S` | 10 |
| Image dies at reset | ELF linked for another board/map | match `-machine` to the link map | 2, 5, 11 |

## Stability

Skill stability: `researched`. All option syntax above is verified against the
QEMU documentation (version 11.1.50 docs, 2026-08-14). No QEMU binary is
installed on this host (`qemu-system-x86_64 --version` and
`qemu-system-arm --version` → command not found), so no invocation was
actually run here; the documented target commands in `examples/good` are
recorded as `documented-as-target`, not host-verified.
