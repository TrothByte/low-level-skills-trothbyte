#!/bin/sh
# GOOD: AArch64 (64-bit ARM) Linux kernel on the `virt` generic platform.
# Teaching points:
#  - -cpu is REQUIRED for AArch64: the virt default is 32-bit cortex-a15.
#  - console=ttyAMA0 matches the virt board's PL011 UART.
#  - virt supports PCI + virtio; -device virtio-net-device uses virtio-mmio.
#  - qemu-system-aarch64 also runs 32-bit ARM machines, so the 64-bit
#    CPU choice is explicit here, not implied by the binary name.
# Documented-as-target: QEMU is not installed on this repo's host (2026-08-14).
qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a53 \
  -m 512M \
  -kernel Image \
  -append "console=ttyAMA0" \
  -netdev user,id=net0 \
  -device virtio-net-device,netdev=net0 \
  -nographic

# Expected on stdout: AArch64 Linux banner, "console [ttyAMA0] enabled",
# "serial0: ttyAMA0 at MMIO 0x9000000 (irq = 33)".
