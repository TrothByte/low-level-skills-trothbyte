# Rust Unsafe Semantics: aliasing, validity, provenance, UB

Knowledge layer for `rust-unsafe-reasoning`. Each rule follows the format
RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.
Uncertainty is marked KNOWN / INFERRED / UNVERIFIED (repo rule: never silently claim).

Compiles against: rustc 1.97.1 (2026-07-14). Target verification: Miri.

## 1. The "behavior considered undefined" list

- **RULE**: The Rust Reference enumerates the undefined behaviors. Compiling is not a
  correctness check: rustc does not validate unsafe contracts, and UB may stay silent until
  the optimizer or another target exploits it. The list covers: data races; dereferencing a
  null or dangling pointer; reading uninitialized memory; breaking the pointer aliasing rules;
  producing an invalid value (bool not 0/1, null/misaligned/dangling reference, invalid enum
  discriminant, invalid char, fn pointer not from a fn item, value of type `!`); creating a
  `&str` with invalid UTF-8; unwinding across an `extern "C"` (non-unwind) boundary.
  IMPORTANT: unlike C, integer overflow in Rust is NOT UB — it wraps (two's complement) when
  `overflow-checks` are off and panics when they are on.
- **WHY AI GETS IT WRONG**: carries over C intuition — "signed overflow is UB" (false in Rust)
  and "if it runs, it is fine" (false in unsafe).
- **CORRECT REASONING**: check each construct against the normative list, not against what the
  program happens to do. A debug run cannot prove absence of UB.
- **EXAMPLE** (bad):
  ```rust
  let raw = Box::into_raw(Box::new(1u32));
  unsafe { drop(Box::from_raw(raw)); } // frees the allocation
  let _ = unsafe { *raw };             // UB: dangling dereference (may "work" on some targets)
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  let raw = Box::into_raw(Box::new(1u32));
  let b = unsafe { Box::from_raw(raw) }; // one from_raw per into_raw
  println!("{}", *b);
  ```
- **VERIFICATION**: `cargo +nightly miri run` — Miri flags dangling deref, UAF, uninit reads,
  invalid values, and races. rustc does not.
- **SOURCE**: rust-reference (behavior-considered-undefined.html); rustonomicon (meet-safe-and-unsafe); cwe (CWE-416, CWE-457)

## 2. Validity invariants vs initialization state

- **RULE**: every type has a validity invariant — the set of bit patterns that constitute a
  valid value. `bool` must be 0 or 1, `char` a Unicode scalar value, references non-null and
  aligned, enum discriminants valid. Initialization state is orthogonal: a location is
  initialized once it has been written, and reading uninitialized memory is UB for every type,
  even `u8`. Producing a value that violates the validity invariant is a separate, additional
  UB.
- **WHY AI GETS IT WRONG**: "`u8` is just bytes, so uninitialized is harmless" (uninit read is
  UB), and "transmute into a `bool` is fine because I control the bytes" (invalid value is UB).
- **CORRECT REASONING**: validity governs what counts as a value; initialization governs whether
  a location has been written. `MaybeUninit<T>` defers the initialized-state check to you;
  `assume_init` checks neither — both are the caller's responsibility.
- **EXAMPLE** (bad):
  ```rust
  let b: bool = unsafe { std::mem::transmute(2u8) }; // UB: invalid bool value (bit pattern 2)
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  let b = 1u8 != 0; // well-defined construction
  ```
- **VERIFICATION**: Miri reports "invalid value" (validity) and "uninitialized memory"
  (init state) as distinct errors.
- **SOURCE**: rust-reference (behavior-considered-undefined.html, type-layout.html); rustonomicon (transmute)

## 3. Pointer aliasing rules

- **RULE**: while a location is accessible through a live `&mut` (or a reference/raw pointer
  derived from it), no other access — including another `&mut` or raw pointer — may write to
  (or, for `&mut`, read from) that location. Safe code enforces this statically; unsafe code
  must maintain it manually. Two raw pointers derived from two different `&mut` to the same
  location are an aliasing violation if both are used while live.
- **WHY AI GETS IT WRONG**: "raw pointers are C pointers, so I can do whatever I want" —
  aliasing rules apply to raw pointers too, and the optimizer assumes exclusivity (LLVM
  `noalias`-style reasoning), so a write through `p` may justify reordering reads that ignore `q`.
- **CORRECT REASONING**: create raw pointers from `&mut`/`&`, keep exactly one mutable channel
  live per location, and re-derive rather than cache stale pointers. Prefer slices +
  `split_at_mut` over manual pointer juggling.
- **EXAMPLE** (bad):
  ```rust
  let mut x = 5u32;
  let a: *mut u32 = &mut x; // safe code would reject this pattern (E0499)
  let b: *mut u32 = &mut x;
  unsafe { *a = 10; *b = 20; } // two aliasing mutable channels to x
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  let mut x = 5u32;
  let p: *mut u32 = &mut x;
  unsafe { *p = 10; } // one channel; x is used only after the borrow ends
  println!("{}", x);
  ```
- **VERIFICATION**: Miri — KNOWN flagged under Stacked Borrows (the second `&mut` retags and
  invalidates the first pointer's tag). Under Tree Borrows (`-Zmiri-tree-borrows`) the verdict
  depends on whether the `&mut`s are dead — model-dependent, INFERRED.
- **SOURCE**: rust-reference (behavior-considered-undefined.html — "breaking the pointer aliasing rules"); rustonomicon (meet-safe-and-unsafe); llvm-langref (noalias)

## 4. Stacked Borrows / Tree Borrows model

- **RULE**: Miri operationalizes aliasing with Stacked Borrows (default) or Tree Borrows
  (`-Zmiri-tree-borrows`). References and raw pointers carry tags; creating a new `&mut`
  retags the location and invalidates older tags; raw pointers keep the tag they had at cast
  time. Access through an invalidated tag is UB. These are models, not the normative spec, but
  they are the closest operational semantics and match optimizer behavior.
- **WHY AI GETS IT WRONG**: agents reason about address equality instead of access history —
  e.g. caching a raw pointer and using it after creating a new `&mut` elsewhere.
- **CORRECT REASONING**: treat a raw pointer as valid only between the last use of the `&mut`
  it derives from and the first conflicting access. Do not interleave `&mut`-based and
  raw-pointer-based access to the same memory.
- **EXAMPLE** (bad):
  ```rust
  let mut v = vec![1u32, 2, 3];
  let p = v.as_mut_ptr();
  v.push(4);          // may realloc and retag; p's tag is invalidated either way
  unsafe { *p = 0; }  // UB: stale pointer (tag invalidated, possibly stale address)
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  let mut v = vec![1u32, 2, 3];
  {
      let p = v.as_mut_ptr();
      unsafe { *p = 0; } // p used within the &mut borrow's scope
  }
  v.push(4);          // borrow ended; safe to mutate again
  ```
- **VERIFICATION**: `cargo +nightly miri run`; `-Zmiri-tree-borrows` to compare models.
  Model verdicts for edge cases are INFERRED; the reference UB list is KNOWN.
- **SOURCE**: rust-reference (behavior-considered-undefined.html); rustonomicon (meet-safe-and-unsafe); llvm-langref (noalias)

## 5. Pointer provenance and strict provenance

- **RULE**: a pointer carries provenance — which allocation it points into. Dereferencing a
  pointer whose provenance does not cover the accessed location is UB even if the address bits
  happen to be "correct". Casting an integer address to a pointer creates a pointer with no
  provenance. The strict-provenance APIs (`with_addr`, `from_addr`) are the future-proof path.
  In-bounds arithmetic preserves provenance; out-of-bounds `offset` is UB before any dereference.
- **WHY AI GETS IT WRONG**: agents do pointer math on integers (`(p as usize + k) as *mut T`)
  "because the address is correct", losing provenance; or hand-roll tagged pointers.
- **CORRECT REASONING**: keep pointers derived from the allocation (base pointer + `add`) and
  reserve integer↔pointer casts for genuinely necessary OS/FFI work. When you must cast,
  re-establish provenance with `with_addr` and keep the allocation alive.
- **EXAMPLE** (bad):
  ```rust
  let buf = [1u8, 2, 3, 4];
  let addr = buf.as_ptr() as usize + 2;
  let p = addr as *const u8;    // provenance lost
  println!("{}", unsafe { *p }); // UB: no provenance over buf
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  let buf = [1u8, 2, 3, 4];
  let p = unsafe { buf.as_ptr().add(2) }; // provenance preserved, in-bounds
  println!("{}", unsafe { *p });
  ```
- **VERIFICATION**: Miri with `-Zmiri-strict-provenance` flags integer-derived pointers
  (by default Miri is permissive). Clippy lints the `as` casts.
- **SOURCE**: rust-reference (expressions/operator-expr — pointer arithmetic requirements); rustonomicon (meet-safe-and-unsafe); clippy-lints

## 6. transmute requirements

- **RULE**: `mem::transmute::<A, B>` requires (1) equal sizes — else compile error E0512
  "cannot transmute between types of different sizes, or dependently-sized types"; (2) the
  resulting bit pattern must satisfy B's validity invariant — else UB; (3) for reference /
  pointer destinations, the address must be a valid reference (non-null, aligned for B, not
  dangling). It copies bytes; it is not a `as`-style value conversion and does not relax
  aliasing or validity.
- **WHY AI GETS IT WRONG**: "transmute is the generic version of `as`" (it is byte
  reinterpretation, not value conversion); "wrong size will just truncate" (it is a compile
  error); "transmute between references is fine" (alignment and validity still apply).
- **CORRECT REASONING**: use `transmute` only when sizes provably match and the destination's
  validity invariant is guaranteed by the data (e.g. `u32` ↔ `[u8; 4]`). Prefer
  `from_ne_bytes`/`to_ne_bytes` or `ptr::copy_nonoverlapping` where they express intent better.
  Add a static `assert_eq!(size_of::<A>(), size_of::<B>())` for maintenance safety.
- **EXAMPLE** (bad):
  ```rust
  let n: u32 = 42;
  let _b: [u8; 3] = unsafe { std::mem::transmute(n) }; // E0512: sizes differ (4 vs 3)
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  use std::mem;
  let n: u32 = 0x1122_3344;
  assert_eq!(mem::size_of::<u32>(), mem::size_of::<[u8; 4]>());
  let bytes: [u8; 4] = unsafe { mem::transmute(n) }; // ok: same size, all bytes valid u8
  println!("{:02x?}", bytes);
  ```
- **VERIFICATION**: rustc raises E0512 for size mismatch; Miri flags validity violations at
  runtime; clippy `wrong_transmute` flags misuse.
- **SOURCE**: rust-reference (std transmute docs, behavior-considered-undefined.html); rustonomicon (transmute); clippy-lints

## 7. Raw pointer arithmetic

- **RULE**: `ptr::offset` and `ptr::add` require the resulting pointer to stay inside the same
  allocation as the original (one-past-the-end may be formed, but dereferencing it is UB).
  Computing an out-of-bounds pointer is UB even if never dereferenced. `wrapping_add` /
  `wrapping_offset` never cause UB by themselves, but dereferencing an out-of-bounds result
  still is UB.
- **WHY AI GETS IT WRONG**: "it is just integer arithmetic on addresses" — the model requires
  in-allocation bounds; `add(len)` on a full slice and last-element arithmetic are classic errors.
- **CORRECT REASONING**: compute against slice/Vec bounds, clamp before `add`, and prefer
  `get_unchecked`/`get_unchecked_mut` with a verified index over manual `offset`.
- **EXAMPLE** (bad):
  ```rust
  let a = [1u32, 2, 3];
  let p = a.as_ptr();
  unsafe { let _ = p.add(3); let _ = *p.add(3); } // UB: deref of one-past-the-end
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  let a = [1u32, 2, 3];
  let p = a.as_ptr();
  for i in 0..a.len() {
      println!("{}", unsafe { *p.add(i) }); // every add stays in-bounds
  }
  ```
- **VERIFICATION**: Miri flags out-of-bounds pointer computation and deref.
- **SOURCE**: rust-reference (expressions/operator-expr, behavior-considered-undefined.html); rustonomicon (meet-safe-and-unsafe)

## 8. Uninitialized memory and MaybeUninit

- **RULE**: reading uninitialized memory is UB for every type. `MaybeUninit<T>` lets you
  manage initialization manually: memory starts uninitialized, `write` / `as_mut_ptr().write()`
  initialize it, and `assume_init` yields a `T` — but calling it before the location is
  initialized (and valid) is UB. `Vec::set_len(n)` likewise claims `n` elements are
  initialized; with fewer written elements, later reads or drops are UB.
- **WHY AI GETS IT WRONG**: "capacity is reserved, so the memory exists" (existence is not
  initialization); "I will write 3 elements and set_len(4)" (one uninitialized slot);
  "MaybeUninit is a wrapper around a default value".
- **CORRECT REASONING**: after every allocation/`set_len`, initialize all elements before the
  first read or drop; pair every `MaybeUninit` with a proof of full initialization before
  `assume_init`; for arrays use `[MaybeUninit::uninit(); N]` and write every slot.
- **EXAMPLE** (bad):
  ```rust
  let mut v: Vec<u8> = Vec::with_capacity(4);
  unsafe { v.set_len(4); }  // claims 4 initialized elements
  println!("{:?}", v);      // UB: reads uninitialized bytes
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  use std::mem::MaybeUninit;
  let mut slots: Vec<MaybeUninit<u8>> = Vec::with_capacity(4);
  slots.resize_with(4, MaybeUninit::<u8>::uninit);
  for (i, s) in slots.iter_mut().enumerate() {
      s.write(i as u8); // every slot initialized
  }
  let v: Vec<u8> = unsafe { slots.into_iter().map(|s| s.assume_init()).collect() };
  println!("{:?}", v);
  ```
- **VERIFICATION**: Miri reports "uninitialized memory" reads. Historical instance:
  CVE-2020-36432 (registry evals.yaml: uninitialized memory drop via unsafe `fill_with`).
- **SOURCE**: rust-reference (behavior-considered-undefined.html); rustonomicon (unchecked-unwrapping); cwe (CWE-457)

## 9. ptr::read / ptr::write ownership semantics

- **RULE**: `ptr::read` copies the bits and moves the value out, leaving the source location
  uninitialized; `ptr::write` overwrites a location, initializing it. Using the location after
  `ptr::read` without a subsequent write, or dropping it, is a double-free; overwriting without
  a prior read is a leak. Ownership must be transferred exactly once per location.
- **WHY AI GETS IT WRONG**: "ptr::read is a byte copy like memcpy" (it is a move; the source
  is left logically uninitialized); "ptr::write is just assignment" (for non-Copy types it is
  a move-in that must match a previous move-out).
- **CORRECT REASONING**: for non-Copy types, treat `read`/`write` as moving ownership in/out;
  use them in pairs so every read is followed by a write (or the location is never used again)
  and every write is preceded by a read (or the location is uninitialized).
- **EXAMPLE** (bad):
  ```rust
  use std::ptr;
  let mut a = String::from("x");
  unsafe { ptr::read(&a); } // a's value moved out; a is logically uninitialized
  drop(a);                  // UB: dropping uninitialized String (double-free)
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  use std::ptr;
  let mut a = String::from("x");
  let mut b = String::from("y");
  unsafe {
      let tmp = ptr::read(&a);
      ptr::write(&mut a, ptr::read(&b)); // b moved out, a rewritten
      ptr::write(&mut b, tmp);           // a's old value moved into b
  }
  // a and b are initialized again; dropping them is sound
  ```
- **VERIFICATION**: Miri flags the double-free / use-after-move; ASan flags the heap
  corruption but does not model ownership — Miri is the target tool.
- **SOURCE**: rust-reference (std ptr docs); rustonomicon (unchecked-unwrapping); cwe (CWE-416)

## 10. Box::from_raw / Box::into_raw ownership

- **RULE**: `Box::into_raw` transfers ownership to the caller; the memory is unmanaged until
  handed back with `Box::from_raw`. `Box::from_raw` requires the pointer came from
  `Box::into_raw` (same allocator), is non-null and aligned, and ownership is returned exactly
  once. Two `from_raw` calls double-free; using the pointer after the `from_raw`-backed Box is
  dropped is use-after-free.
- **WHY AI GETS IT WRONG**: "the raw pointer keeps the allocation alive" (it does not — the
  Box dropped via from_raw frees it); "from_raw is like casting back".
- **CORRECT REASONING**: a raw pointer from `into_raw` is a lease: exactly one `from_raw` must
  follow, and never dereference after that Box is dropped. Reserve such round-trips for FFI.
- **EXAMPLE** (bad):
  ```rust
  let raw = Box::into_raw(Box::new(7u32));
  unsafe { drop(Box::from_raw(raw)); } // frees the allocation
  let _ = unsafe { *raw };             // UB: dangling pointer read
  ```
- **COUNTEREXAMPLE** (good):
  ```rust
  let raw = Box::into_raw(Box::new(7u32));
  let b = unsafe { Box::from_raw(raw) }; // ownership restored exactly once
  println!("{}", *b);
  ```
- **VERIFICATION**: Miri reports "pointer to freed allocation".
- **SOURCE**: rust-reference (std Box docs); rustonomicon (meet-safe-and-unsafe); cwe (CWE-416)

## 11. unsafe impl Send / Sync contracts

- **RULE**: `Send` means a value may be moved to another thread; `Sync` means `&T` may be
  shared across threads. Implementing either unsafely is a promise to the compiler that the
  fields allow it. `Rc` is `!Send` (non-atomic refcount), `Cell`/`RefCell` are `!Sync`
  (non-atomic access), raw pointers are neither. An incorrect unsafe impl turns otherwise-safe
  code into a data race (UB).
- **WHY AI GETS IT WRONG**: "unsafe impl is boilerplate to satisfy the compiler" — it is the
  whole safety contract; "my field is only used under a Mutex, so it is fine" (then the wrapper
  is Sync because of the Mutex, not the field).
- **CORRECT REASONING**: justify every field: atomics → Send+Sync; `Cell`/`RefCell` → Send,
  not Sync; `Rc` → neither; raw pointers → prove externally enforced synchronization. Prefer a
  safe newtype (`struct Shared(Arc<Mutex<T>>)`) over unsafe impls.
- **EXAMPLE** (bad): see `examples/bad/unsafe_send_sync.rs` — `unsafe impl Send` on an
  `Rc<Cell<u32>>` wrapper lets two threads do unsynchronized get/set on the same Cell and
  concurrent refcount drops: a data race (UB).
- **COUNTEREXAMPLE** (good):
  ```rust
  use std::sync::atomic::{AtomicU32, Ordering};
  use std::sync::Arc;
  let counter = Arc::new(AtomicU32::new(0));
  let mut handles = vec![];
  for _ in 0..2 {
      let c = Arc::clone(&counter);
      handles.push(std::thread::spawn(move || {
          c.fetch_add(1, Ordering::Relaxed);
      }));
  }
  for h in handles { h.join().unwrap(); }
  ```
- **VERIFICATION**: Miri data-race detection; TSan (heuristic for Rust); rust-api-guidelines
  C-SEND-SYNC (a type that is Send/Sync must not require unsafe to be used across threads).
- **SOURCE**: rust-reference (send-and-sync.html, behavior-considered-undefined.html); rust-api-guidelines (C-SEND-SYNC); rustonomicon (races)

## 12. Data races

- **RULE**: two or more threads concurrently accessing the same memory location, at least one
  access a write, without synchronization, is a data race — UB. This applies to `Cell` fields,
  `Rc` refcounts, and raw-pointer accesses, not just to `unsafe` code: unsafe `Send`/`Sync`
  impls are how such code becomes reachable from safe code.
- **WHY AI GETS IT WRONG**: "the OS will crash or tear" — UB is not a crash guarantee; "the
  compiler will not actually reorder" — it may.
- **CORRECT REASONING**: use `Arc` + `Mutex`/`RwLock` or atomics for shared state; keep unsafe
  `Send`/`Sync` impls for provably thread-safe types only, and audit them like unsafe blocks.
- **EXAMPLE** (bad): the `BadWrapper` case in section 11.
- **COUNTEREXAMPLE** (good): the `Arc<AtomicU32>` case in section 11.
- **VERIFICATION**: Miri data-race detection; TSan.
- **SOURCE**: rust-reference (behavior-considered-undefined.html); rustonomicon (races); rust-api-guidelines (C-SEND-SYNC)

## 13. Verification toolchain notes

- Miri (`cargo +nightly miri run`, `cargo +nightly miri test`) is the ground truth for the UB
  classes above: uninit reads, invalid values, dangling/UAF, aliasing, provenance
  (`-Zmiri-strict-provenance`), data races. `-Zmiri-tree-borrows` switches the aliasing model.
- rustc rejects only what it can prove statically: safe-code borrow errors (E0499/E0502),
  transmute size mismatches (E0512). Everything else needs Miri.
- ASan/TSan do not model Rust's aliasing/provenance; their silence is not a pass.
- Install Miri: `rustup component add --toolchain nightly miri`.
