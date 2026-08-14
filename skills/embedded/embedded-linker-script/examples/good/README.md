# examples/good — correct FLASH/RAM linker scripts with startup

## Files

- `cortex_m7.ld` — a complete production-style script for a Cortex-M7 class
  part: MEMORY (FLASH/RAM), `_estack`, KEEP'd vector table, `.text`/`.rodata`
  in FLASH, `.data` with `AT> FLASH` + `__etext = LOADADDR(.data)`, `.bss`,
  ALIGN'd heap. Entry `Reset_Handler`.
- `startup.c` — vector table + `Reset_Handler` with the `.data` copy loop and
  the `.bss` zero loop, exactly matching the script symbols.
- `main.c` — the target application.
- `host_demo.ld` — the same structure in a form verifiable on this repository's
  host (GNU ld 2.46, PE/COFF). ARM-style addresses are kept so it doubles as a
  target script; see VERIFIED notes below.
- `host_demo.c` — references every contract symbol so the linker must resolve
  them; a 2-byte `.data` payload and char-only `.bss` make ALIGN effects
  visible.

## Target build (documented-as-target; needs arm-none-eabi-gcc)

```
arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -nostdlib -T cortex_m7.ld \
    startup.c main.c -o firmware.elf
arm-none-eabi-nm firmware.elf | grep -E '__etext|__data_start|__bss_start|_estack'
arm-none-eabi-objdump -h firmware.elf     # .data has VMA in RAM, LMA in FLASH
```

## Host verification (VERIFIED with GNU ld 2.46 on this host)

```
gcc -nostdlib -O2 -c host_demo.c -o host_demo.o
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,host_demo.ld \
    -Wl,--gc-sections -o good.exe host_demo.o        # link exit 0
nm good.exe | grep -E '__etext|__data_start|__data_end|__bss_start|__bss_end|__heap_start'
objdump -h good.exe    # .isr_vector + .text in FLASH (0x08000000), .data/.bss in RAM (0x20000000)
```

Verified on this host: link acceptance, all contract symbols defined,
`.isr_vector` retained under `--gc-sections` (KEEP), sections placed in the
declared regions. Not verifiable on PE/COFF: the VMA/LMA split of `.data`
(PE output shows LMA == VMA; `__etext`/`LOADADDR` semantics are recorded from
the ELF/ARM target, documented-as-target), the reset-time copy behavior, and
vector-table boot behavior.
