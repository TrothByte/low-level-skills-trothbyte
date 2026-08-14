# examples/bad — four silent or loud firmware-linking failures

Each subdirectory is a minimal reproduction of one failure class with a target
`bad.ld` + `startup.c` and a `host_main.c` that reproduces the relevant linker
behavior on this host (GNU ld 2.46, PE/COFF).

| Directory | Bug | Host evidence |
|---|---|---|
| `vector-in-ram` | vector table placed in RAM region | `objdump -h` shows `.isr_vector` at 0x20000000, not FLASH base |
| `forgot-keep` | `.isr_vector` not KEEP'd | with `--gc-sections`, `nm` shows the vector symbols gone |
| `data-not-copied` | script defines no contract symbols / no `AT>` | link fails: `undefined reference to '__etext'` |
| `missing-align` | no `. = ALIGN(4)` on the data/bss boundary | `nm` shows `__data_end`/`__bss_start`/`__heap_start` mod 4 != 0 |
| `region-overflow` | RAM region smaller than `.bss`+`.data` | link fails: `region 'RAM' overflowed by N bytes` |

Boot-time consequences (CPU reads garbage vectors, RAM `.data` stale, unaligned
faults) are documented-as-target; the linker-side evidence is VERIFIED.
