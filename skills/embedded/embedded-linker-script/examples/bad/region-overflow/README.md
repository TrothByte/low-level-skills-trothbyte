# examples/bad/region-overflow — RAM region too small for the data

`bad.ld` declares `RAM` with `LENGTH = 0x20` (32 bytes) while the program needs
hundreds of bytes of `.bss`.

## Why it breaks

`MEMORY` LENGTH is a hard bound. The linker refuses to build an image that
overflows a region; it is the only check that code+data actually fit the part
(`gnu-ld-manual`, MEMORY command).

## Host reproduction (VERIFIED)

```
gcc -nostdlib -c host_main.c -o host_main.o
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,bad.ld -o bad.exe host_main.o
# ld: ... section .bss ... region 'RAM' overflowed by 228 bytes -> exit 1
```

(The exact overflow byte count depends on the linker version.)

## Fix

Increase `RAM LENGTH` to the real SRAM size from the datasheet, or reduce
`.bss`/`.data`/stack/heap usage. The overflow number printed by ld is the
diagnosis to quote.
