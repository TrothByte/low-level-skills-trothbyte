#!/bin/sh
# BAD: serial misconfiguration.
# Teaching points:
#  - Case 1: the kernel command line names a console driver that does not
#    exist on this machine. virt has a PL011 (ttyAMA0), not a 8250 (ttyS0),
#    so "console=ttyS0" yields NO kernel output even though boot succeeds.
#  - Case 2: -serial none removes every emulated UART; a bare-metal print
#    loop that writes to the CMSDK UART is silently lost.
qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a53 \
  -m 512M \
  -kernel Image \
  -append "console=ttyS0" \
  -nographic

qemu-system-arm \
  -machine mps2-an385 \
  -nographic \
  -serial none \
  -kernel app.elf

# GOOD:
#   qemu-system-aarch64 -machine virt -cpu cortex-a53 -m 512M \
#     -kernel Image -append "console=ttyAMA0" -nographic
#   qemu-system-arm -machine mps2-an385 -nographic -kernel app.elf
