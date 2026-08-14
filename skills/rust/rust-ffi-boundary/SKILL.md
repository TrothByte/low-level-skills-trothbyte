---
name: rust-ffi-boundary
description: Use when writing or reviewing Rust FFI code — repr(C) layout, enum discriminants, CString/CStr and Box::into_raw ownership, extern "C" callbacks, opaque handles, or panic/unwind at the boundary. Teaches Rust-specific rules for safe C interop and how to verify layout and ownership on both sides.
---

# Rust FFI Boundary

## When to use

- Declaring or consuming C interfaces from Rust: `repr(C)` structs, `extern "C"` blocks,
  `#[no_mangle]` exports, callbacks, opaque handles.
- Moving strings or heap objects across the boundary (`CString`/`CStr`,
  `Box::into_raw`/`from_raw`); pinning the ABI of a C-compatible enum or callback.
- Debugging boundary crashes: double-free, foreign drop, layout mismatch, abort on panic.

## When not to use

- General unsafe Rust without a C counterpart — use `rust-unsafe-reasoning`.
- Non-Rust boundaries — use `ffi-boundary-cross-language`; pure ABI mechanics —
  use `abi-layout-reasoning`.

## What the agent often gets wrong

- "`repr(C)` matches whatever C does" — a `#pragma pack(1)` C struct is NOT matched by
  `#[repr(C)]`; offsets drift (C 9 bytes vs Rust 12).
- "An enum is just an int" — a plain `enum` is not FFI-safe (rustc only warns
  `improper_ctypes_definitions`); `repr(C)` enums leave C `int` size once a
  discriminant passes `INT_MAX`.
- "`from_raw` just wraps the pointer" — two `from_raw` on one pointer = double-free;
  `from_raw` on C-allocated memory = foreign free.
- "Panics don't cross `extern "C"`" — they abort (verified: exit 0xC0000409); rustc
  emits no warning.
- "Pass the closure, C can't tell" — capturing closures are not function pointers; use
  `extern "C" fn` + a context pointer.
- "Rust `char` == C `char`" — Rust `char` is 4 bytes and not FFI-safe; use `c_char`/`u32`.
- "`no_mangle` is about performance" — it is the symbol-export mechanism; without it the
  C linker cannot resolve the mangled name.

## How to reason correctly

1. Pin layout: `#[repr(C)]` (or `#[repr(transparent)]` for one-field newtypes); verify
   `size_of`/`align_of`/`offset_of` against C `sizeof`/`_Alignof`/`offsetof`.
2. Pin values: enums get explicit discriminants and a C-compatible representation;
   booleans are exactly 0/1; bytes use `c_char`/`c_uchar`, never Rust `char`.
3. Pin ownership: one owner per allocation — borrow via `CStr::from_ptr`, or transfer
   via `into_raw`/`from_raw` exactly once, on the agreed side.
4. Pin control flow: wrap every `extern "C"` export in `catch_unwind`; callbacks are
   `extern "C" fn` + user-data pointer.
5. Pin symbols: `#[no_mangle] pub extern "C" fn` with namespaced, unique names.

## What to verify

- Size, alignment, and every critical field offset agree on both sides.
- One owner per pointer: no double `from_raw`, no C buffer passed to `from_raw`, no
  dangling `&` exported instead of `into_raw`.
- No panic path can unwind through an `extern "C"` frame; callback signatures and
  exported symbol names match the C declarations.

## How to verify

```
# layout on both sides, compare outputs line by line
rustc --edition 2021 examples/good/rust_layout.rs -o rust_layout && ./rust_layout
gcc -std=c11 -Wall -Wextra -Werror -O2 examples/good/c_side.c -o c_side && ./c_side

# good examples compile as libs; C headers under -Werror
for f in examples/good/*.rs; do [ "$f" = examples/good/rust_layout.rs ] || rustc --edition 2021 --crate-type=lib "$f" || exit 1; done
gcc -Wall -Wextra -Werror -O2 -c examples/good/c_side.c examples/bad/c_side.c

# bad cases: record the diagnostic, don't let it pass silently
rustc --edition 2021 --crate-type=lib examples/bad/bad_enum.rs      # improper_ctypes_definitions
rustc --edition 2021 --crate-type=lib examples/bad/bad_callback.rs  # E0308, fails
rustc --edition 2021 examples/bad/run_bad.rs -o run_bad && ./run_bad  # abort 0xC0000409

cargo +nightly miri test     # double from_raw, foreign free, leak
```

## Where the knowledge comes from

- Rust Reference: type-layout (repr(C)/repr(transparent), primitives), items/functions
  (extern ABI, no_mangle), unsafety, behavior-considered-undefined
- Rustonomicon: FFI, Unwinding, Ownership; Rust API Guidelines C-NEWTYPE;
  clippy `improper_ctypes_definitions`; Matklad on handle types
- SysV AMD64 ABI §3.2/§4, AAPCS64 §6–8 (calling conventions, data representation)

## Related skills

- `ffi-boundary-cross-language` — shared boundary rules; this skill `extend`s it with
  Rust-specific repr/ownership/unwind detail (require)
- `abi-layout-reasoning` — computing layout and argument placement (require)
- `rust-unsafe-reasoning` — unsafe semantics under the FFI surface (recommend)
- `c-undefined-behavior` — the C side's UB rules at the same boundary

## Evaluation

Synthetic: easy (repr(C) vs plain struct), medium (double `from_raw`), hard (panic
through `extern "C"`), adversarial (plausible-but-wrong packed layout the agent must
compute with `offsetof`; capturing closure as fn pointer).
False-positive: correct `repr(C)` + verified layout, `catch_unwind` exports, documented
C-owns/CStr-borrow patterns must NOT be flagged.
Historical: CVE-2020-36432 (drop of uninitialized value), CVE-2021-32714 (usize overflow
in boundary parsing).
