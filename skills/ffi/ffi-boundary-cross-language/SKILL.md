---
name: ffi-boundary-cross-language
description: Use when passing data or control across a language boundary — C to Rust, C++ to C, Zig to C, Rust to WASM — where layout, ownership, error translation, and unwind semantics must be pinned. Teaches the shared rules: repr(C) layout, who frees/drops, panic/unwind prohibition, and opaque handles.
---

# Cross-Language FFI Boundary

## When to use

- Exposing a C library to Rust, wrapping C++ in C, calling C from Zig, exporting to WASM.
- Defining `repr(C)`/`extern "C"` interfaces, opaque handles, or callback surfaces.
- Diagnosing crashes at a language boundary (double-free, dangling, ABI mismatch).

## When not to use

- Single-language internal code — layout still matters but ownership rules are intra-language.
- Pure asm/ABI mechanics without a second language — use `abi-layout-reasoning`.

## What the agent often gets wrong

- "The layouts are the same, so passing a struct by pointer is fine" — padding, `bool`
  representation, enum sizes differ (A23).
- "Rust will drop it" — a Rust `Drop` on a C-allocated pointer is a foreign drop (A22);
  ownership must be explicit.
- "Panics don't cross the boundary" — a Rust panic unwinding through `extern "C"` is UB (A20).
- "Stringly-typed handles are fine" — `void*`/`usize` handles lose type safety; use newtype
  or explicit handle structs.
- "The C side will free it" / "Rust will free it" — unspecified ownership is a leak or double-free.

## How to reason correctly

1. Pin the layout: `repr(C)` (or `#[repr(transparent)]` for newtype), verify with
   `offsetof` on both sides; enum → explicit C-compatible discriminant size.
2. Pin ownership: state per object — allocated/freed by which side, or moved once. Prefer
   opaque handles + explicit create/destroy over raw pointers.
3. Pin error translation: return codes/enums, never unwrap across the boundary; map Rust
   errors to C codes explicitly.
4. Pin unwind: `extern "C"` must not unwind; use `catch_unwind` at the boundary in Rust, or
   `noexcept` semantics in C++.
5. Pin strings: `CString`/`CStr` for null-terminated; document who owns the buffer.

## What to verify

- Layout matches: `sizeof`/`offsetof`/`alignof` agree on both sides.
- Ownership is single and explicit: exactly one free/drop, on the agreed side.
- No unwinding crosses the boundary.
- Callback/function-pointer ABI matches (calling convention).

## How to verify

```
# C side layout program (offsetof/sizeof) vs Rust
#[repr(C)] #[derive(Debug)] struct S { ... }
let _ = std::mem::size_of::<S>();   // compare
# ownership: run under valgrind / ASan; for Rust, cargo +nightly miri
# unwind: a Rust function that panics called through extern "C" aborts — test it
```

## Where the knowledge comes from

- Rustonomicon FFI chapter; Rust Reference `repr(C)`; C++ Core Guidelines I.*
- SysV AMD64 / AAPCS64 / Itanium C++ ABI
- Zig c-interop docs (cImport / extern structs)

## Related skills

- `abi-layout-reasoning` (require)
- `rust-ffi-boundary` — Rust-specific repr(C)/ownership detail (extend)
- `c-integer-promotion-and-conversion` — enum/width pitfalls at the boundary

## Evaluation

Historical: CVE-2020-36432 (uninit drop in Rust), CVE-2021-32714 (usize overflow at parsing
boundary). Synthetic: `repr(C)` struct mismatch, foreign drop, panic-through-extern-C,
CString dangling. Adversarial (AD-06): a plausible-but-wrong FFI layout the agent must
compute with `offsetof` instead of guessing.
