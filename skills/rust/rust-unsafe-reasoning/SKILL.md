---
name: rust-unsafe-reasoning
description: Use when writing, reviewing, or debugging Rust code that uses unsafe blocks — raw pointers, transmute, MaybeUninit, Box::from_raw/into_raw, unsafe impl Send/Sync, or FFI-adjacent code — to reason about validity invariants, aliasing, and pointer provenance, and to detect undefined behavior that compiles and runs but is wrong.
---

# Rust Unsafe Reasoning (aliasing / validity / provenance)

## When to use

- Writing, reviewing, or debugging code containing `unsafe { }`, `unsafe fn`, or `unsafe impl`.
- Raw pointer operations: `*p`, `ptr::read` / `ptr::write`, `p.add(n)`, `p.offset(n)`, `cast`.
- Type punning with `mem::transmute` or raw-pointer/reference casts.
- `MaybeUninit`, `Vec::set_len`, manual allocation — anything touching uninitialized memory.
- Ownership transfer: `Box::from_raw` / `Box::into_raw`, `String::from_raw_parts`, `Rc::from_raw`.
- `unsafe impl Send` / `unsafe impl Sync` and other trait-safety contracts.
- Interpreting a Miri report, or diagnosing unsafe code that "works in practice".

## When not to use

- Pure safe Rust with no `unsafe` — the borrow checker already enforces aliasing there.
- Panic-safety / unwind-correctness of safe code — use `rust-panic-safety`.
- FFI layout and ABI (repr(C), CString, extern "C") — use `rust-ffi-boundary` / `ffi-boundary-cross-language`.
- Atomic ordering between threads in safe code — use `memory-ordering-reasoning`.
- C code — use `c-undefined-behavior` (note: C signed overflow is UB; Rust's is defined).

## What the agent often gets wrong

- "It compiles and the tests pass, so it is correct." rustc does not check unsafe contracts; UB can be silent, platform-dependent, and optimization-dependent.
- "transmute is just a bitcast." It requires equal sizes (else E0512) and a value valid for the destination type; it does not relax aliasing or validity.
- "A raw pointer keeps the data alive." `Box::into_raw` + early `Box::from_raw` frees the memory while the pointer still dangles.
- "Raw pointers are exempt from aliasing rules." Two `&mut` (or pointers derived from them) to the same live location is UB under the aliasing model.
- "set_len is fine if capacity is enough." `set_len(n)` with uninitialized elements makes later reads produce invalid values (UB).
- "unsafe impl Send/Sync is just a declaration." It is a contract; a wrong one creates data races.
- "Reading uninitialized memory gives garbage but is harmless." It is UB, not "some number".
- "ptr::read is like `*p`." `ptr::read` moves the value out, leaving uninitialized memory; dropping the location later is a double-free.
- "Signed integer overflow is UB like in C." Wrong: Rust wraps (defined) when `overflow-checks` are off and panics when they are on.

## How to reason correctly

1. Enumerate every unsafe operation and state its documented safety precondition before writing it.
2. Track the validity invariant of the destination type (`bool` must be 0/1, `&T` non-null and aligned, enum discriminant valid). Validity is not the same as initialization.
3. Track aliasing: exactly one live `&mut` (or a pointer derived from it) per location; a conflicting write invalidates other pointers.
4. Track initialization state byte-by-byte: every byte must be written before it is read.
5. Track provenance: a pointer may only access inside the allocation it was derived from; in-bounds arithmetic preserves provenance, out-of-bounds does not.
6. Track ownership of raw pointers: `from_raw` returns exactly one owner; losing the pointer leaks, double `from_raw` double-frees.
7. Prefer safe abstractions (`MaybeUninit` + full init, slices with checked indices) over raw `transmute` and hand-rolled pointer loops.
8. Verify with Miri before trusting any unsafe code; a debug run proves nothing.

## What to verify

- Miri clean: zero reports of UB, data races, invalid values, uninit reads, or dangling/UAF.
- Every unsafe block has a SAFETY comment stating the real preconditions that hold.
- No `set_len` / `reserve` + `set_len` path leaves bytes uninitialized before read or drop.
- No `transmute` between differently-sized types (E0512) or validity-incompatible types.
- No raw pointer is used after the allocation or the borrow it derives from ends.
- `unsafe impl Send/Sync` is justified field-by-field (Rc, Cell, raw pointers are the traps).
- Each `ptr::read` is paired with a `ptr::write` (or the location is never used again).

## How to verify

```
rustc --edition 2021 examples/good/maybe_uninit.rs -o g1.exe && ./g1.exe
rustc --edition 2021 examples/bad/set_len_uninit.rs -o b4.exe && ./b4.exe   # UB: use Miri
rustup component add --toolchain nightly miri
cargo +nightly miri run       # target verification: flags UB that rustc accepts
cargo +nightly miri test
```

Miri (`cargo +nightly miri run`) is the ground truth for aliasing, validity, and provenance
violations. rustc only rejects what the borrow checker can prove (safe-code `&mut` conflicts,
transmute size mismatch); everything else needs Miri. ASan/TSan do not model Rust aliasing.

## Where the knowledge comes from

- The Rust Reference — unsafety, "behavior considered undefined", type layout (rust-reference)
- The Rustonomicon — aliasing, transmute, unchecked unwrapping, FFI, races (rustonomicon)
- Rust API Guidelines — C-SEND-SYNC, unsafe-impl contracts (rust-api-guidelines)
- Miri (Ralf Jung et al.) — operational model for aliasing/validity/provenance, Stacked Borrows

## Related skills

- `rust-ffi-boundary` — repr(C) layout and ownership transfer across FFI (recommend)
- `rust-panic-safety` — unwinding and cleanup correctness (require of)
- `memory-ordering-reasoning` — atomics and synchronization ordering (require of)
- `safe-low-level-from-scratch` — positive safe-first path (recommend)
- `compiler-ub-assumptions` — why the optimizer exploits UB (recommend)
- `c-undefined-behavior` — the C analog; different overflow rules (cross-link)

## Evaluation

Historical CVE: CVE-2020-36432 (uninitialized memory drop via `fill_with`), CVE-2021-32714
(usize overflow) — per registry/evals.yaml.
Synthetic + adversarial: aliasing violation, transmute invalidity, provenance loss, dangling
Box pointer, uninit Vec, invalid unsafe impl Send.
False-positive: correct `MaybeUninit` / `transmute` / scoped raw pointer / `ptr::read`+`write`
code must NOT be flagged.
