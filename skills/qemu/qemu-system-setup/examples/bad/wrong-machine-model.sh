#!/bin/sh
# BAD: wrong machine model.
# Teaching points:
#  - ARM system emulation has NO default machine; the first command fails
#    with "No machine specified, and there is no default" before any boot.
#  - The second command boots the AArch64 Image on the DEFAULT 32-bit
#    cortex-a15 CPU: the guest cannot fetch AArch64 instructions and the
#    serial stays silent. This is a wrong-CPU-for-the-machine failure.
qemu-system-arm -nographic -kernel app.elf

qemu-system-aarch64 \
  -machine virt \
  -m 512M \
  -kernel Image \
  -append "console=ttyAMA0" \
  -nographic

# GOOD (see examples/good):
#   qemu-system-arm -machine mps2-an385 -nographic -kernel app.elf
#   qemu-system-aarch64 -machine virt -cpu cortex-a53 -m 512M \
#     -kernel Image -append "console=ttyAMA0" -nographic
