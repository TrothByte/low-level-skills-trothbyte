# examples/bad/vector-in-ram — vector table placed in the RAM region

The `.isr_vector` output section is placed with `> RAM`, so the vector table
ends up at the RAM base instead of the flash base.

## Why it breaks

On Cortex-M the CPU fetches the initial stack pointer from address
`ORIGIN(FLASH) + 0` and the reset vector from `+ 4` (unless VTOR redirects the
table). If the table lives in RAM, the flash address 0 holds code bytes, the
CPU treats them as SP/PC values, and reset faults immediately
(documented-as-target, `cmsis`).

## Host reproduction (VERIFIED)

The linker places the section where the script says, independent of the bug:

```
gcc -nostdlib -c host_main.c -o host_main.o
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,bad.ld -o out.exe host_main.o
nm out.exe | grep fake_vectors        # present
objdump -h out.exe                    # .isr_vector VMA 0x20000000 (RAM base)
```

Result: `.isr_vector` at VMA `0x20000000` — inside RAM, not at FLASH base
`0x08000000`. The section exists (KEEP works); the REGION is wrong.

## Fix

Place `.isr_vector` first in FLASH: `KEEP(*(.isr_vector)) } > FLASH` (see
`examples/good/host_demo.ld`).
