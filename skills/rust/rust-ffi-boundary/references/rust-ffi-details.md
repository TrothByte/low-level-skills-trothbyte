# Rust FFI Boundary — Reference

Rust-specific rules for the C boundary: layout, discriminants, ownership, callbacks,
unwind. Verified against rustc 1.97.1 (x86_64-pc-windows-msvc) and GCC 16.1 (MinGW,
Windows x64) on 2026-08-14. Format per rule: RULE / WHY AI GETS IT WRONG /
CORRECT REASONING / EXAMPLE / COUNTEREXAMPLE / VERIFICATION / SOURCE.

## 1. Cross-boundary structs need repr(C); verify with size_of/offsetof

- **RULE**: any struct read or written by C must be `#[repr(C)]` (or a C-compatible
  alternative). Its size, alignment, and field offsets must then be verified on both
  sides with `size_of`/`offset_of` (Rust) and `sizeof`/`offsetof` (C) — never assumed.
- **WHY AI GETS IT WRONG**: "Rust structs are laid out in field order anyway" — default
  Rust layout is unspecified and may reorder/pad; `#[derive(Default)]` layouts look
  right in tests and break at the boundary.
- **CORRECT REASONING**: `#[repr(C)]` fixes field order and C-style padding. Padding is
  inserted after each field to align the next field and at the end to align the whole
  struct. If the C side is packed (`#pragma pack(1)`), Rust `repr(C)` does NOT follow —
  offsets drift and every field access misreads.
- **EXAMPLE** (bad):
  ```rust
  #[repr(C)]
  struct Msg { magic: u32, ver: u8, len: u32 }  // Rust: size 12, len at offset 8
  ```
  paired with a C side that is `#pragma pack(push,1)` (size 9, len at offset 5).
- **COUNTEREXAMPLE** (good):
  ```rust
  #[repr(C)]
  struct Header { magic: u32, ver: u8, _pad: [u8; 2], len: u32 }  // size 12 both sides
  ```
  verified: Rust `size_of = 12, align_of = 4, offset_of(len) = 8`; C
  `sizeof = 12, _Alignof = 4, offsetof(len) = 8`.
- **VERIFICATION**: compile and run a layout probe on both sides (see
  `examples/good/rust_layout.rs` vs `examples/good/c_side.c`); compare the numbers.
- **SOURCE**: rust-reference type-layout.html; rustonomicon FFI chapter.

## 2. repr(transparent) for single-field newtypes

- **RULE**: a newtype wrapping exactly one non-zero-sized field that crosses the
  boundary gets `#[repr(transparent)]`, which guarantees the same layout and ABI as
  the inner field.
- **WHY AI GETS IT WRONG**: agents reach for `repr(C)` on every wrapper, or wrap a
  `*mut c_void` in a plain struct and pass it, silently changing size/alignment.
- **CORRECT REASONING**: `repr(transparent)` keeps a handle exactly one pointer wide
  (size 8, align 8 on x86-64) with no padding baggage, and lets the field be accessed
  safely. `repr(C)` on a newtype also works but is noisier.
- **EXAMPLE** (bad): `struct Handle(*mut c_void);` (unspecified layout) passed to C.
- **COUNTEREXAMPLE** (good): `#[repr(transparent)] pub struct Handle(pub *mut c_void);`
  — verified `size_of::<Handle>() == 8`, `align_of::<Handle>() == 8`.
- **VERIFICATION**: `size_of::<Handle>()` vs `size_of::<*mut c_void>()`.
- **SOURCE**: rust-reference type-layout.html (transparent); rust-api-guidelines C-NEWTYPE.

## 3. Enum discriminants: size and values must both be pinned

- **RULE**: fieldless enums crossing FFI need a C-compatible representation
  (`#[repr(C)]` or an explicit `#[repr(u8/i8/u16/...)`), and their discriminant values
  must be written explicitly. A plain `enum` has unspecified layout.
- **WHY AI GETS IT WRONG**: "an enum is just an int" — a plain Rust `enum` is not
  FFI-safe (rustc warns `improper_ctypes_definitions`), and `#[repr(C)]` enums default
  to a discriminant type that only matches C `int` while discriminants fit.
- **CORRECT REASONING**: `#[repr(C)] enum` uses the C `int` size (4 bytes, align 4) when
  all discriminants fit in `int`; discriminants above `INT_MAX`/below `INT_MIN` make it
  non-portable — rustc 1.97 warns `repr_c_enums_larger_than_int` (future-incompatible,
  issue #124403). For a fixed wire size use `#[repr(u8)]` etc. Always write `= 0, = 1`
  so values survive edits.
- **EXAMPLE** (bad):
  ```rust
  enum Status { Ok, Busy, Error }   // unspecified layout; passing it to C is not FFI-safe
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  #[repr(C)]
  pub enum Status { Ok = 0, Busy = 1, Error = 2 }  // size 4, matches C enum
  ```
- **VERIFICATION**: `size_of::<Status>()` on the Rust side vs `sizeof(enum status)` in C
  (both 4); compile the plain enum and read the `improper_ctypes_definitions` warning.
- **SOURCE**: rust-reference type-layout.html (repr(C) enums); rust-reference items/enumerations.html.

## 4. bool is 1 byte and only holds 0 or 1

- **RULE**: `bool` is FFI-safe and 1 byte with alignment 1; a C `_Bool` is also 1 byte.
  A `bool` value crossing the boundary must be exactly 0 or 1.
- **WHY AI GETS IT WRONG**: agents assume `bool` is `int`-sized or that any nonzero
  byte means `true`.
- **CORRECT REASONING**: writing any byte other than 0/1 into a `bool` (e.g. by copying
  a raw byte from C) creates an invalid value; constructing a `bool` from an invalid
  value is UB. In structs, `{ bool, u32 }` is `size 8, v at offset 4` on both sides —
  verified identical in Rust and C.
- **EXAMPLE** (bad): C writes `buf[0] = 2;` into a `bool` field; Rust reads garbage
  instead of `true`.
- **COUNTEREXAMPLE** (good): C stores exactly `0`/`1`; Rust validates with
  `if raw != 0 && raw != 1 { return Err(...) }` before building a `bool`.
- **VERIFICATION**: `size_of::<bool>() == 1`; `-fsanitize=bool` or Miri flags invalid bools.
- **SOURCE**: rust-reference type-layout.html (primitive layouts); rustonomicon FFI chapter.

## 5. Rust `char` has no C equivalent; use c_char/u32

- **RULE**: Rust `char` (4-byte Unicode scalar value) is NOT FFI-safe. For bytes crossing
  the boundary use `c_char`/`c_uchar` (1 byte); for code points use `u32`.
- **WHY AI GETS IT WRONG**: "char is a character in both languages" — C `char` is 1 byte,
  Rust `char` is 4 bytes; rustc rejects `char` in `extern "C"` signatures with
  `improper_ctypes_definitions` ("the `char` type has no C equivalent").
- **CORRECT REASONING**: C strings are byte buffers, so map them to `*const c_char`
  (`CStr`) and decode in Rust; never pass Rust `char`.
- **EXAMPLE** (bad): `pub extern "C" fn take_char(c: char) -> i32` — warning at compile,
  ABI mismatch at runtime.
- **COUNTEREXAMPLE** (good): `pub extern "C" fn take_byte(c: c_uchar) -> i32` or
  `code_point: u32`.
- **VERIFICATION**: compile the `char` version and read the warning; the `c_uchar`
  version compiles clean.
- **SOURCE**: rust-reference items/functions.html (extern blocks); rustonomicon FFI chapter.

## 6. extern "C" blocks and calling conventions

- **RULE**: foreign declarations live in `extern "C" { ... }` blocks; exported functions
  are `pub extern "C" fn`. The calling convention must match the platform C ABI:
  SysV AMD64 (Linux/macOS x86-64), Microsoft x64, AAPCS64, RISC-V psABI, etc.
- **WHY AI GETS IT WRONG**: agents assume "C is C" — e.g. that `extern "C"` behaves the
  same on 32-bit Windows (where it is `cdecl` and `extern "system"`/`"stdcall"` differ),
  or that structs passed by value always go in registers.
- **CORRECT REASONING**: on x86-64 the C convention is unified, but register/stack
  argument placement and struct-classification rules are still platform-specific (SysV
  §3.2, Microsoft x64 ABI, AAPCS64 §6). `extern "C"` selects the platform default C
  convention; anything else (`extern "stdcall"`, `"sysv64"`, `"aapcs"`) is an explicit
  override that must match the other side exactly.
- **EXAMPLE** (bad): declaring a Windows `__stdcall` API as `extern "C"` and passing
  arguments that the stdcall prologue mis-reads.
- **COUNTEREXAMPLE** (good): use `extern "C"` for the platform default C ABI, or the
  explicit matching ABI, and cross-check argument placement in disassembly.
- **VERIFICATION**: compare generated asm (`rustc -C opt-level=2 --emit=asm`) with the
  C side for the same signature; see `abi-layout-reasoning` for the classification rules.
- **SOURCE**: sysv-amd64-abi §3.2 (x86-64); aapcs64 §6; rust-reference items/functions.html (Abi).

## 7. Exported symbols need no_mangle and stable names

- **RULE**: Rust symbols are mangled; an exported FFI function needs `#[no_mangle]` (or
  `#[export_name = "..."]`). Symbol names must not collide across the linked binary.
- **WHY AI GETS IT WRONG**: agents link a Rust lib into C and get "unresolved symbol"
  because the mangled name does not match; or use `#[no_mangle]` on everything and hit
  collisions.
- **CORRECT REASONING**: `#[no_mangle] pub extern "C" fn` produces a C-visible symbol
  with the exact function name. In edition 2024 this becomes `#[unsafe(no_mangle)]`.
  Every `#[no_mangle]` symbol is global: keep names namespaced (e.g. `libname_op`).
- **EXAMPLE** (bad): linking fails with `undefined reference to `foo`` or duplicated
  symbols from two `#[no_mangle] fn foo`.
- **COUNTEREXAMPLE** (good): `#[no_mangle] pub extern "C" fn token_new(...)` and a
  matching declaration in the C header.
- **VERIFICATION**: `nm`/`dumpbin` the built object and confirm the symbol name;
  link the C side against it.
- **SOURCE**: rust-reference items/functions.html (no_mangle); rustonomicon FFI chapter.

## 8. CString/CStr: borrow vs take ownership, exactly once

- **RULE**: `CStr::from_ptr(p)` borrows a NUL-terminated C string (caller keeps
  ownership, Rust never frees). `CString::from_raw(p)` TAKES ownership of a buffer
  that must have come from `CString::into_raw` (or an equivalent Rust allocation) and
  must be consumed exactly once. `CString::into_raw` hands ownership of a
  NUL-terminated buffer to the other side.
- **WHY AI GETS IT WRONG**: agents call `from_raw` on C-allocated pointers, or call
  `from_raw` twice — both are double-free/allocator-mismatch UB. The two functions
  compile fine; the contract is soundness, not syntax.
- **CORRECT REASONING**: every pointer has one owner. If C allocated it, Rust borrows
  it (`from_ptr`) and C frees it. If Rust allocated it via `CString::into_raw`, exactly
  one `from_raw` reconstructs and drops it. `CStr::to_string_lossy()` is the safe way
  to read borrowed bytes.
- **EXAMPLE** (bad):
  ```rust
  let a = CString::from_raw(p);
  let b = CString::from_raw(p);  // second owner of the same buffer: double free
  drop(a); drop(b);
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  // C allocated p and will free it: borrow only, never free in Rust.
  let s = CStr::from_ptr(p);
  // Rust allocated: exactly one from_raw/into_raw pair.
  let c = CString::new("x").unwrap();
  let p = c.into_raw();
  let back = CString::from_raw(p);   // once
  ```
- **VERIFICATION**: ASan/Valgrind across the boundary; Miri (`cargo +nightly miri test`)
  flags double `from_raw` and wrong-allocator frees.
- **SOURCE**: rust-reference type-layout.html and std docs (CStr/CString); rustonomicon FFI ownership.

## 9. Box::into_raw / Box::from_raw move heap objects across the boundary

- **RULE**: to hand a Rust heap object to C, `Box::into_raw(Box::new(x))` returns a
  non-null `*mut T`; the object is later reclaimed with exactly one `Box::from_raw(p)`
  (or a leak if C never returns it). `from_raw` requires a pointer produced by
  `into_raw` from the same Rust allocator.
- **WHY AI GETS IT WRONG**: agents pass `&x as *const _` to C and drop `x` (use-after-free
  when C uses it later), or reconstruct with `Box::from_raw` on a C-malloc'd pointer
  (invalid free with the Rust allocator).
- **CORRECT REASONING**: `into_raw` converts a `Box` into a raw pointer without running
  `Drop`, transferring the single ownership to C. The symmetric `from_raw` is the only
  valid way to get the `Box` back. Null must be handled explicitly: `into_raw` never
  returns null; use `std::ptr::null_mut()` as the "no object" sentinel if needed.
- **EXAMPLE** (bad):
  ```rust
  let b = Box::new(Token::default());
  let p: *mut Token = &*b;   // borrow, not transfer; b still drops -> dangling p
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  #[no_mangle] pub extern "C" fn token_new() -> *mut Token {
      Box::into_raw(Box::new(Token::default()))
  }
  #[no_mangle] pub unsafe extern "C" fn token_free(p: *mut Token) {
      if !p.is_null() { unsafe { drop(Box::from_raw(p)) } }   // exactly once per token_new
  }
  ```
- **VERIFICATION**: ASan/Valgrind/Miri for leaks, double-frees, and foreign frees.
- **SOURCE**: rust-reference behavior-considered-undefined.html (aliasing); rustonomicon FFI ownership.

## 10. Opaque handles: pointer to a private struct, explicit create/destroy

- **RULE**: the safe FFI shape for a persistent object is an opaque handle: a
  `#[repr(transparent)]` newtype or `*mut` to a private struct, created and destroyed
  only through the exported API. C never sees the fields.
- **WHY AI GETS IT WRONG**: agents export the inner struct definition, or use
  `usize`/`void*` casts as handles, losing type safety and inviting size mismatches
  and wrong-pointer reuse.
- **CORRECT REASONING**: opaque handles keep the representation private and the layout
  stable across releases; `create`/`destroy` pairs pin ownership on the Rust side while
  C only stores and passes the pointer. Null is the documented "no handle" value.
- **EXAMPLE** (bad): `typedef void* handle_t;` in C with no Rust-side type, so a
  function-pointer or int can be passed by mistake.
- **COUNTEREXAMPLE** (good): `pub struct Token { ... }` private fields;
  `token_new`/`token_free` exported; C sees only `token_t*`.
- **VERIFICATION**: the handle is 1 pointer (size 8); create/destroy under ASan shows no
  leaks and no double-free; passing a bogus handle is caught by validity checks.
- **SOURCE**: matklad-preconditions (handle types); rust-api-guidelines C-NEWTYPE; rustonomicon FFI chapter.

## 11. Callbacks: extern "C" fn + user-data pointer

- **RULE**: a callback crossing into C must be an `extern "C" fn` (with matching
  signature and calling convention) plus an opaque context pointer that carries state.
  A Rust closure is NOT a function pointer.
- **WHY AI GETS IT WRONG**: agents pass a capturing closure or `|| ...` to C; closures
  with captures have no stable code address and cannot coerce to `extern "C" fn`
  (rustc error E0308: "expected fn pointer, found closure").
- **CORRECT REASONING**: state travels through the context pointer: C's API takes
  `cb, void* user`; Rust registers an `extern "C" fn` that casts `user` back to the
  context type. Non-capturing fn items (plain `fn`s) CAN coerce to fn pointers, but the
  `extern "C"` ABI must still be declared.
- **EXAMPLE** (bad):
  ```rust
  let n = 7;
  let cb = |x: i32| { let _ = x + n; };   // captures n
  register_cb(cb);                        // E0308: not a fn pointer
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  type VisitFn = extern "C" fn(key: *const c_char, value: c_longlong, user: *mut c_void);
  pub unsafe extern "C" fn on_visit(_key: *const c_char, value: c_longlong, user: *mut c_void) {
      let ctx = unsafe { &mut *(user as *mut Visitor) };
      ctx.sum += value;
  }
  ```
  with the context pointer supplied by C on every call.
- **VERIFICATION**: compile the closure version and record E0308; run the fn-pointer
  version under ASan to confirm the context pointer round-trips.
- **SOURCE**: rust-reference type-layout.html and items/functions.html; rustonomicon FFI callbacks.

## 12. Panics must not unwind through extern "C"

- **RULE**: a Rust panic that unwinds through an `extern "C"` frame is UB. Export a
  boundary function whose body is `catch_unwind`; translate the panic to an error code.
- **WHY AI GETS IT WRONG**: "panic=abort fixes it" (only if the whole crate graph is
  built with it, and it still kills the process); or "nobody calls this with bad input".
  rustc compiles `extern "C" fn` that panics with NO warning on 1.97 — it is silent UB.
- **CORRECT REASONING**: with `panic=unwind`, a panic in an `extern "C"` function hits
  `panic_cannot_unwind` and aborts (verified: process exits with status 0xC0000409 and
  prints "panic in a function that cannot unwind"); from a real C caller the behavior is
  unspecified. Wrap the whole exported body in `catch_unwind(AssertUnwindSafe(...))` and
  return an error code on `Err`.
- **EXAMPLE** (bad):
  ```rust
  #[no_mangle] pub extern "C" fn run(v: c_int) -> c_int {
      if v < 0 { panic!("negative"); }   // unwinds through extern "C" -> abort/UB
      v
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  #[no_mangle] pub extern "C" fn run(v: c_int) -> c_int {
      match panic::catch_unwind(AssertUnwindSafe(|| work(v))) {
          Ok(code) => code,
          Err(_) => -1,
      }
  }
  ```
- **VERIFICATION**: run the bad binary (`examples/bad/run_bad.rs`) and record the abort
  and exit code; run the good version and confirm the error code path returns -1.
- **SOURCE**: rust-reference items/functions.html (unwinding); rustonomicon Unwinding chapter.

## 13. unsafe fn needs a written safety contract

- **RULE**: every `unsafe fn` at the boundary documents its preconditions — who owns the
  pointer, what validity/alignment is required, what the callee may do with the memory.
- **WHY AI GETS IT WRONG**: agents put `unsafe` on everything and nothing, treating it as
  a performance annotation instead of a soundness contract.
- **CORRECT REASONING**: `unsafe fn` pushes the responsibility to the caller; the
  contract (often in a `// Safety:` comment) is what makes a call sound. `unsafe` blocks
  inside should be as small as the contract allows, not wrapping whole functions.
- **EXAMPLE** (bad): `pub unsafe fn peek(p: *const c_char)` with no documented
  NUL-termination or lifetime contract.
- **COUNTEREXAMPLE** (good): the contract states "p must point to a NUL-terminated
  buffer valid for the call; the result borrows it", and the body checks null first.
- **VERIFICATION**: Miri on the caller; review the safety comments against each
  precondition; clippy `missing_safety_doc`.
- **SOURCE**: rust-reference unsafety.html; rust-api-guidelines; clippy-lints missing_safety_doc.

## Quick decision table

| Situation | Correct rule |
|---|---|
| struct over boundary | `#[repr(C)]` + `size_of`/`offsetof` verified both sides |
| newtype handle | `#[repr(transparent)]` over one pointer/field |
| enum over boundary | `#[repr(C)]` or `#[repr(u8/...)` with explicit `= N` discriminants |
| byte/char data | `c_char`/`c_uchar`/`u32`; never Rust `char` |
| C string | `CStr::from_ptr` borrow, or one `into_raw`/`from_raw` pair |
| heap object to C | `Box::into_raw` / exactly one `Box::from_raw` |
| persistent object | opaque handle + explicit create/destroy |
| callback | `extern "C" fn` + user-data context pointer; no closures |
| Rust panic risk | `catch_unwind` inside every `extern "C"` export |
| exported symbol | `#[no_mangle]` (edition 2024: `#[unsafe(no_mangle)]`) + unique name |
