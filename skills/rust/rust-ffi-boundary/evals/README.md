# Evaluation — rust-ffi-boundary

Skill: `skills/rust/rust-ffi-boundary`. Extends `ffi-boundary-cross-language`.
Stability target: `evaluated`. All facts below were verified on 2026-08-14 with
rustc 1.97.1 (x86_64-pc-windows-msvc) and gcc 16.1 (MinGW, Windows x64).

## Synthetic evals

- **easy/negative**: a plain `struct` (no `repr`) passed to an `extern "C"` fn — the
  agent must add `#[repr(C)]` and verify layout.
- **medium/negative**: `CString::from_raw(p)` called twice — the agent must detect two
  owners of one allocation (double free).
- **hard/negative**: `extern "C" fn` body panics — the agent must wrap it in
  `catch_unwind` and translate the panic to an error code.
- **hard/negative**: a plain enum passed to C — the agent must recognize it is not
  FFI-safe (warning, not error) and add a representation + explicit discriminants.
- **adversarial (AD-06 pattern)**: C declares `#pragma pack(1)` struct (size 9,
  len@5); Rust `#[repr(C)]` gives size 12, len@8. The agent must compute offsets on
  both sides instead of guessing that repr(C) "just matches".
- **adversarial**: a capturing closure passed where C expects a function pointer —
  the agent must explain E0308 and redesign with `extern "C" fn` + context pointer.

## False-positive evals

- A correct `#[repr(C)]` struct whose `size_of`/`offset_of` match C `sizeof`/`offsetof`
  must NOT be flagged.
- A `catch_unwind`-wrapped export with error-code translation must NOT be flagged as
  "unnecessary" or "panic is impossible here".
- A documented C-owns pointer that Rust borrows via `CStr::from_ptr` must NOT be
  "fixed" into `CString::from_raw`.
- A `#[repr(u8)]` protocol enum with explicit `= 0, = 1` discriminants is correct and
  must NOT be flagged as "missing repr(C)".
- A `#[repr(transparent)]` handle with explicit create/destroy pair must NOT be flagged
  as "raw pointer without ownership".

## Verified facts

- Layout match (good): `Header { u32, u8, [u8;2], u32 }` — Rust
  size=12 align=4 off_len=8; C size=12 align=4 off_len=8.
- Layout match (good): `#[repr(C)] enum Status { Ok=0, Busy=1, Error=2 }` — Rust
  size=4 align=4; C `enum status` size=4 align=4.
- Layout match (good): `{ bool, u32 }` — Rust size=8 align=4 off_v=4; C
  `{ _Bool, uint32_t }` size=8 align=4 off_v=4; `_Bool` size=1, `bool` size=1.
- Layout mismatch (bad): packed C `struct msg` size=9 align=1 off_len=5 vs Rust
  `#[repr(C)]` size=12 align=4 (len at offset 8) — a 3-byte/3-offset drift.
- `#[repr(transparent)]` handle over `*mut c_void`: size=8 align=8.
- `#[repr(C)]` enum with discriminant `1isize << 40`: size=8, and rustc 1.97.1 warns
  `repr_c_enums_larger_than_int` (future-incompatible, issue #124403).
- Panic through `extern "C"`: compiles clean (no warning); running it aborts with
  "panic in a function that cannot unwind", exit status 0xC0000409 (the process is
  killed; `catch_unwind` on the Rust caller side does not rescue it).
- Plain enum in `extern "C"`: compiles, warns `improper_ctypes_definitions`
  ("enum has no representation hint").
- Rust `char` in `extern "C"`: warns `improper_ctypes_definitions` ("the `char` type
  has no C equivalent").
- Closure as fn pointer: error E0308 "expected fn pointer, found closure", with note
  "closures can only be coerced to `fn` types if they do not capture any variables".

## Verification commands

Run from the skill directory:

```
# good Rust, strict lib compile
rustc --edition 2021 --crate-type=lib examples/good/good_layout.rs
rustc --edition 2021 --crate-type=lib examples/good/good_enum.rs
rustc --edition 2021 --crate-type=lib examples/good/good_ownership.rs
rustc --edition 2021 --crate-type=lib examples/good/good_unwind.rs
rustc --edition 2021 --crate-type=lib examples/good/good_callback.rs

# layout comparison
rustc --edition 2021 examples/good/rust_layout.rs -o rust_layout && ./rust_layout
gcc -std=c11 -Wall -Wextra -Werror -O2 examples/good/c_side.c -o c_side && ./c_side

# C side under -Werror
gcc -Wall -Wextra -Werror -O2 -c examples/good/c_side.c
gcc -Wall -Wextra -Werror -O2 -c examples/bad/c_side.c

# bad examples: record the outcome, do not let them pass silently
rustc --edition 2021 --crate-type=lib examples/bad/bad_layout.rs      # compiles; mismatch vs C
rustc --edition 2021 --crate-type=lib examples/bad/bad_ownership.rs   # compiles; double free
rustc --edition 2021 --crate-type=lib examples/bad/bad_unwind.rs      # compiles; abort at runtime
rustc --edition 2021 examples/bad/run_bad.rs -o run_bad && ./run_bad  # exit 0xC0000409
rustc --edition 2021 --crate-type=lib examples/bad/bad_enum.rs        # improper_ctypes_definitions
rustc --edition 2021 --crate-type=lib examples/bad/bad_callback.rs    # E0308, fails

# ownership/UB checks
cargo +nightly miri test       # double from_raw, foreign free
```

## Historical CVE evals

- **CVE-2020-36432** (alg_ds, Rust): drop of an uninitialized value — an ownership
  failure at the boundary. DETECT (uninit read in drop) -> EXPLAIN (validity rules) ->
  FIX -> VERIFY (Miri).
- **CVE-2021-32714** (hyper, Rust): `usize` overflow in chunked-transfer parsing — size
  arithmetic at a parsing boundary. DETECT -> EXPLAIN (CWE-190) -> FIX -> VERIFY.

## Cross-skill notes

- Layout and calling-convention details live in `abi-layout-reasoning`; this skill owns
  the Rust-specific repr/discriminant/ownership/unwind rules.
- General boundary rules (who frees, opaque handles, no-unwind) are in
  `ffi-boundary-cross-language`; this skill extends that surface with Rust specifics.
