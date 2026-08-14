# Evaluation — qemu-system-setup

Skill: `skills/qemu/qemu-system-setup`. Stability: `researched`.

QEMU is not installed on this host (2026-08-14): `qemu-system-x86_64 --version`,
`qemu-system-arm --version`, and `qemu-system-aarch64 --version` all fail with
"command not found". All command-line syntax was therefore verified against the
QEMU documentation (qemu.org docs, version 11.1.50, man page
`system/qemu-manpage.html`, `target-arm.html`, `arm/virt.html`, `arm/mps2.html`)
and the `examples/` invocations are recorded as **documented-as-target**, not
host-verified.

## Verified facts (this environment, 2026-08-14)

- No QEMU binary on PATH (system mode for x86-64, arm, or aarch64). Result:
  the actual-run verification described in the task is deferred to a host that
  has QEMU; everything here is documented-as-target.
- Verified against the QEMU docs (KNOWN, from `qemu-docs`):
  - `-machine`/`-M` select the machine; ARM has no default machine;
    x86 has `pc` (default) and `q35` (`pc-q35-<ver>` versioned types).
  - `-cpu model` with `-cpu help`; the ARM `virt` default CPU is 32-bit
    `cortex-a15`, so an AArch64 guest needs e.g. `-cpu cortex-a53` or `max`.
  - `-m` default 128 MiB, "M"/"G" suffixes, hotplug form
    `-m 1G,slots=3,maxmem=4G`.
  - `-kernel bzImage`, `-append cmdline`, `-initrd file`; ARM `virt` DTB
    handling: Linux-protocol boot passes DTB in r2 (32-bit) / x0 (64-bit),
    ELF (bare-metal) boot finds the DTB at the start of RAM (0x4000_0000);
    flash at 0x0, RAM at 0x4000_0000.
  - `-nographic` disables graphics and redirects serial to the console muxed
    with the monitor; `-display none` only hides video.
  - `-drive file=,format=,if=` (if types: ide, scsi, sd, mtd, floppy, pflash,
    virtio, none); `-pflash` needs a flash-sized file on non-x86; `-snapshot`
    makes writes transient.
  - `-netdev user,id=` needs a `-device ...netdev=` partner (or `-nic`);
    slirp default net 10.0.2.0/24, host 10.0.2.2, DNS 10.0.2.3,
    DHCP pool 10.0.2.15-31.
  - `-serial dev`, default `vc` in graphical / `stdio` in non-graphical mode;
    `-serial none` suppresses UARTs; `-serial mon:stdio` muxes monitor.
  - `-gdb tcp::1234` (and `-s` shorthand) does NOT pause; `-S` pauses at
    startup. `-loadvm file`; `-incoming tcp:[host]:port` for migration.
  - `mps2-an385` = Cortex-M3 (ARMv7-M), `mps2-an505` = Cortex-M33 (ARMv8-M
    TrustZone); board CPU is fixed. `virt` supports GIC (v2/v3/v4) and
    PL011 UARTs (one or two NonSecure; more with `secure=on`).
- KNOWN (kernel serial naming): x86 16550 COM1 → `ttyS0`; ARM PL011 → `ttyAMA0`
  (amba-pl011 driver).
- KNOWN (from `aarch64-boot-protocol`): AArch64 `Image` boot follows the Linux
  booting protocol (DTB address passed in x0).
- KNOWN (from `binutils-docs`): `readelf -h`/`readelf -l` verifies ELF machine
  type and PT_LOAD load addresses.

## Synthetic evals

Each case: construct the command line (DETECT missing/broken piece → EXPLAIN
the rule → FIX → VERIFY by command).

| Level | Task | Defect to detect | Rule |
|---|---|---|---|
| easy | Boot x86-64 bzImage headless | missing `-nographic` | 6 |
| easy | Boot a Linux kernel on ARM | missing `-machine` (no default) | 2 |
| medium | Boot AArch64 `Image` on virt | missing `-cpu` (default is 32-bit) | 3 |
| medium | Kernel boots but prints nothing on virt | `console=ttyS0` instead of `ttyAMA0` | 9 |
| medium | Guest has no network interface | `-netdev` without `-device` | 8 |
| hard | Attach gdb, breakpoint never hits | `-gdb` without `-S` | 10 |
| hard | Bare-metal ELF dies at reset | ELF linked for the wrong board map | 2, 5, 11 |
| hard | `-bios Image` on ARM virt | firmware vs kernel path confusion | 11 |
| adversarial | "no output" reported; fix must be serial routing, not `-m 4G` | wrong hypothesis | 4, 6, 9 |
| adversarial | AArch64 failure; fix must be `-cpu`, not a different machine | wrong machine swap | 3 |

## False-positive evals (correct commands must NOT be flagged)

- `qemu-system-arm -machine mps2-an385 -nographic -kernel app.elf` — must NOT
  be flagged for a missing `-nographic` (it is present) nor for a missing
  `-cpu` (the board fixes the CPU).
- `qemu-system-aarch64 -machine virt,secure=on -cpu max ... -serial mon:stdio`
  with a second `-serial` for the secure-world PL011 — a second `-serial` is a
  valid console backend, not a misconfiguration.
- `qemu-system-x86_64 -machine q35 -m 1G -kernel bzImage -append "console=ttyS0" -nographic`
  without networking — a guest is allowed to have no NIC.
- A bare-metal ELF linked for the AN385 map booted with `-machine mps2-an385`
  must NOT be flagged for "missing console=" — bare-metal has no `console=`.
- An ELF boot on `virt` (DTB found at start of RAM) must NOT be flagged for
  "no DTB in x0" — that rule applies only to Linux-protocol boots.

## Verification commands (documented-as-target; run on a QEMU host)

```
# host environment check (this repo: both absent -> stability 'researched')
qemu-system-x86_64 --version
qemu-system-arm --version

# option-level sanity (no image needed)
qemu-system-x86_64  -machine help | grep q35
qemu-system-aarch64 -machine help | grep -E 'virt|mps2'
qemu-system-aarch64 -cpu help | grep -E 'cortex-a53|max'

# kernel-less serial test (expected: QEMU monitor prompt / no crash)
qemu-system-x86_64 -machine q35 -m 256M -nographic

# x86-64 Linux (expected: banner + "console [ttyS0] enabled")
qemu-system-x86_64 -machine q35 -cpu qemu64 -m 256M \
  -kernel bzImage -append "console=ttyS0" -nographic

# AArch64 Linux (expected: banner + "console [ttyAMA0] enabled")
qemu-system-aarch64 -machine virt -cpu cortex-a53 -m 512M \
  -kernel Image -append "console=ttyAMA0" -nographic

# bare-metal Cortex-M (expected: UART0 text from app.elf)
qemu-system-arm -machine mps2-an385 -nographic -kernel app.elf

# gdb stub (expected: gdb connects, guest paused at reset)
qemu-system-x86_64 -machine q35 -m 256M -kernel bzImage \
  -append "console=ttyS0" -nographic -S -gdb tcp::1234 &
gdb -ex "target remote :1234" -ex "info registers" vmlinux

# ELF verification (binutils)
readelf -h app.elf        # Machine must match the -machine architecture
readelf -l app.elf        # PT_LOAD p_paddr must lie inside the board map
```

## Scoring (for routing eval)

- precision: every flagged defect maps to a reference rule (1-14).
- recall: the easy/medium/hard fixtures above must all be detected.
- FP-rate: the false-positive fixtures must produce zero flags.
- routing: trigger must fire on "QEMU", "qemu-system", "boot kernel in QEMU",
  "-nographic", "virt", "mps2", "PL011", "-netdev"; must NOT fire on
  "qemu-img", user-mode `qemu-aarch64` runs, or pure ARM register semantics.
