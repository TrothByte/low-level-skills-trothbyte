---
name: embedded-linker-script
description: Use when writing, reviewing, or debugging a bare-metal embedded GNU ld linker script — MEMORY regions, SECTIONS placement, FLASH/RAM mapping, KEEP() on the vector table, startup copy loops with __etext/__data_start/__bss_start, ALIGN(), the location counter, and why firmware fails to boot or starts with stale .data.
---

# Embedded Linker Scripts (GNU ld) for Bare-Metal Firmware

## When to use

- Writing or reviewing a bare-metal `.ld` script (MEMORY/SECTIONS) for a Cortex-M
  or similar flash/RAM MCU.
- Debugging "program does nothing at reset", "hard fault immediately", or
  "global variables have the wrong values at startup".
- Adding a custom section (`.noinit`, a second flash bank), moving code or data
  between regions, or tuning stack/heap placement.
- Explaining why `__etext`, `__data_start`, `__bss_start` exist and what startup
  code does with them.
- Checking why `--gc-sections` removed the vector table or why the linker reports
  `region 'RAM' overflowed`.

## When not to use

- Host or OS linking (ELF executables, shared objects, DLLs) — use
  `elf-layout-and-relocations` / `elf-linker-loader-debugger`.
- Dynamic linking, GOT/PLT, PIE, symbol interposition — not bare-metal concerns.
- Startup code that does not interact with script-defined symbols (pure RTOS
  init) — use `rtos-concurrency-and-isr` where relevant.
- A linker script you do not own (GCC default, vendor BSP) unless you must change
  it; read it with `ld -M` first.

## What the agent often gets wrong

- "The linker places sections in the order they appear in the C file." No — the
  SECTIONS command order defines memory order.
- "`ENTRY(main)`." On bare metal the entry is `Reset_Handler` (or the vector
  table's second word); `main` is reached from C startup code.
- "The vector table is kept because it sits at address 0." Under
  `--gc-sections` it is removed unless wrapped in `KEEP()`.
- "`.data` is copied to RAM automatically." The linker only emits file bytes at
  the LMA; the STARTUP code performs the copy. The script just records the
  contract (`AT>` + `__etext`).
- "Symbol names are my choice." They must match the startup file exactly; a
  mismatch is an undefined-reference link error or a silent wrong copy.
- "`ALIGN()` is optional." Without it `.bss`/heap/stack bases can be
  misaligned, which faults or slows unaligned word accesses on ARM.
- "Region overflow is a compile error." It is a linker error
  (`region 'RAM' overflowed by N bytes`).
- "RAM is zero at reset." It is undefined; `.bss` must be zeroed and `.data`
  copied by startup code.

## How to reason correctly

1. Draw the MCU memory map from the datasheet: FLASH base/length and RAM
   base/length. These are the ORIGIN/LENGTH of the MEMORY regions.
2. Decide placement: vectors, code, and rodata live in FLASH (execute in place);
   `.data`, `.bss`, heap, stack live in RAM.
3. Split VMA from LMA for `.data`: `.data : AT> FLASH { ... } > RAM`, and set
   `__etext = LOADADDR(.data)`.
4. Define the startup contract symbols (`__data_start`, `__data_end`,
   `__bss_start`, `__bss_end`) and make startup reference EXACTLY those names.
5. Place `.isr_vector` FIRST in FLASH inside `KEEP()`; define `_estack` at
   `ORIGIN(RAM) + LENGTH(RAM)`.
6. Insert explicit `. = ALIGN(4)` between the data stages so `.bss`, heap, and
   stack bases stay aligned.
7. Verify with a map file and `nm`/`objdump` before running on hardware.

## What to verify

- Every output section lands in the intended region (map file, `ld -M`).
- `.data` has distinct VMA and LMA (`objdump -h` shows both) and
  `__etext == LOADADDR(.data)`.
- `__etext`, `__data_start`, `__data_end`, `__bss_start`, `__bss_end` exist in
  the linked image (`nm`) with aligned (mod 4 == 0) values.
- `.isr_vector` survives `--gc-sections` (KEEP works) and sits at FLASH ORIGIN.
- Link succeeds with no `region '...' overflowed`.
- No `undefined reference to '__etext'` when linking the startup object.

## How to verify

Host (this repo: MinGW gcc 16.1 / GNU ld 2.46, PE/COFF). The linker is GNU ld,
so MEMORY/SECTIONS/KEEP/ALIGN/symbol behavior is directly testable:

```
gcc -Wall -Wextra -Werror -O2 -c examples/good/host_demo.c -o host_demo.o
gcc -Wl,-T,examples/good/host_demo.ld -Wl,--gc-sections -o good.exe host_demo.o
./good.exe                  # prints symbol addresses; exit 0 (VERIFIED)
nm good.exe | grep -E '__etext|__data_start|__data_end|__bss_start|__bss_end'
objdump -h good.exe         # .data shows VMA and LMA columns
```

ARM target (documented-as-target; needs arm-none-eabi-gcc):

```
arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -nostdlib \
    -T examples/good/cortex_m7.ld examples/good/startup.c examples/good/main.c \
    -o firmware.elf
arm-none-eabi-objdump -h firmware.elf
arm-none-eabi-nm firmware.elf
```

## Where the knowledge comes from

- GNU ld manual — Linker Scripts: MEMORY, SECTIONS, AT/LOADADDR, KEEP, ALIGN,
  the location counter `.`.
- GNU binutils documentation — ld, objdump, nm, `--gc-sections`.
- System V ABI — ELF section/symbol semantics underlying VMA/LMA.
- Arm CMSIS — startup files, vector table layout, reset conventions.

## Related skills

- `elf-layout-and-relocations` — sections/symbols/relocations the linker
  consumes (require of).
- `elf-linker-loader-debugger` — linker/loader mechanics, `ld -M` map reading.
- `embedded-mpu-trustzone` — memory regions/attributes on Cortex-M.
- `embedded-volatile-and-memory-ordering` — MMIO and ISR-flag rules once
  firmware runs.

## Evaluation

Synthetic: given a MEMORY+SECTIONS script, predict section placement, the
defined symbols, the `.data` copy range (`__etext`..`__data_end`), and the
`region overflowed` failure for a too-small RAM. Adversarial: a script that
links cleanly but boots into garbage (vector table in RAM, no copy loop, no
ALIGN) — find the silent boot/runtime bug, not only link errors.
False-positive: a correct KEEP'd vector table, aligned `.data`/`.bss` with
matching startup symbols, and a correct flash-copy loop must NOT be flagged.
See `evals/README.md`.
