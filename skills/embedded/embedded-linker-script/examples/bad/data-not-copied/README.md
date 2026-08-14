# examples/bad/data-not-copied — startup/script contract mismatch

Two failures, one directory:

## 1. The script does not define the contract symbols (link error)

`bad.ld` places `.data`/`.bss` in RAM but never defines `__etext`,
`__data_start`, `__data_end`, `__bss_start`, `__bss_end`, and never uses
`AT> FLASH`. The startup file (`startup.c`, identical to the good one)
references those names.

Host reproduction (VERIFIED):

```
gcc -nostdlib -c host_main.c -o host_main.o
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,bad.ld -o bad.exe host_main.o
# ld: ... undefined reference to `__etext'
# ld: ... undefined reference to `__bss_start'     -> exit 1
```

Diagnosis: script and startup disagree on the contract. Either the script must
define the symbols (good scripts do), or the startup must not use them.

## 2. The script is correct but the startup skips the copy (silent, target)

If `bad.ld` were fixed to define the symbols AND use `AT> FLASH` for `.data`,
but the startup never ran the copy/bss-zero loops, the link would succeed. On
bare metal RAM holds undefined values at reset, so `.data` starts stale and
`.bss` contains garbage (documented-as-target; not reproducible on the PE
host, whose OS loader initializes sections).

## Also wrong in this script

`.data > RAM` without `AT> FLASH`: the initial `.data` payload is not emitted
into the flash image, so even a correct copy loop would copy nothing useful.
The correct pair is `.data : AT> FLASH { ... } > RAM` with
`__etext = LOADADDR(.data)` (see `examples/good/cortex_m7.ld`).
