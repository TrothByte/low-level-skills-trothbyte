# Evaluation — wasm-runtime-from-scratch

Skill: `skills/_meta/wasm-runtime-from-scratch`. Stability target: `evaluated`.

## Verified facts (GCC 16.1, MSYS2, x86-64 Windows)

`examples/good/interp.c` (bounds-checked, trap-correct skeleton), compiled
`gcc -Wall -Wextra -Werror -O2`:

| test | bytecode | expected | observed | exit |
|---|---|---|---|---|
| 1 mem-roundtrip | const 8, const 4, store, const 8, load, const 12, add, ret | 16 | `PASS mem-roundtrip result=16` | 0 |
| 2 call-indirect | const 5, const 1, call_indirect, ret | 25 (table[1]=sq) | `PASS call-indirect result=25` | 0 |
| 3 load-oob | const 65536, load, ret | trap MEM_OOB | `TRAP load-oob trap=2` | 102 |
| 4 callind-oob | const 0, const 9, call_indirect, ret | trap CALL_INDIRECT_OOB | `TRAP callind-oob trap=3` | 103 |
| 5 stack-underflow | add, ret | trap STACK_UNDERFLOW | `TRAP stack-underflow trap=5` | 105 |
| 6 grow-over-max | const 100, memory.grow, ret | -1 (max 32 pages) | `PASS grow-over-max result=-1` | 0 |

`examples/bad/interp.c` (same bytecode, checks removed) — demonstrates UB instead of traps:

| test | observed | meaning |
|---|---|---|
| 1 | `value=16`, exit 16 | matches good on valid path |
| 2 | `value=25`, exit 25 | matches good on valid path |
| 3 load-oob | `value=0` (garbage; observed 0) | OOB read fabricates a value, no trap |
| 4 callind-oob | crash, exit 0xC0000005 (ACCESS_VIOLATION) | OOB function-pointer read + call |
| 5 stack-underflow | `value=-1545722066 / 1962419915 / 1787373594` across runs | garbage from wild stack index, non-deterministic |
| 6 grow-over-max | `value=1`, exit 1 | grows beyond declared max, no -1 |

Key discrimination: good exit codes {0,102,103,105,0} vs bad {16,25,0,garbage,crash,1};
tests 3-5 differentiate the runtime by whether a defined trap or C UB occurs.

## Synthetic evals

- **easy/positive**: write `vm_push`/`vm_pop` with underflow/overflow traps; must pass
  `-Werror -O2` and not index the stack on empty.
- **medium/positive**: implement `i32.load`/`i32.store` with the
  `addr > size || size - addr < n` check; OOB must trap with code 102, never read/write
  adjacent memory.
- **hard/positive**: implement `call_indirect` (index + null + type checks) and
  `memory.grow` (max cap, overflow, zeroing); reuse the test bytecode arrays from
  `examples/good` and match all six rows of the table above.
- **adversarial**: hostile guests — load at `size-1`, `size`, `0xFFFFFFFF`; store at
  `size-3`; `call_indirect` with index 0xFFFFFFFF; grow by 0xFFFFFFFF pages; `add` on an
  empty stack. Runtime must trap with defined codes, keep the process alive, and match
  wasmtime's trap for the same module.

## False-positive evals

- Correct runtime that bounds-checks every index is NOT flagged: `if (idx < 0 || (size_t)idx
  >= TABLE_LEN) trap(...)` is the correct pattern, not a bug.
- `memory.grow` returning -1 on failure is correct behavior, not an "error" to flag.
- Traps matching wasmtime output are correct, not defects.

## Verification commands

```
cd examples/good && gcc -Wall -Wextra -Werror -O2 interp.c -o interp
for i in 1 2 3 4 5 6; do ./interp $i; done     # expect PASS, PASS, TRAP 102, 103, 105, PASS
cd ../bad  && gcc -Wall -Wextra -Werror -O2 interp.c -o interp
for i in 1 2 3 4 5 6; do ./interp $i; done     # expect garbage/crash on 3-5 (UB demo)
# sanitizer gate on the good runtime
gcc -O2 -g -fsanitize=address,undefined interp.c -o interp_asan && ./interp_asan 3
```

## Tool gap

`wat2wasm` / `wasmtime` / `wasm-interp` are NOT installed on this machine (checked with
`Get-Command`). Cross-checking good-runtime traps against a reference implementation is a
documented target: install wabt + wasmtime and re-run tests 2-5 with the same WAT programs.

ASan/UBSan runtime libs are also NOT installed in this MSYS2 (ucrt64) GCC 16.1 toolchain
(`ld: cannot find -lasan`). The sanitizer gate (`-fsanitize=address,undefined`) is a
documented target on a toolchain with libasan/libubsan (e.g. Linux GCC/Clang, or
`pacman -S mingw-w64-ucrt-x86_64-gcc` with sanitizer packages).
