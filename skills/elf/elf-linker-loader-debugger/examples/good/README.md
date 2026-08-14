# examples/good — correct extern linkage, visibility, and link commands

`libfoo.h` declares the API, `libfoo.c` defines it with default (external) linkage, and
`main.c` calls it through the header. This is the canonical correct path through the
pipeline: one definition, one reference, matching relocation, clean link, clean run.

## Files

- `libfoo.h` — declarations only (`int foo_add(int, int);`, `int foo_get_calls(void);`).
- `libfoo.c` — definitions; `calls` is `static` (file-local, invisible to the linker
  outside this object).
- `main.c` — includes the header and calls both functions.

## Verified build (MinGW, PE/COFF, gcc 16.1 / binutils 2.46)

Static link:

```
gcc -Wall -Wextra -Werror -O2 -c libfoo.c -o libfoo.o   # exit 0
gcc -Wall -Wextra -Werror -O2 -c main.c -o main.o       # exit 0
gcc main.o libfoo.o -o good_static.exe                  # exit 0
./good_static.exe                                       # prints:
#   foo_add(2, 3) = 5
#   calls = 1
# exit 0
```

Symbol view (VERIFIED):

```
nm main.o        #  U foo_add
                 #  U foo_get_calls
                 #  T main
nm libfoo.o      #  T foo_add
                 #  T foo_get_calls
                 #  b calls            <- file-local static
```

Relocation view (VERIFIED): the object's calls are placeholders plus relocation records;
the linked image has patched displacements.

```
objdump -r main.o          # IMAGE_REL_AMD64_REL32  foo_add  (offset 0x14)
objdump -d main.o          # call 9 <main+9>  -- placeholder, not a real target
objdump -d good_static.exe # call 140001490 <foo_add>  -- patched by the linker
```

Shared / dynamic link (VERIFIED):

```
gcc -shared -o libfoo.dll libfoo.o -Wl,--out-implib,libfoo.dll.a   # exit 0
gcc main.o -L. -lfoo -o good_dyn.exe                               # exit 0
./good_dyn.exe                                                     # same output, exit 0
objdump -p good_dyn.exe   # DLL Name: libfoo.dll
                          # foo_add, foo_get_calls imported from libfoo.dll
objdump -p libfoo.dll     # Export Address Table: foo_add, foo_get_calls
```

Debugger mapping (VERIFIED, DWARF debug info inside the PE image):

```
gcc -g -O0 -o good_g0.exe main.c libfoo.c
gdb -batch -ex "info line main" -ex "break main" -ex run -ex step -ex bt good_g0.exe
#   Line 5 of main.c starts at address 0x140001490 <main>
#   Breakpoint 1 at main.c:6
#   Thread 1 hit Breakpoint 1, main () at main.c:6
#   foo_add (a=2, b=3) at libfoo.c:7
#   #0 foo_add ... #1 main ...
```

## Target build on ELF (documented-as-target; needs Linux or WSL)

```
gcc -Wall -Wextra -Werror -O2 -c libfoo.c -o libfoo.o
gcc -Wall -Wextra -Werror -O2 -c main.c -o main.o
gcc main.o libfoo.o -o good_static
gcc -fPIC -shared -o libfoo.so libfoo.o
gcc main.o -L. -lfoo -Wl,-rpath,'$ORIGIN' -o good_dyn
readelf -h -S -l -s -d -r good_dyn
objdump -R good_dyn       # JUMP_SLOT entries for foo_add, foo_get_calls
```

## What this demonstrates

- Correct `extern` linkage: `U` in the caller, `T` in the library, linker resolves.
- Visibility: default-visibility globals become exports; the file-local `calls` does not.
- The link command is the interface between stages: objects first, then libraries,
  `-L` for the library path, `-lfoo` for the name.
- The same DWARF debug information drives the debugger on both PE and ELF.
