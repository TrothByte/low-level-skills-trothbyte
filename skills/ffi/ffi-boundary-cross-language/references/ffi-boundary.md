# FFI Boundary — Reference

Sources: Rustonomicon FFI; Rust Reference (repr/type-layout); C++ Core Guidelines I.*;
SysV AMD64 / AAPCS64; Itanium C++ ABI. Format: RULE → WHY AI GETS IT WRONG →
CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. Layout must be pinned, not assumed

- **RULE**: cross-language structs must use a C-compatible layout (`repr(C)` in Rust,
  plain struct in C, `extern "C"`/standard layout in C++). `bool`, `enum`, and `char`
  sizes differ per language/compiler.
- **WHY AI GETS IT WRONG**: "both are structs, so they're compatible".
- **CORRECT REASONING**: Rust `bool` is 1 byte but C `_Bool` may differ in ABI contexts;
  Rust enums have no fixed size unless discriminants are explicit; C++ `bool` vs C.
  Verify with `size_of`/`offsetof`, never assume.
- **EXAMPLE** (correct): `#[repr(C)] struct Header { u32 magic; u8 ver; u8 _pad[2]; u32 len; }`.
- **COUNTEREXAMPLE** (bad): plain `#[derive]` struct passed to C — layout not guaranteed.
- **VERIFICATION**: print sizes on both sides; `llvm-readobj --file-headers` not needed —
  `offsetof` on both.
- **SOURCE**: Rust Reference type-layout; Rustonomicon FFI.

## 2. Ownership transfer must be explicit

- **RULE**: for every pointer crossing the boundary, document who allocates and who frees.
  Foreign drop (Rust dropping a C-allocated pointer) and C freeing a Rust-allocated pointer
  are both bugs unless the allocator is shared (malloc on both sides).
- **WHY AI GETS IT WRONG**: "Rust will clean up automatically" (Drop runs!) or "C owns it"
  with nobody freeing.
- **CORRECT REASONING**: Rust `Drop` runs on the value; if the memory was C-malloc'd and
  Rust deallocates with the Rust allocator, that's a double-free/invalid-free. Either use
  `Box::from_raw`/`into_raw` deliberately, or keep C ownership and never let Rust drop it.
- **EXAMPLE** (bad): a C `char*` returned to Rust wrapped in `CString::from_raw` twice (double-free).
- **COUNTEREXAMPLE** (good): document "C allocates, C frees"; Rust uses `CStr` borrowed, no drop.
- **VERIFICATION**: ASan/Valgrind across the boundary; Miri (Rust).
- **SOURCE**: Rustonomicon FFI ownership; C++ CG R.3/R.37.

## 3. No unwinding across the boundary

- **RULE**: a Rust panic unwinding through `extern "C"` (or C++ exception crossing a C
  frame) is UB. Boundaries must be `catch_unwind`/`noexcept`.
- **WHY AI GETS IT WRONG**: "Rust panics abort only in some configs" — with
  `panic=unwind` (default), unwinding past an FFI frame is UB.
- **CORRECT REASONING**: wrap the exported function body in `catch_unwind` and translate the
  panic to an error code; compile C++ exported functions with `noexcept`.
- **EXAMPLE** (correct): `extern "C" fn exported() -> i32 { match catch_unwind(|| work()) { Ok(_) => 0, Err(_) => -1 } }`.
- **COUNTEREXAMPLE** (bad): `extern "C" fn exported() { panic!("boom"); }`.
- **VERIFICATION**: run the panic case — with `panic=unwind` it is UB (abort may or may not
  happen); fix to catch_unwind.
- **SOURCE**: Rustonomicon Unwinding; Rust Reference unwinding; C++ [except.handle].

## 4. Callbacks and function pointers

- **RULE**: callback types must match the calling convention and ABI exactly; state must be
  passed through a user-data pointer, not captured.
- **WHY AI GETS IT WRONG**: "callbacks just work"; missing `extern "C"` on a Rust callback
  passed to C.
- **CORRECT REASONING**: Rust closures aren't C function pointers; wrap as
  `extern "C" fn(data: *mut c_void)` + context pointer.
- **EXAMPLE** (good): pass a `extern "C" fn(*mut c_void)` plus an opaque context pointer.
- **VERIFICATION**: callback invokes correctly with context; no capture in the fn pointer.
- **SOURCE**: Rustonomicon FFI callbacks; C++ CG I.11.

## 5. String and slice ownership

- **RULE**: C strings crossing into Rust are `CStr` (borrowed) or `CString::from_raw`
  (takes ownership); never both. Slices need length + ownership contract.
- **WHY AI GETS IT WRONG**: "I'll just make a CString from a C pointer".
- **CORRECT REASONING**: `CStr::from_ptr` borrows (caller keeps ownership); `from_raw`
  transfers ownership and MUST be freed with `into_raw` exactly once.
- **EXAMPLE** (bad): `CString::from_raw(p)` then later `CString::from_raw(p)` again — double free (A21).
- **COUNTEREXAMPLE** (good): one `from_raw`/`into_raw` pair, or borrow with `from_ptr`.
- **VERIFICATION**: ASan; Miri; run twice.
- **SOURCE**: Rust std::ffi docs; Rustonomicon FFI.

## Quick decision table

| Situation | Correct rule |
|---|---|
| struct over boundary | `repr(C)` + `offsetof` verification both sides |
| pointer ownership | single owner; explicit create/destroy |
| Rust panic risk | `catch_unwind` in `extern "C"` export |
| C++ exception risk | `noexcept` on exported functions |
| callback | `extern "C" fn` + user-data pointer |
| C string | `CStr` borrow or single `CString` ownership |
