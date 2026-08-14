# GNU ld Linker Scripts for Bare-Metal Embedded — Reference

Rules for writing and debugging `MEMORY`/`SECTIONS` linker scripts for
flash/RAM MCUs. Each rule: RULE / WHY AI GETS IT WRONG / CORRECT REASONING /
EXAMPLE (bad) / COUNTEREXAMPLE (good) / VERIFICATION / SOURCE.

Registry sources: `gnu-ld-manual`, `binutils-docs`, `sysv-elf`, `cmsis`.

## 1. MEMORY command — named regions with hard bounds

- **RULE**: `MEMORY { NAME (attrs) : ORIGIN = addr, LENGTH = size; ... }`
  defines named address regions. Output sections bind to a region with `>NAME`.
  A section larger than the region fails the link with
  `region 'NAME' overflowed by N bytes` (`gnu-ld-manual`, MEMORY command).
- **WHY AI GETS IT WRONG**: treats region names as decoration, invents
  ORIGIN/LENGTH without checking the MCU memory map, or forgets that the
  overflow check is the linker's only guard that code/data fit the part.
- **CORRECT REASONING**: ORIGIN/LENGTH come from the datasheet (e.g. STM32F4:
  FLASH 0x08000000/1M, RAM 0x20000000/128K). Attrs matter (rx for flash, rwx
  for RAM); sections that do not fit any region fall to orphan placement and
  silently escape your map.
- **EXAMPLE (bad)**: `RAM : ORIGIN = 0x20000000, LENGTH = 0x10000` on a chip
  with 0x8000 of RAM — a large `.data+.bss+stack` overflows at link time.
- **COUNTEREXAMPLE (good)**: correct LENGTH from the part; `.text/.rodata`
  `>FLASH`, `.data/.bss` `>RAM`; map file confirms every section is inside a
  named region.
- **VERIFICATION**: link with a too-small RAM region and read
  `region 'RAM' overflowed` (VERIFIED on host GNU ld 2.46).
- **SOURCE**: `gnu-ld-manual` (MEMORY command, overflow diagnostic).

## 2. SECTIONS command — script order is memory order

- **RULE**: `SECTIONS { .out : { *(.in1) *(.in2) ... } }` collects input
  sections into output sections; the location counter `.` advances by each
  output section's size, so SECTIONS order determines memory order. Input
  sections not named become orphans placed heuristically (`gnu-ld-manual`,
  SECTIONS command).
- **WHY AI GETS IT WRONG**: assumes C-file or object-file order is preserved;
  writes `*(.text)` and misses `.text.startup`/`.text.*`; or drops a needed
  section and never checks the map file.
- **CORRECT REASONING**: write the script as a placement program. Use
  `*(.text*)`, `*(.rodata*)`, `*(.data*)`, `*(.bss*)` to catch compiler
  variants, and always inspect `ld -M` output for orphans.
- **EXAMPLE (bad)**: `*(.text)` only — `startup.c`'s `.text.startup` (or the
  vector table placed by attribute) lands in an orphan section at an arbitrary
  address.
- **COUNTEREXAMPLE (good)**: `*(.text .text*)`, vector table first, explicit
  placement for every region; map file shows a single contiguous FLASH image.
- **VERIFICATION**: `objdump -h` / `ld -M`; the map file lists orphans as
  placed outside the named sections (VERIFIED on host).
- **SOURCE**: `gnu-ld-manual` (SECTIONS, orphan sections).

## 3. VMA vs LMA — `AT>` and LOADADDR make the `.data` copy real

- **RULE**: every output section has a VMA (runtime address) and an LMA (load
  address). `.data : AT> FLASH { ... } > RAM` gives VMA in RAM, LMA in FLASH.
  `__etext = LOADADDR(.data)` records the flash address of the `.data` payload
  (`gnu-ld-manual`, AT / LOADADDR).
- **WHY AI GETS IT WRONG**: assumes the file is loaded to the VMA; on bare
  metal only flash exists at reset, so `.data` initial values must be in flash
  (LMA) and copied to RAM (VMA) by startup code. Omitting `AT>` silently makes
  LMA == VMA, so the payload never lives in flash.
- **CORRECT REASONING**: at reset SRAM content is undefined. The binary stores
  `.data` bytes in flash at the LMA; startup copies
  `__etext` → `__data_start` for `__data_end - __data_start` bytes.
- **EXAMPLE (bad)**: `.data : { *(.data*) } > RAM` (no `AT> FLASH`) — the
  payload sits at the RAM address in the file; flash has nothing; after reset
  RAM is stale garbage.
- **COUNTEREXAMPLE (good)**: `.data : AT> FLASH { __data_start = .;
  *(.data*); __data_end = .; } > RAM` plus `__etext = LOADADDR(.data)` and a
  matching startup copy loop.
- **VERIFICATION**: `objdump -h` on an ELF/Arm image shows two addresses for
  `.data` (VMA in RAM, LMA in FLASH) — documented-as-target. On this PE/COFF
  host, `objdump -h` shows LMA == VMA for every section regardless of `AT>`
  (VERIFIED PE format property), and `nm` still shows `__etext` defined
  (VERIFIED). The distinct-address behavior belongs to the ELF/Arm target.
- **SOURCE**: `gnu-ld-manual` (AT, LOADADDR); `sysv-elf` (p_vaddr/p_offset
  split that LMA models).

## 4. KEEP() — the vector table is invisible to garbage collection

- **RULE**: `KEEP(*(.isr_vector))` marks the vector table as a GC root.
  `--gc-sections` keeps only sections reachable from the entry point and other
  roots; the vector table is referenced only by the CPU's reset fetch, which
  the linker cannot see (`gnu-ld-manual`, KEEP; `binutils-docs`, ld options).
- **WHY AI GETS IT WRONG**: thinks "it is first in the script" or "it is
  referenced by startup" is enough; nothing in the object graph references the
  table, so GC discards it.
- **CORRECT REASONING**: only symbols reachable from the entry (plus KEEP /
  `EXTERN`) survive GC. Vector entries are data words, not calls. If you use
  `--gc-sections`, every hardware-referenced table must be KEEP'd.
- **EXAMPLE (bad)**: `.text : { *(.isr_vector) *(.text*) } > FLASH` linked
  with `--gc-sections` — `nm` shows the table is gone; the device boots into
  undefined flash.
- **COUNTEREXAMPLE (good)**: `KEEP(*(.isr_vector))`; `nm` still lists the
  vector symbols after `--gc-sections` (VERIFIED on host).
- **VERIFICATION**: link the same object with and without KEEP under
  `-Wl,--gc-sections`; compare `nm` (VERIFIED on host).
- **SOURCE**: `gnu-ld-manual` (KEEP, --gc-sections); `binutils-docs` (nm).

## 5. Startup contract symbols — `__etext`/`__data_start`/`__bss_start`

- **RULE**: the script defines `__etext` (LMA of `.data`), `__data_start`,
  `__data_end`, `__bss_start`, `__bss_end`; startup code copies and zeroes the
  ranges. Names are a free convention but MUST match the startup file exactly
  (`cmsis` startup files; GNU ARM toolchain sample scripts).
- **WHY AI GETS IT WRONG**: invents `_sdata`/`_edata`/`_sbss` while the
  startup file uses `__data_start`/`__bss_start` (or the reverse), producing an
  undefined-reference link error — or defines the symbols in C so they get
  optimized away.
- **CORRECT REASONING**: the symbol is a plain address constant the linker
  resolves. Verify the actual startup file's names before choosing them; keep
  the four/six names in one place.
- **EXAMPLE (bad)**: script defines `__data_start`/`__bss_start` but startup.c
  references `_sdata` — `undefined reference to '_sdata'` at link
  (VERIFIED on host by reproducing the mismatch).
- **COUNTEREXAMPLE (good)**: script and startup.c both use
  `__etext/__data_start/__data_end/__bss_start/__bss_end`; link succeeds and
  `nm` shows all five.
- **VERIFICATION**: `nm exe | grep __` shows both ends of each range; a
  deliberate name mismatch fails the link (VERIFIED on host).
- **SOURCE**: `cmsis` (startup conventions); `gnu-ld-manual` (symbol
  assignment).

## 6. ALIGN() and the location counter `.`

- **RULE**: `. = ALIGN(n)` rounds the location counter up to the next multiple
  of n; ALIGN inside a section advances `.` before a symbol. Section starts are
  auto-aligned to member alignment, but the END of a stage can be unaligned, so
  `__bss_start`/heap/stack bases need explicit `. = ALIGN(4)` (or 8)
  (`gnu-ld-manual`, ALIGN, dot).
- **WHY AI GETS IT WRONG**: believes the linker guarantees 4-alignment of
  every symbol; a 2-byte `.data` payload leaves `__bss_start` on a mod-2
  address.
- **CORRECT REASONING**: `.` is a counter; ALIGN is the instruction that snaps
  it to a boundary. Align before `__data_start`, before `__bss_start`, and
  before heap/stack symbols.
- **EXAMPLE (bad)**: no `. = ALIGN(4)` before `.bss` — `nm` shows
  `__bss_start` at an address with `value % 4 != 0`.
- **COUNTEREXAMPLE (good)**: `. = ALIGN(4);` between stages; `nm` shows every
  start symbol `% 4 == 0` (VERIFIED on host).
- **VERIFICATION**: `nm` the symbol addresses and check `mod 4` (VERIFIED on
  host).
- **SOURCE**: `gnu-ld-manual` (ALIGN, location counter).

## 7. Startup requirements — vector table at 0, `_estack`, entry

- **RULE**: on Cortex-M the CPU reads the initial SP from flash address
  `ORIGIN(FLASH) + 0` and the reset vector from `+ 4` (or from the VTOR-specified
  table). The script must place `.isr_vector` FIRST in FLASH, KEEP'd, and define
  `_estack` (or `__stack_top`) at `ORIGIN(RAM) + LENGTH(RAM)`; the entry should
  be `Reset_Handler`, not `main` (`cmsis` device startup; `gnu-ld-manual`,
  ENTRY).
- **WHY AI GETS IT WRONG**: places vectors in RAM or after `.text`, sets
  `ENTRY(main)`, or forgets `_estack`, so the CPU faults at the first word.
- **CORRECT REASONING**: address 0 must contain the stack pointer, address 4
  the reset handler; startup then sets up `.data`/`.bss` and calls `main`.
- **EXAMPLE (bad)**: `.isr_vector > RAM` — at reset the CPU reads SP/PC from
  flash offset 0, which holds code bytes, and immediately faults.
- **COUNTEREXAMPLE (good)**: KEEP'd `.isr_vector` first in FLASH,
  `_estack = ORIGIN(RAM) + LENGTH(RAM)`, `ENTRY(Reset_Handler)`.
- **VERIFICATION**: `objdump -h`/`nm` show the vectors at FLASH ORIGIN (layout
  VERIFIED on host); the boot behavior is target-only (documented-as-target).
- **SOURCE**: `cmsis` (vector table, reset); `gnu-ld-manual` (ENTRY, MEMORY).

## 8. Common-mistake checklist

| Mistake | Symptom | Where caught |
|---|---|---|
| Forgot KEEP on vector table | `.isr_vector` absent after `--gc-sections`; no reset vector | `nm`, target boot |
| Vector table in wrong region | vectors not at FLASH ORIGIN (e.g. in RAM) | `objdump -h` |
| No `AT> FLASH` on `.data` | flash has no `.data` payload; startup copy reads stale flash | `objdump -h` on target image |
| Startup/script symbol mismatch | `undefined reference to '__etext'` | link |
| No copy/bss-zero in startup | `.data` stale, `.bss` garbage on target | target run |
| Missing `. = ALIGN(4)` | `__bss_start`/heap unaligned | `nm`, target fault |
| Region too small | `region 'RAM' overflowed by N bytes` | link |
| `ENTRY(main)` | reset vector points at CRT/`main`, not `Reset_Handler` | `nm`, target |
| Orphan sections | sections outside your map | `ld -M` map file |
