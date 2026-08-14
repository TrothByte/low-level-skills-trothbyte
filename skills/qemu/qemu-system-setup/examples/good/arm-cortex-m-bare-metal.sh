#!/bin/sh
# GOOD: ARM Cortex-M3 bare-metal ELF on the MPS2 AN385 board.
# Teaching points:
#  - ARM system emulation needs an explicit -machine; no default exists.
#  - mps2-an385 = Cortex-M3 (ARMv7-M). The CPU is fixed by the board.
#  - app.elf must be linked for the AN385 map (ZBT SSRAM at 0, 16K remap);
#    QEMU loads the ELF PT_LOAD segments at their link addresses.
#  - UART0 (CMSDK APB UART) is muxed onto the console by -nographic.
#  - No -m is needed: the board model defines the SRAM/flash sizes.
# Documented-as-target: QEMU is not installed on this repo's host (2026-08-14).
qemu-system-arm \
  -machine mps2-an385 \
  -nographic \
  -kernel app.elf

# Verify before booting (binutils):
#   arm-none-eabi-readelf -h app.elf   # Machine: ARM
#   arm-none-eabi-readelf -l app.elf   # PT_LOAD p_paddr inside AN385 map
