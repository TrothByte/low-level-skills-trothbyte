# Safe Writing Process — Reference

The point of the process is that safety is designed in, not verified in. Every step
produces an artifact that the next step reuses. Format: STEP → CHECKLIST → COMMON TRAP → SOURCE.

## Step 1 — Contract & ownership

- **CHECKLIST**: inputs (ranges, nullability), output lifetime, who owns what, error model,
  thread-safety requirement (none/shared-read/lock-free/mutex).
- **COMMON TRAP**: "the caller will handle it" — unspecified ownership is the root of UAF/leaks.
- **SOURCE**: C++ CG R.3 (raw pointer is non-owning); Rustonomicon ownership rules.

## Step 2 — Types before logic

- **CHECKLIST**: use strong types (`size_t` for lengths, enums for states, newtype/descriptor
  for handles), RAII for resources, `const` where mutable isn't needed.
- **COMMON TRAP**: `int` lengths, `void*` handles, `bool` flags — the type system can't help you.
- **SOURCE**: Rust API Guidelines C-NEWTYPE/C-CUSTOM-TYPE; C++ CG I.*.

## Step 3 — Arithmetic discipline

- **CHECKLIST**: every expression that can overflow is checked or uses a wider type; every
  multiplication feeding an allocation is guarded (`n > SIZE_MAX / k`); signed/unsigned
  decisions are explicit, not accidental (integer promotion!).
- **COMMON TRAP**: `malloc(n * sizeof(T))` — CWE-190. Also `int len = (int)size_t_value` — CVE-2021-33909.
- **SOURCE**: `c-integer-promotion-and-conversion`; CERT INT30-34.

## Step 4 — Memory operations

- **CHECKLIST**: bounds are verified at the source, not at the sink; `memmove` where overlap
  possible; strings always terminated; uninitialized reads impossible; `free` exactly once
  per allocation.
- **COMMON TRAP**: `strncpy` without explicit termination; `memcpy` on overlapping buffers;
  `snprintf` return ignored (truncation).
- **SOURCE**: `c-undefined-behavior` classes 1-7; CERT STR30-35, MEM30-35.

## Step 5 — Concurrency

- **CHECKLIST**: shared state is immutable/atomic/mutex; the sync edge is designed BEFORE the
  ordering is picked; ordering is the weakest that forms the edge; RMWs for counters.
- **COMMON TRAP**: `Relaxed` flag protocol (AD-01); `volatile` for sync; assuming x86 asm
  strength on ARM.
- **SOURCE**: `memory-ordering-reasoning`; C11 §5.1.2.4/§7.17; Intel SDM Vol.3.

## Step 6 — Boundary (FFI/hardware)

- **CHECKLIST**: layout computed with `offsetof` (not hand-summed) and pinned with
  `repr(C)`/packed; ownership transfer explicit (who frees/drops); no panic/unwind across
  extern C; MMIO accesses `volatile` + correct barrier.
- **COMMON TRAP**: repr mismatch (A23); foreign drop (A22); unaligned packed field on ARM (AD-05).
- **SOURCE**: `abi-layout-reasoning`, `ffi-boundary-cross-language`; AAPCS64/SysV.

## Step 7 — Verify against the contract

- **CHECKLIST**: `-Wall -Wextra -Werror -O2` clean; sanitizers appropriate to the bug class
  (ASan/UBSan; TSan/Miri for threads); asm spot-check for UB-sensitive claims; the exact
  success criterion from Step 1 is measured.
- **COMMON TRAP**: verifying at `-O0` only; ASan-clean claimed as race-free.
- **SOURCE**: `meta-verification`; `registry/evals.yaml`.

## Minimal verification matrix (cheat)

| What | Command |
|---|---|
| UB/overflow | `clang -O2 -fsanitize=undefined,address -fno-sanitize-recover=undefined` |
| uninitialized | MSan (`clang -fsanitize=memory`) |
| races (C/C++) | TSan |
| races (Rust) | `cargo +nightly miri run` |
| layout | `offsetof` program + `-fdump-record-layouts` |
| optimizer | diff `-O0` vs `-O2` asm |
