# examples/good — a shared library and a main that calls it correctly

`libcounter` hides all state (`static int counter`) behind three exported API functions.
`main.c` uses only the API — nothing leaks across the dynamic boundary, which is the
correct visibility discipline. The same sources demonstrate the PE/COFF analog of
GOT/PLT (import thunk ≈ PLT stub, IAT slot ≈ GOT slot) on this host.

## Files

- `libcounter.h` — `LIBCOUNTER_API` macro: `__declspec(dllexport)` when building the
  library on Windows, `dllimport` for consumers, empty on ELF. Build the library with
  `-DLIBCOUNTER_BUILD`.
- `libcounter.c` — `static int counter` (file-local, never exported) plus
  `counter_reset`/`counter_bump`/`counter_get`.
- `main.c` — calls only the API, checks `counter_get() == 3`.

## Verified build (MinGW gcc 16.1 / binutils 2.46, PE/COFF)

Compile clean, build DLL, dynamic link, run (VERIFIED):

```
gcc -Wall -Wextra -Werror -O2 -DLIBCOUNTER_BUILD -c libcounter.c -o libcounter.o
gcc -Wall -Wextra -Werror -O2 -c main.c -o main.o
gcc -shared -o libcounter.dll libcounter.o -Wl,--out-implib=libcounter.dll.a
gcc main.o -L. -lcounter -o good_dyn.exe
./good_dyn.exe
# counter = 3        -> exit 0
```

Export table — exactly the three API functions; `counter` is `static` and absent
(VERIFIED):

```
objdump -p libcounter.dll
# ... Export Address Table -- Ordinal Base 1
# [   0] +base[   1]  0000 counter_bump
# [   1] +base[   2]  0001 counter_get
# [   2] +base[   3]  0002 counter_reset
```

Import table — the exe records its dependency (VERIFIED):

```
objdump -p good_dyn.exe | grep -A3 "DLL Name"
# > DLL Name: libcounter.dll
#   vma:     Ordinal  Hint  Member-Name  Bound-To
#   00008290  <none>  0000  counter_bump
#   00008298  <none>  0001  counter_get
#   000082a0  <none>  0002  counter_reset
```

Note the `vma` values are exactly the IAT slots used by the import thunks below
(`__IAT_start__` = 0x8290, `__imp_counter_get` = 0x8298, `__imp_counter_reset` =
0x82a0): the import table and the thunks are two views of the same slots.

Import thunks — the PE analog of PLT stubs: `jmp *rel32(%rip)` through the `__imp_*`
IAT slot (the GOT analog). The Windows loader fills the IAT at process start (eager,
unlike ELF lazy binding) (VERIFIED):

```
objdump -d good_dyn.exe
# 0000000140001490 <counter_reset>:
#   140001490:  ff 25 0a 6e 00 00   jmp    *0x6e0a(%rip)   # 1400082a0 <__imp_counter_reset>
#   140001496:  90                  nop
# 0000000140001498 <counter_get>:
#   140001498:  ff 25 fa 6d 00 00   jmp    *0x6dfa(%rip)   # 140008298 <__imp_counter_get>
# 00000001400014a0 <counter_bump>:
#   1400014a0:  ff 25 ea 6d 00 00   jmp    *0x6dea(%rip)   # 140008290 <__IAT_start__>
```

Calls in `main` are indirect through the IAT slots (VERIFIED):

```
# 0000000140002a30 <main>:
#   140002a3a:  ff 15 60 58 00 00   call   *0x5860(%rip)   # 1400082a0 <__imp_counter_reset>
#   140002a40:  48 8b 1d 49 58 00 00  mov   0x5849(%rip),%rbx  # 140008290 <__IAT_start__>
#   140002a47:  ff d3              call   *%rbx
#   140002a49:  ff d3              call   *%rbx
#   140002a4b:  ff d3              call   *%rbx
#   140002a4d:  48 8b 1d 44 58 00 00  mov   0x5844(%rip),%rbx  # 140008298 <__imp_counter_get>
```

`objdump -R` on PE (VERIFIED — ELF-only tool):

```
objdump -R good_dyn.exe
# objdump: good_dyn.exe: not a dynamic object      -> exit 1
```

Static link also works (VERIFIED): `gcc main.o libcounter.o -o good_static.exe` runs and
prints `counter = 3`.

## Target build on ELF (documented-as-target; needs Linux or WSL)

```
gcc -Wall -Wextra -Werror -O2 -fPIC -shared -o libcounter.so libcounter.c
gcc -Wall -Wextra -Werror -O2 -pie -fPIE main.c -L. -lcounter -Wl,-rpath,'$ORIGIN' -o prog
./prog                 # counter = 3
readelf -d prog        # NEEDED libcounter.so, JMPREL, RELA
objdump -R prog        # R_X86_64_JUMP_SLOT counter_reset/counter_bump/counter_get
objdump -d prog | grep '@plt'
nm -D libcounter.so    # exported: counter_bump counter_get counter_reset (no counter)
LD_DEBUG=bindings ./prog    # counter_bump binds on first call only
LD_BIND_NOW=1 ./prog        # all bindings at load
```

## What this demonstrates

- File-local state stays private: `static int counter` appears in neither the DLL export
  table nor (on ELF) `.dynsym`; the executable must use the API.
- External calls are indirect: through PLT/GOT on ELF, through the import thunk/IAT on
  PE — never a direct call to the other module's code.
- The PE/COFF analog is structural only: imports are resolved eagerly at load on
  Windows, so lazy binding (`LD_BIND_NOW`) has no PE counterpart.
