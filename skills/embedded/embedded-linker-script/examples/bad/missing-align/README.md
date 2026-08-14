# examples/bad/missing-align — no ALIGN() on the data/bss boundary

`bad.ld` is otherwise correct (KEEP, `AT> FLASH`, symbols) but has NO
`. = ALIGN(4)` anywhere. `host_main.c` deliberately uses a 2-byte `.data`
payload (`char blob[2]`) and a char-only `.bss`, so the natural boundary is
mod-2.

## Why it breaks

The location counter `.` advances byte-for-byte. After a 2-byte `.data`, the
next symbol is 2-byte aligned; without `. = ALIGN(4)` the `.bss` base, heap
base, and stack base can land on mod-2 addresses. On Cortex-M unaligned word
loads/stores fault or are emulated slowly, and a misaligned heap poisons every
`malloc`-style allocation (documented-as-target).

## Host reproduction (VERIFIED)

Compare symbol addresses from the bad and good scripts:

```
gcc -nostdlib -c host_main.c -o host_main.o
gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,bad.ld -o bad.exe host_main.o
nm bad.exe | grep -E '__data_end|__bss_start|__bss_end|__heap_start'
# __data_end and __bss_start are NOT 4-aligned (mod 4 != 0)

gcc -nostdlib -Wl,--image-base,0x1000000 -Wl,-T,../good/host_demo.ld \
    -o good.exe host_main.o
nm good.exe | grep -E '__data_end|__bss_start|__bss_end|__heap_start'
# all 4-aligned (mod 4 == 0)
```

The `good` script snaps the dot with `. = ALIGN(4)` inside `.data`/`.bss` and
`. = ALIGN(8)` before `__heap_start`; the bad script does not.

## Fix

Add explicit alignment at each boundary you hand a symbol across:
`. = ALIGN(4);` before `__data_start`, before `__bss_start`, and
`. = ALIGN(8);` before the heap/stack symbols (see `cortex_m7.ld`).
