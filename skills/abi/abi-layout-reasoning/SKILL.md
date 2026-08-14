---
name: abi-layout-reasoning
description: Use when writing or reviewing code that crosses a calling convention or binary interface — structs by value, FFI boundaries, inline asm, varargs, or layout-dependent code. Teaches how to compute struct layout and argument passing for SysV AMD64, AAPCS64, RISC-V psABI, and how to verify with the compiler.
---

# ABI Struct Layout & Calling Convention Reasoning

## When to use

- Passing structs/classes by value to functions (do they go in registers or on the stack?).
- FFI boundaries (C↔Rust↔C++), `repr(C)`, extern structs shared across languages.
- Writing inline asm that must match the caller/callee ABI.
- Reading disassembly and predicting prologue/epilogue and argument placement.
- Serialization/layout-dependent code where padding matters.

## When not to use

- Platform-independent pure logic without ABI crossing (no boundary) — still mind UB (`c-undefined-behavior`).
- Floating-point calling conventions on Windows x64 (different from SysV) — scope is covered for SysV/AAPCS64/psABI here.

## What the agent often gets wrong

- "A struct of two `int`s always goes in two registers" — SysV passes structs by value in
  registers only when they fit the integer register class (≤16 bytes, both members integer);
  otherwise on the stack with specific rules.
- "Alignment is just `alignof`" — struct alignment is the max of member alignments; padding
  is inserted accordingly; `#pragma pack` changes it but then field access can be slow/unaligned.
- "16-byte stack alignment is 'nice to have'" — SysV requires `rsp % 16 == 0` at the `call`
  site; misalignment breaks `movaps`-using leaf functions.
- "A `char` field is 1 byte so a struct of `char, int` is 5 bytes" — it is 8 bytes with 3 padding bytes.

## How to reason correctly

1. Compute member alignment: `alignof(T)`. Struct alignment = max of member alignments.
2. Lay out fields in order; after each, pad to the next field's alignment.
3. Pad the whole struct to a multiple of its alignment at the end.
4. For argument passing, classify each struct by the ABI rules (integer class vs stack;
   ≤16 bytes & all-integer for SysV AMD64) before guessing registers.
5. Verify every assumption with the compiler (`offsetof`, `sizeof`, `-S`).

## What to verify

- `offsetof` / `sizeof` match your hand computation (use `-fdump-record-layouts` or a tiny program).
- The function prologue matches the ABI (argument registers, stack alignment).
- `repr(C)`/packed structs used at FFI match the C side exactly.

## How to verify

```
cat > layout.c <<'EOF'
#include <stddef.h>
#include <stdio.h>
struct S { char c; int i; };
int main(void){ printf("off_c=%zu off_i=%zu size=%zu align=%zu\n",
  offsetof(struct S,c), offsetof(struct S,i), sizeof(struct S), _Alignof(struct S)); }
EOF
gcc layout.c -o /tmp/layout && /tmp/layout
gcc -O2 -S -o - func.c   # check prologue / arg placement
```

## Where the knowledge comes from

- System V AMD64 ABI §3.2 (registers, stack frame), §3.3 (varargs), §4 (data representation)
- AAPCS64 §6.1–6.3, §7 (stack), §8 (data representation)
- RISC-V psABI (calling convention, stack layout)

## Related skills

- `asm-calling-conventions` — the per-architecture reference tables (require)
- `ffi-boundary-cross-language` — using layout at language boundaries (verify)
- `elf-linker-loader-debugger` — binary-level consequences

## Evaluation

Synthetic: struct-passing classification (registers vs stack), padding computation,
alignment trap. Adversarial: "ABI looks plausible but wrong" — a `repr(C)` struct whose
computed layout the agent gets wrong; `works x86 fails ARM` — unaligned field access.
