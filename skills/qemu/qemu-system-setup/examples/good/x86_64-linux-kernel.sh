#!/bin/sh
# GOOD: x86-64 Linux kernel on the q35 machine.
# Teaching points:
#  - q35 is the modern PCIe PC chipset; the default `pc` (i440fx) also works.
#  - console=ttyS0 matches the emulated 16550 COM1 (Linux 8250 driver).
#  - -netdev user creates the host backend; -device e1000 wires the guest NIC.
#  - -nographic sends the serial console to stdout (headless-safe).
# Documented-as-target: QEMU is not installed on this repo's host (2026-08-14).
qemu-system-x86_64 \
  -machine q35 \
  -cpu qemu64 \
  -m 256M \
  -kernel bzImage \
  -append "console=ttyS0" \
  -initrd initrd.img \
  -netdev user,id=net0 \
  -device e1000,netdev=net0 \
  -nographic

# Expected on stdout: Linux banner, "console [ttyS0] enabled",
# guest DHCP from 10.0.2.0/24 slirp network.
