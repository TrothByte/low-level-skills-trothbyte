---
name: wasm-runtime-from-scratch
description: Use when writing, reviewing, or debugging a WebAssembly runtime, interpreter, loader, or validator in C — module binary parsing, validation, linear memory bounds, tables and call_indirect, traps vs undefined behavior, memory.grow, and host function imports.
---

# Writing a WebAssembly Runtime From Scratch

## When to use

- Implementing a WASM binary loader, validator, or interpreter in C (or any memory-unsafe
  language).
- Reviewing a runtime for unchecked guest-controlled indices into memory, tables, or stacks.
- Adding `memory.grow`, `call_indirect`, or imported host functions to an existing runtime.
- Debugging why a runtime "sometimes crashes" on a guest that should simply trap.

## When not to use

- Reading/understanding existing WASM bytecode — use `elf-binary-parsing`-style skills only
  if you actually need binary section tools; this skill is about building the runtime.
- Writing WASM guest code (WAT/WebAssembly itself) — that is spec-side authoring, not runtime.
- Compiler backend work targeting WASM as an output format — that is codegen, not a runtime.
- A browser/JVM embedded runtime you do not control.

## What the agent often gets wrong

- "The guest is buggy, so the host can do anything." Wrong: guest errors are defined traps;
  an unchecked C array index turns a trap into host UB (see `c-undefined-behavior`).
- Checking `addr < size` but not `addr + width <= size` on every memory access.
- `table[i]()` with `i` from the guest, no length or null check (OOB function-pointer read).
- Popping the operand stack with no underflow check, fabricating a value instead of failing.
- Skipping validation and "just running" — malformed modules must be rejected before execution.
- Treating `memory.grow` failure as "realloc returned NULL, keep old pointer" but forgetting
  to return -1, or forgetting to zero new pages, or ignoring the page-count maximum.
- Not checking that the code section body count matches the function section.

## How to reason correctly

1. Split the pipeline: decode sections → validate types/indices → execute. Run them in order;
   a module is executed only if decode and validation succeed.
2. Every guest-controlled value that indexes a host array (memory, table, stack, types) is
   untrusted: bound it, then trap, exactly as the spec requires.
3. Distinguish layers: a WASM trap is defined behavior for the guest; in the C runtime it
   must become a checked error path, never an out-of-bounds access or null dereference.
4. Model the operand stack as typed and depth-capped; every pop/push checks before touching
   the array.
5. Verify against a reference implementation (wasmtime/wasm-interp), not against your own
   expectations.

## What to verify

- Loader: rejects truncated, reordered, and duplicate sections with defined errors, no crash.
- Validator: rejects type errors (missing operand, `br` to unknown label, bad `call_indirect`
  type index) before execution.
- Memory: load/store at `size - 1` ok; at `size` and `0xFFFFFFFF` traps; `memory.grow`
  returns old size or -1 and never exceeds the declared maximum; new pages are zero.
- `call_indirect`: index OOB and null entry trap; valid index dispatches correctly.
- Stack: underflow traps (or module fails validation), overflow at max depth is handled.
- Traps: deterministic, same outcome as a reference runtime, host process survives.
- Build: `-Wall -Wextra -Werror -O2` clean.

## How to verify

```
gcc -Wall -Wextra -Werror -O2 interp.c -o interp && ./interp 1   # see examples/good
# good interpreter: tests 3,4,5 must exit 102/103/105 (traps); tests 1,2 pass
# same bytecode under wasmtime/wat2wasm must produce the same trap
# ASan/UBSan run must be clean: gcc -fsanitize=address,undefined ...
```

## Where the knowledge comes from

- WebAssembly Core Specification 2.0 (`wasm-core-spec`): module structure §3.2, validation
  §3.3/§4.5, linear memory §3.3.4, tables §3.3.5, traps §3.2.5, binary format §4.1.
- ISO C11 N1570 (`iso-c11-n1570`) §6.5.6p8, Annex J.2 — why unchecked indices are C UB.
- MITRE CWE (`cwe`): CWE-787/125 (OOB write/read), CWE-476 (null dereference).

## Related skills

- `c-undefined-behavior` — traps vs UB, OOB access taxonomy (require)
- `safe-low-level-from-scratch` — safe writing process for the runtime code (recommend)
- `ffi-boundary-cross-language` — host function imports cross the trust boundary (recommend)
- `meta-verification` — verification gates for the runtime (recommend)

## Evaluation

Synthetic: write a loader/validator/interpreter for a 6-op subset and pass the test suite in
`examples/good`; bad runtime must be rejected on review for each missing check.
Adversarial: feed truncated binaries and hostile guests (OOB load/store, call_indirect index
beyond table, empty-stack add, grow beyond max) — runtime must trap with defined codes and
survive, never crash or fabricate values.
False-positive: a correct runtime that bounds-checks every index must NOT be flagged;
`call_indirect` and memory traps that match wasmtime's behavior are correct.
Verified: examples/good/interp.c exits 0 on valid programs, 102/103/105 on the three trap
tests, 0 with result=-1 on grow-over-max, clean under `-Werror -O2`.
