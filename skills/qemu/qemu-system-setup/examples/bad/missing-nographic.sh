#!/bin/sh
# BAD: missing -nographic.
# Teaching points:
#  - Without -nographic, QEMU opens a graphical window and the emulated
#    serial port goes to a windowed virtual console (`vc`), not stdout.
#  - On a headless/CI host the command appears to "produce no output",
#    which is a serial-routing problem, NOT a boot or memory problem.
#  - -display none alone is NOT a fix: it hides video only and leaves
#    serial on the virtual console.
qemu-system-x86_64 \
  -machine q35 \
  -cpu qemu64 \
  -m 256M \
  -kernel bzImage \
  -append "console=ttyS0"

# GOOD: add -nographic so the serial console lands on stdout:
#   qemu-system-x86_64 -machine q35 -cpu qemu64 -m 256M \
#     -kernel bzImage -append "console=ttyS0" -nographic
