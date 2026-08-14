# Evaluation — embedded-linker-script

Skill: `skills/embedded/embedded-linker-script`. Stability target: `evaluated`.

## Synthetic evals

- **easy/positive**: given a `MEMORY` block (FLASH ORIGIN/LENGTH, RAM
  ORIGIN/LENGTH), state where `.text`, `.data`, `.bss` land and why; name the
  symbols the startup copy loop needs.
- **easy/negative**: given `.data : { *(.data*) } > RAM` with no `AT>`, state
  that the payload never lives in flash and the copy loop has nothing to copy.
- **medium/positive**: compute the copy range
  `__data_end - __data_start` and the bss-zero range
  `__bss_end - __bss_start` from a symbol dump.
- **medium/negative**: predict that a too-small `RAM LENGTH` fails the link
  with `region 'RAM' overflowed by N bytes`; predict that a script without
  KEEP loses `.isr_vector` under `--gc-sections`.
- **hard/negative**: a script that links cleanly but boots into garbage —
  vector table in RAM region, or missing `AT>`, or no `. = ALIGN(4)` — find the
  bug from `objdump -h`/`nm` evidence, not from a runtime crash you cannot see.
- **adversarial**: given only `nm` output of a firmware image, determine
  whether `.data` will be initialized at reset (are `__etext`/`__data_start`
  defined and distinct?) and whether the vector table survived GC (are vector
  symbols present and at FLASH base?).

## False-positive evals (correct code must NOT be flagged)

- A correct KEEP'd vector table first in FLASH must NOT be flagged as
  "unreachable" or "unused".
- Aligned `.data`/`.bss` with matching startup symbols and a correct flash-copy
  loop must NOT be flagged.
- `__etext`/`__data_start`/`__bss_start` present and 4-aligned is correct; do
  NOT invent an alignment bug.
- A script without `AT>` is only wrong for copyable initialized data; a pure
  XIP configuration (no `.data` in RAM) is legitimate — do NOT flag it.
- On the PE/COFF host, `objdump -h` showing LMA == VMA is a host format
  property, NOT a script bug — do NOT flag the script for it.

## Verification commands (host, GNU ld 2.46 / gcc 16.1, PE/COFF)

```
# good: script accepted, contract symbols defined, vector table kept by GC
gcc -nostdlib -O2 -c examples/good/host_demo.c -o host_demo.o
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,examples/good/host_demo.ld \
    -Wl,--gc-sections -o good.exe host_demo.o          # exit 0
nm good.exe | grep -E '__etext|__data_start|__data_end|__bss_start|__bss_end|__heap_start'
nm good.exe | grep fake_vectors                          # retained (KEEP)

# bad 1: vector table in RAM region
gcc -nostdlib -c examples/bad/vector-in-ram/host_main.c -o v.o
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,examples/bad/vector-in-ram/bad.ld \
    -o v.exe v.o
objdump -h v.exe | grep isr_vector                        # VMA 0x20000000

# bad 2: forgot KEEP -> GC drops the table
gcc -nostdlib -c examples/bad/forgot-keep/host_main.c -o k.o
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,examples/bad/forgot-keep/bad.ld \
    -Wl,--gc-sections -o k.exe k.o
nm k.exe | grep fake_vectors                              # absent

# bad 3: script/startup symbol mismatch -> undefined reference
gcc -nostdlib -c examples/bad/data-not-copied/host_main.c -o d.o
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,examples/bad/data-not-copied/bad.ld \
    -o d.exe d.o                                          # exit 1, undefined __etext

# bad 4: missing ALIGN vs good ALIGN (compare symbol addresses mod 4)
gcc -nostdlib -c examples/bad/missing-align/host_main.c -o a.o
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,examples/bad/missing-align/bad.ld \
    -o a_bad.exe a.o
nm a_bad.exe | grep -E '__data_end|__bss_start|__heap_start'   # mod 4 != 0
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,examples/good/host_demo.ld \
    -o a_good.exe a.o
nm a_good.exe | grep -E '__data_end|__bss_start|__heap_start'  # mod 4 == 0

# bad 5: region overflow
gcc -nostdlib -c examples/bad/region-overflow/host_main.c -o o.o
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,examples/bad/region-overflow/bad.ld \
    -o o.exe o.o                                          # exit 1, region 'RAM' overflowed
```

## Verified facts (host: MinGW gcc 16.1, GNU ld 2.46, PE/COFF; 2026-08-14)

| Fact | Result | How verified |
|---|---|---|
| `gcc -Wl,-T,<script>` accepts the MEMORY/SECTIONS script | exit 0 | gcc link |
| Contract symbols `__etext/__data_start/__data_end/__bss_start/__bss_end/__heap_start` exist in the linked image | present | `nm` |
| `.isr_vector` retained under `--gc-sections` with KEEP | present in `nm`/`objdump -h` | gcc link + nm |
| `.isr_vector` dropped under `--gc-sections` without KEEP | absent | gcc link + nm |
| `.isr_vector` placed in the RAM region when script says `> RAM` | VMA 0x20000000 | `objdump -h` |
| Script without contract symbols + startup referencing them | exit 1, `undefined reference to '__etext'` | gcc link |
| `.data`/`.bss`/heap symbol alignment without ALIGN (2-byte payload) | mod 4 != 0 | `nm` |
| Same symbols with `. = ALIGN(4)`/`ALIGN(8)` | mod 4 == 0 | `nm` |
| Linking the same C without the custom script | exit 1, undefined `__bss_start` | gcc link |
| Too-small RAM region | `region 'RAM' overflowed by N bytes` | ld link |
| PE/COFF quirk: `AT> FLASH` produces no VMA/LMA split in `objdump -h` | LMA == VMA shown | `objdump -h` |

## Documented-as-target facts (ARM Cortex-M, needs arm-none-eabi-gcc)

| Claim | Expected result |
|---|---|
| CPU reads SP from flash+0 and reset vector from flash+4; table must be at FLASH ORIGIN | boots into `Reset_Handler` |
| Vector table in RAM region -> reset faults (code bytes read as SP/PC) | hard fault at reset |
| `.data` LMA in FLASH, VMA in RAM; `__etext = LOADADDR(.data)`; startup copy loop initializes RAM | `.data` correct at reset |
| Missing copy/bss-zero loops -> stale `.data`, garbage `.bss` | wrong globals at startup |
| Unaligned `.bss`/heap base -> unaligned word access faults on Cortex-M | fault or slow emulation |
| `_estack = ORIGIN(RAM) + LENGTH(RAM)`; first vector word is the initial SP | stack configured at reset |
| Vector table must be KEEP'd when `--gc-sections` is used (target build flags) | table survives GC |

## Scoring (routing eval)

- precision: every placement/symbol claim matches the script text and the
  `nm`/`objdump` output.
- recall: each bad class (wrong region, forgotten KEEP, no copy contract,
  missing ALIGN) is detected from linker evidence.
- FP-rate: `examples/good` and the false-positive list produce zero findings.
- platform correctness: PE-host-verified facts are labeled VERIFIED; VMA/LMA,
  copy-loop, and boot behavior are labeled documented-as-target.
