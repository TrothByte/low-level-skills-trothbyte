# examples/bad/forgot-keep — vector table not wrapped in KEEP()

The `.isr_vector` input is listed inside `.text` but WITHOUT `KEEP(...)`.
The rest of the script is fine; the missing KEEP is the only bug.

## Why it breaks

`--gc-sections` keeps only sections reachable from the entry point and other
roots. The vector table is data referenced by the CPU's reset fetch, which the
linker cannot see, so with GC enabled the whole table is discarded
(`gnu-ld-manual`, KEEP; `binutils-docs`).

## Host reproduction (VERIFIED)

The GC behavior is reproduced on the host linker:

```
gcc -nostdlib -c host_main.c -o host_main.o
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,bad.ld \
    -Wl,--gc-sections -o bad.exe host_main.o
nm bad.exe | grep fake_vectors          # NOT present — GC dropped it
```

Same object linked with `examples/good/host_demo.ld` (which has KEEP):
`nm good.exe | grep fake_vectors` lists it (VERIFIED). The only difference is
`KEEP(*(.isr_vector))`.

## Fix

Wrap the vector table: `KEEP(*(.isr_vector))`. Without GC (`--gc-sections`
absent) the table survives, which is why this bug hides until GC is enabled —
always link firmware with GC on and check `nm` for the vector symbols.
