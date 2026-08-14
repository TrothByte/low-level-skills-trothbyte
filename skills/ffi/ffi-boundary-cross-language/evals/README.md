# Evaluation — ffi-boundary-cross-language

Skill: `skills/ffi/ffi-boundary-cross-language`. Stability target: `evaluated`.

## Historical CVE evals

- **CVE-2020-36432** (alg_ds, Rust): drop of uninitialized value — ownership/init at the
  boundary. DETECT (uninit read in drop) → EXPLAIN (validity rules) → FIX → VERIFY (Miri).
- **CVE-2021-32714** (hyper, Rust): usize overflow in chunked parser — size arithmetic at a
  boundary. DETECT → EXPLAIN (CWE-190) → FIX → VERIFY.

## Synthetic evals

- **easy/negative**: `#[repr(C)]` vs plain struct passed to C — detect unpinned layout.
- **medium/negative**: `CString::from_raw` called twice — detect double-ownership.
- **hard/negative**: `extern "C"` function that panics — detect unwind-across-boundary UB.
- **adversarial (AD-06)**: a plausible-but-wrong FFI struct (padding/enum size) — agent must
  compute with `offsetof`/`size_of` on both sides, not guess.

## False-positive evals

- Correct `repr(C)` + verified layout — must NOT be flagged.
- A documented C-owns pointer (Rust borrows via `CStr`) — must NOT be "fixed" into `from_raw`.
- A `catch_unwind`-wrapped export — must NOT be flagged as "unnecessary".

## Verified facts

- `examples/good/ffi_good.rs` compiles (rustc 1.97.1, edition 2021).
- `#[repr(C)] Header { u32, u8, [u8;2], u32 }` → size 12, align 4 (matches C: 4+1+2+pad1+4).
- `catch_unwind` + error-code translation pattern is the correct panic boundary.
- `examples/bad/ffi_bad.rs`: repr-free struct, double `from_raw`, panic-through-`extern "C"`
  — each maps to a bug class (A23, A21/A22, A20).

## Verification commands

```
rustc --edition 2021 --crate-type=lib examples/good/ffi_good.rs
cargo +nightly miri run    # ownership/UB checks (Rust)
# C side: offsetof program to cross-check layout
```
