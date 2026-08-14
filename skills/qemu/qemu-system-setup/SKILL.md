---
name: qemu-system-setup
description: Use when setting up QEMU system emulation to boot a Linux kernel, firmware, or bare-metal ELF for x86-64, ARM Cortex-M, or AArch64 — machine model selection, -kernel/-nographic/-drive/netdev, serial console, and gdb remote debugging.
---

# QEMU System Setup: Machine, Boot, Serial, Debug

## When to use

- Writing a `qemu-system-*` command line to boot a Linux kernel, firmware, or a bare-metal ELF.
- A guest produces no serial output, hangs at reset, or dies immediately (boot debugging).
- Attaching gdb to a guest through QEMU's built-in gdb stub (`-gdb tcp::1234`).
- Choosing a machine model and CPU model for x86-64, ARM (virt, mps2-an385/an505), or AArch64.
- Wiring disks (`-drive`), slirp networking (`-netdev user`), or a serial console.

## When not to use

- Running a single user-space ELF with no devices or kernel — that is user-mode emulation (`qemu-aarch64 ./prog`), not `qemu-system-*`.
- Disk creation and conversion — that is `qemu-img`, not QEMU system options.
- Deep QMP/monitor automation, migration tuning, or `-blockdev` backend design — see the QEMU interop docs.
- Guest ISA semantics — pair with `asm-x86-64-registers-and-addressing` and the ARM architecture references.

## What the agent often gets wrong

- ARM system emulation has NO default machine: forgetting `-machine virt` fails with "No machine specified" before anything boots.
- `virt` defaults to the 32-bit `cortex-a15`: booting an AArch64 `Image` without `-cpu cortex-a53` (or `max`) fails.
- Forgetting `-nographic` and then reading "no output": the serial went to a windowed virtual console instead of stdout.
- `console=ttyS0` on ARM: the virt board's serial is a PL011, which Linux sees as `ttyAMA0`; `ttyS0` prints nothing.
- `-netdev user,id=net0` without a matching `-device ...netdev=net0`: the guest has no NIC.
- `-gdb tcp::1234` without `-S`: the guest boots and is long gone before gdb attaches.
- Treating `-kernel` as "load the file anywhere": on ARM a non-ELF `-kernel` file follows the Linux boot protocol (DTB in r2/x0); an ELF loads as bare-metal at its link addresses.
- "It compiled, so it will boot": a bare-metal ELF linked for the wrong memory map (e.g. AN385 vs AN505) silently dies at reset.

## How to reason correctly

1. Decide the guest kind first: kernel (Linux boot protocol), firmware (flash/BIOS), or bare-metal ELF. This picks the loading path and the serial device.
2. Pick the machine model before any other option: x86 → `pc`/`q35`; ARM → always `-machine` (`virt` for Linux, `mps2-an385`/`mps2-an505` for Cortex-M).
3. On `virt`, pick `-cpu` explicitly (AArch64 needs a 64-bit model; the default is 32-bit).
4. Close the serial loop: `-nographic` AND a matching `console=` (`ttyS0` on x86 COM1, `ttyAMA0` on virt PL011) AND no `-serial none`.
5. Debug with the gdb stub: `-S -gdb tcp::1234`, then `target remote :1234` in gdb.
6. Verify with `-machine help`, `-cpu help`, and the loaded file's ELF headers (`readelf`), not by guessing.

## What to verify

- `-machine help` lists the machine you named; `-cpu help` lists the CPU model.
- The image format matches the machine: bzImage for x86; `Image` (Linux protocol) or an ELF for ARM; `readelf -h` shows the expected architecture.
- Serial path is closed end-to-end: `-nographic` present AND `console=` matches the board UART AND no `-serial none`.
- A guest NIC exists: `-netdev user,id=net0` AND `-device <model>,netdev=net0`.
- `-gdb tcp::1234` is paired with `-S` when the guest must pause for attach.

## How to verify

```
qemu-system-x86_64  -machine help | grep q35
qemu-system-aarch64 -machine help | grep virt
qemu-system-aarch64 -cpu help | grep -E 'cortex-a53|max'

# boot a kernel to a serial banner and exit
qemu-system-x86_64 -machine q35 -cpu qemu64 -m 256M \
  -kernel bzImage -append "console=ttyS0" -nographic

# bare-metal ELF: check load addresses before booting
readelf -l app.elf          # PT_LOAD segments must match the board memory map
readelf -h app.elf          # Machine: ARM / AArch64

# gdb attach
qemu-system-aarch64 -machine virt -cpu cortex-a53 -m 512M \
  -kernel Image -append "console=ttyAMA0" -nographic -S -gdb tcp::1234 &
gdb -ex "target remote :1234" -ex "continue" vmlinux
```

## Where the knowledge comes from

- QEMU system-emulation docs and man page: `-machine`/`-cpu`/`-m`/`-kernel`/`-nographic`/`-drive`/`-netdev`/`-serial`/`-gdb`, ARM targets (`virt`, `mps2-an385`, `mps2-an505`)
- Linux kernel AArch64 booting protocol (Image loading, DTB in x0)
- GNU binutils docs (`readelf`/`objdump` for ELF verification)
- GDB manual (remote attach via the gdb stub)

## Related skills

- `embedded-mpu-trustzone` — ARMv7-M/ARMv8-M MPU and TrustZone on mps2-an385/mps2-an505 targets (require of)
- `asm-x86-64-registers-and-addressing` — what the x86-64 guest sees
- `elf-linker-loader-debugger` — ELF loading; `readelf`/`objdump` verification (require of)
- `asm-calling-conventions` — AAPCS64 for AArch64 bare-metal entry

## Evaluation

- Synthetic: generate the correct command line for (a) x86-64 Linux on q35, (b) Cortex-M bare-metal on mps2-an385, (c) AArch64 Linux on virt; each must include machine, cpu (where required), `-nographic`, correct `console=`.
- False-positive: a correct invocation must NOT be flagged for a missing `-nographic` on mps2 (no display hardware) or for a second `-serial` used for the secure-world UART on `virt,secure=on`.
- Adversarial: a "no output" symptom must be fixed by serial routing (`console=`/`-nographic`), not by adding memory; an AArch64 boot failure must be fixed with `-cpu`, not by changing the machine.
- Historical: replicate the QEMU docs examples in `references/qemu-setup.md`; score detection of the missing-`-M`, missing-`-cpu`, and missing-`console=` classes.
