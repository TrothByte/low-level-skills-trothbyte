# Atomics API Reference: C11 / C++20 / Rust

Sources (registry ids): `iso-c11-n1570` (C11 §6.7.2.4, §5.1.2.4, §7.17), `iso-cpp20-n4861`
(C++20 [atomics]), `rust-reference` (memory model; std `std::sync::atomic`), `intel-sdm`
(x86 locked ops, TSO), `arm-arm` (AArch64 memory model).

## 1. API mapping table

- **RULE**: C11 `<stdatomic.h>` on `_Atomic T`, C++20 `<atomic>` on `std::atomic<T>`, and
  Rust `std::sync::atomic::{AtomicBool, AtomicI32, ...}` express the same atomic operations.

| Operation | C11 | C++20 | Rust |
|---|---|---|---|
| load | `atomic_load(&x)` / `atomic_load_explicit(&x, mo)` | `x.load(mo)` | `x.load(Ordering::_)` |
| store | `atomic_store(&x, v)` / `atomic_store_explicit(&x, v, mo)` | `x.store(v, mo)` | `x.store(v, Ordering::_)` |
| fetch_add | `atomic_fetch_add(&x, n)` / `atomic_fetch_add_explicit(&x, n, mo)` | `x.fetch_add(n, mo)` | `x.fetch_add(n, Ordering::_)` |
| CAS | `atomic_compare_exchange_weak/strong` / `_explicit` | `x.compare_exchange_weak/strong(e, d, mo[, fail])` | `x.compare_exchange(cur, new, ok, fail) -> Result<T,T>` |
| lock-free | `atomic_is_lock_free(&x)` | `x.is_lock_free()` | guaranteed lock-free when available (Rust 1.9x+; see rule 7) |

Orders: `memory_order_relaxed/consume/acquire/release/acq_rel/seq_cst` (C11, C++20) ≡
`Ordering::Relaxed/Acquire/Release/AcqRel/SeqCst` (Rust; no consume). C++20 made the enum
scoped (`std::memory_order::relaxed`); the unscoped `memory_order_relaxed` names remain as
synonyms.

- **WHY AI GETS IT WRONG**: translates function names 1:1 and assumes the return types and
  parameter passing are identical across languages (e.g. treats Rust's `Result` as a bool).
- **CORRECT REASONING**: map semantics, not names. The only truly different surface is Rust's
  `compare_exchange`: `Ok(old)` on success, `Err(observed)` on failure — the failure payload
  plays the role of C/C++'s updated `expected`.
- **EXAMPLE** (bad): in Rust, `let _ = slot.compare_exchange(cur, new, Ordering::SeqCst,
  Ordering::SeqCst);` then reusing the stale `cur` on the failure path.
- **COUNTEREXAMPLE** (good):
  ```rust
  let mut cur = slot.load(Ordering::Relaxed);
  loop {
      match slot.compare_exchange(cur, new, Ordering::SeqCst, Ordering::SeqCst) {
          Ok(_) => break,
          Err(observed) => cur = observed, // refresh expected from the payload
      }
  }
  ```
- **VERIFICATION**: compile the same algorithm in all three languages; run identical test vectors.
- **SOURCE**: iso-c11-n1570 §7.17; iso-cpp20-n4861 [atomics.types.generic]; rust-reference.

## 2. Memory-order validity per operation

- **RULE**: acquire is valid only on loads (and CAS success/failure loads), release only on
  stores (and CAS success), acq_rel/seq_cst on read-modify-write operations. C11: acquire
  "applies only to load operations", release "only to store operations", acq_rel and seq_cst
  "only to read-modify-write operations". C++ `store` precondition: relaxed/release/seq_cst;
  `load` precondition: relaxed/acquire/seq_cst. Rust mirrors this and panics at runtime on an
  invalid `store` order ("there is no such thing as an acquire store").
- **WHY AI GETS IT WRONG**: passes `Acquire` to a store (or `Release` to a load) "to be safe".
- **CORRECT REASONING**: ordering class is tied to operation kind. A store produces no value
  for an acquire to consume; an acquire store is meaningless. Violations are precondition
  violations: UB in C/C++, a debug panic in Rust.
- **EXAMPLE** (bad): `flag.store(true, Ordering::Acquire);` — Rust panics; C/C++ UB.
- **COUNTEREXAMPLE** (good): `flag.store(true, Ordering::Release);` paired with
  `flag.load(Ordering::Acquire)`.
- **VERIFICATION**: run the Rust bad example (panic observed, exit 101); C/C++ UB is not
  diagnosed — audit manually.
- **SOURCE**: iso-c11-n1570 §7.17.3; iso-cpp20-n4861 [atomics.types.operations]; rust-reference.

## 3. Default memory order is seq_cst (C11, C++20); Rust has no default

- **RULE**: C11 functions without `_explicit` (and C++ member functions without an order
  argument) default to `memory_order_seq_cst`. Rust requires an explicit `Ordering` on every
  operation.
- **WHY AI GETS IT WRONG**: believes `atomic_load` is "the cheap relaxed path", or that all
  three languages share defaults.
- **CORRECT REASONING**: default = strongest. Want relaxed? Ask for it explicitly
  (`_explicit`, an argument, or Rust's mandatory argument). "It's default so it's fast" is wrong.
- **EXAMPLE** (bad): using `atomic_load_explicit(&x, memory_order_relaxed)` in a
  publish/consume protocol "for speed" — the required acquire edge disappears.
- **COUNTEREXAMPLE** (good): `atomic_load_explicit(&x, memory_order_acquire)`; reserve
  `relaxed` for counters where order is irrelevant.
- **VERIFICATION**: `gcc -O2 -S` / `rustc --emit=asm` per target; on x86 loads look identical
  (`mov`) — the difference is semantic and shows on AArch64 (`ldar`).
- **SOURCE**: iso-c11-n1570 §7.17; iso-cpp20-n4861 [atomics.types.operations]; rust-reference.

## 4. CAS `expected` is in-out

- **RULE**: `atomic_compare_exchange_*(obj, &expected, desired)` compares the value
  representation of `*obj` with `*expected`; on equality it stores `desired`, otherwise it
  loads `*obj` into `*expected`. The comparison is bitwise (memcmp-like). C++ and Rust have
  the same semantics (Rust: the `Err` payload is the observed value).
- **WHY AI GETS IT WRONG**: treats `expected` as read-only and reuses the pre-call assumption
  after a failure.
- **CORRECT REASONING**: after `false`/`Err`, the variable holds the *observed* value, not your
  assumption. If you need the assumed value for logic, copy it before the call.
- **EXAMPLE** (bad):
  ```c
  int expected = 0;
  if (atomic_compare_exchange_strong(slot, &expected, 1)) { /* won */ }
  else { decide_using(expected); } // expected was overwritten by the live value
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int expected = 0;
  while (!atomic_compare_exchange_weak(slot, &expected, 1)) {
      if (expected != 0) break;      // use the observed value, not the assumption
  }
  ```
- **VERIFICATION**: single-threaded run — print `*expected` after a forced failure.
- **SOURCE**: iso-c11-n1570 §7.17.7.4; iso-cpp20-n4861 [atomics.types.operations] (Note on
  `memcmp`/`memcpy` semantics); rust-reference.

## 5. Weak CAS may fail spuriously; strong must not

- **RULE**: `compare_exchange_weak` may return false even when values match (LL/SC reservation
  loss on AArch64/POWER). `compare_exchange_strong` must not fail spuriously. Rust has only
  the weak form (`compare_exchange`), so it must always sit in a retry loop.
- **WHY AI GETS IT WRONG**: uses a single unprotected `compare_exchange_weak` as a one-shot
  gate and claims it is a strong operation; or writes a strong-style loop for Rust where the
  strong form does not exist.
- **CORRECT REASONING**: weak is correct only inside a loop; strong is for one-shot
  transitions. In Rust, always loop and treat the `Err` payload as "value observed".
- **EXAMPLE** (bad):
  ```c
  _Atomic int gate = 0;
  if (atomic_compare_exchange_weak(&gate, &(int){0}, 1)) start_work(); // may skip
  ```
- **COUNTEREXAMPLE** (good): the retry loop in rule 4, or `atomic_compare_exchange_strong`
  for the one-shot case.
- **VERIFICATION**: a 10M-iteration weak loop must leave the gate at 1; a strong one-shot must
  never lose the race with itself.
- **SOURCE**: iso-c11-n1570 §7.17.7.4; iso-cpp20-n4861 [atomics.types.operations]
  ("may fail spuriously ... nearly all uses of weak ... will be in a loop"); rust-reference.

## 6. CAS failure memory order

- **RULE**: C11 `_explicit` CAS `fail` must not be `memory_order_release`/`memory_order_acq_rel`
  and must not be stronger than `succ`. C++ `failure` precondition: relaxed/acquire/seq_cst
  (violation is UB). The C++ single-order overload derives the failure order: acq_rel ->
  acquire, release -> relaxed, otherwise the given order. Rust: failure must be
  Relaxed/Acquire/SeqCst and no stronger than success.
- **WHY AI GETS IT WRONG**: passes one ordering to both slots, e.g.
  `compare_exchange_weak(e, d, Release, Release)`, and expects a diagnostic (there is none).
- **CORRECT REASONING**: the failure path is a load — it can only be relaxed/acquire/seq_cst,
  never release. When unsure, use the single-order overload, which always derives a valid
  failure order.
- **EXAMPLE** (bad): `slot.compare_exchange_strong(e, d, std::memory_order_acq_rel,
  std::memory_order_release);` — violates the failure precondition (UB); g++ accepts it
  silently.
- **COUNTEREXAMPLE** (good): `slot.compare_exchange_weak(e, d, std::memory_order_acq_rel,
  std::memory_order_acquire);`
- **VERIFICATION**: audit manually; no compiler diagnostic is emitted.
- **SOURCE**: iso-c11-n1570 §7.17.7.4; iso-cpp20-n4861 [atomics.types.operations];
  rust-reference (std::sync::atomic docs).

## 7. Lock-free is not guaranteed per type

- **RULE**: In C and C++, `int`/`i32` is NOT guaranteed lock-free. C11 `ATOMIC_INT_LOCK_FREE`
  expands to 0 (never lock-free), 1 (sometimes, runtime check needed), or 2 (always). Runtime
  check: `atomic_is_lock_free(&x)`. C++: `is_lock_free()` and the C++17 static
  `is_always_lock_free`. The only type guaranteed lock-free by the C11/C++ standards is
  `atomic_flag`. Rust differs: since Rust 1.97 the std docs guarantee every available
  `std::sync::atomic` type is lock-free (no `is_lock_free` method exists anymore); the
  remaining portability concern there is type *availability* (`AtomicU64` on 32-bit targets
  without hardware CAS, `AtomicU128`), handled with `cfg(target_has_atomic)`.
- **WHY AI GETS IT WRONG**: assumes "int is lock-free everywhere" and builds async-signal-safe
  or ISR paths on `atomic_int` without checking; conversely, assumes the Rust guarantee does
  not exist and writes dead lock-free checks.
- **CORRECT REASONING**: lock-freedom is per type and per implementation in C/C++ — check the
  macro/`is_lock_free`, or fall back to `atomic_flag`. In Rust 1.97+ the guarantee is part of
  the std contract; portability is about availability, not lock-freedom.
- **EXAMPLE** (bad): `atomic_fetch_add_explicit(&g, 1, memory_order_relaxed);` in a C signal
  handler with no `ATOMIC_INT_LOCK_FREE` check.
- **COUNTEREXAMPLE** (good):
  ```c
  #if ATOMIC_INT_LOCK_FREE == 2
  void handler(void) { atomic_fetch_add_explicit(&g, 1, memory_order_relaxed); }
  #endif
  ```
  For Rust: gate on availability — `#[cfg(target_has_atomic = "64")]` or use `AtomicUsize`.
- **VERIFICATION**: `gcc -O2 -S` on x86-64 shows `lock xadd` for `int` (lock-free there); the
  check matters on other targets. Rust 1.97.1: `AtomicI32::is_lock_free()` no longer exists
  (E0599) — the guarantee replaced the query.
- **SOURCE**: iso-c11-n1570 §7.17.1/§7.17.2; iso-cpp20-n4861 [atomics.types.generic];
  rust-reference (std docs "Portability").

## 8. What is atomic per type

- **RULE**: C: `_Atomic` can qualify any object type (C11 §6.7.2.4). C++: `std::atomic<T>`
  for any TriviallyCopyable T (CAS compares the value representation — padding bits can make
  weak CAS fail repeatedly). Rust: only the predefined `Atomic*` types are atomic
  (`AtomicBool`, `AtomicI*`, `AtomicU*`, `AtomicUsize/Isize`, `AtomicPtr<T>`); there is no
  atomic struct.
- **WHY AI GETS IT WRONG**: reaches for `Atomic`-like behavior on an arbitrary struct, or uses
  `AtomicPtr<T>` where an owned `AtomicUsize` index would do.
- **CORRECT REASONING**: Rust atomics are a closed set of types; shared structures must
  decompose into those (e.g. a Treiber stack uses `AtomicPtr<Node>`). C/C++ atomic structs
  exist but carry padding-bit CAS hazards.
- **EXAMPLE** (bad): `struct Node { key: u32, next: *const Node }` used directly across
  threads — no atomic struct exists in Rust.
- **COUNTEREXAMPLE** (good): `AtomicPtr<Node>` with a CAS loop.
- **VERIFICATION**: compile-time in Rust (no such type); C++ `-Wpadded`-style audit is
  optional.
- **SOURCE**: iso-c11-n1570 §6.7.2.4; iso-cpp20-n4861 [atomics.types.generic]; rust-reference.

## 9. volatile is not an atomic

- **RULE**: `volatile` gives neither atomicity nor ordering. Concurrent read/write of a
  volatile non-atomic object is still a data race (UB). C++: "volatile accesses are not atomic
  and do not order memory." Rust: volatile accesses are explicit (`read_volatile`) and never
  a substitute for `Atomic*`.
- **WHY AI GETS IT WRONG**: "volatile prevents caching, so threads see each other's writes."
- **CORRECT REASONING**: volatile is for MMIO / same-thread signal-flag (single producer),
  never for inter-thread synchronization. Use the atomics library instead.
- **EXAMPLE** (bad): `volatile int ready;` written by one thread, spun on by another.
- **COUNTEREXAMPLE** (good): `_Atomic int ready` / `std::atomic<int>` / `AtomicBool`.
- **VERIFICATION**: TSan flags the volatile version; the atomic version survives.
- **SOURCE**: iso-c11-n1570 §5.1.2.3; iso-cpp20-n4861 [atomics.order] (volatile note);
  rust-reference.

## 10. ABI: where atomicity and ordering come from

- **RULE**: x86: atomicity comes from the `lock` prefix (`lock xadd`, `lock cmpxchg`); `xchg`
  is implicitly locked. AArch64: atomicity from load-exclusive/store-exclusive pairs
  (`ldxr`/`stxr`, with acquire/release `ldaxr`/`stlxr`); release store = `stlr`, acquire load
  = `ldar`. RISC-V: `lr`/`sc` and `amo*`. The language memory model is abstract — these
  instructions change cost and lock-freedom, not semantics.
- **WHY AI GETS IT WRONG**: reads x86 asm where Relaxed/Acquire/SeqCst loads are all plain
  `mov` and concludes ordering is free or universal.
- **CORRECT REASONING**: x86 TSO hides ordering differences; the same source emits
  `dmb`/`ldar`/`stlr` on AArch64. Reason at the model level; verify asm per target when
  portability matters.
- **EXAMPLE** (bad): `objdump` on x86-64 shows `mov` for `load(Acquire)`; the agent reports
  "acquire is free on all architectures".
- **COUNTEREXAMPLE** (good): the same function compiled for AArch64 shows `ldar`; the acquire
  semantics were always part of the model.
- **VERIFICATION**: `rustc --emit=asm -C opt-level=2` (x86-64: `store(SeqCst)` -> `xchg`,
  RMW carries a `lock` prefix — observed `lock incl`/`lock xadd`/`lock cmpxchg`); cross-check
  AArch64/RISC-V on Godbolt.
- **SOURCE**: intel-sdm Vol.3 §8; arm-arm (AArch64 memory model); riscv-isa-spec.

## 11. fetch_add / exchange return the previous value

- **RULE**: `fetch_add`, `fetch_sub`, `fetch_and`, `fetch_or`, `fetch_xor`, and `exchange`
  return the value that was in the atomic before the operation. C11 `atomic_fetch_add` returns
  the old value; C++ `x.fetch_add(n)` and Rust `x.fetch_add(n)` the same.
- **WHY AI GETS IT WRONG**: treats the return as void or as the new value.
- **CORRECT REASONING**: the return is the old value; add `n` locally if the post-state is the
  semantic one.
- **EXAMPLE** (bad): `long v = atomic_fetch_add(&hits, 1); if (v >= 1000) report();` — fires
  one count late because `v` is the old value.
- **COUNTEREXAMPLE** (good): `long v = atomic_fetch_add(&hits, 1) + 1;` when the post-state
  threshold is intended.
- **VERIFICATION**: single-threaded run printing consecutive returns (1, 2, 3 ...).
- **SOURCE**: iso-c11-n1570 §7.17.5; iso-cpp20-n4861 [atomics.types.operations]; rust-reference.

## 12. Direct operator access on atomics

- **RULE**: C: built-in assignment, compound assignment, and `++`/`--` on an lvalue of
  `_Atomic` type are atomic RMW operations with `seq_cst` semantics (the `_explicit` functions
  are the fine-grained alternative). C++: `operator=`, `operator++` etc. on `std::atomic<T>`
  are atomic with seq_cst default. Reading an atomic as its plain type is NOT allowed — you
  must go through the atomic functions/`load()`.
- **WHY AI GETS IT WRONG**: casts an atomic to its plain type to "read it fast" (`int v =
  *(int*)&a;`), producing a non-atomic access to an atomic object — a data race.
- **CORRECT REASONING**: atomicity lives on the type; stripping the qualifier (C) or bypassing
  the member functions (C++) reintroduces the race. Prefer the explicit functions for
  readability.
- **EXAMPLE** (bad): `int v = *(const int*)&atomic_int_var;` in C — non-atomic read of an
  atomic object.
- **COUNTEREXAMPLE** (good): `int v = atomic_load(&atomic_int_var);` / `a.load()`.
- **VERIFICATION**: TSan flags the cast version; asm of the explicit version shows the atomic
  instruction.
- **SOURCE**: iso-c11-n1570 §6.7.2.4 (lvalue semantics, seq_cst built-ins); iso-cpp20-n4861
  [atomics.types.operations]; rust-reference.

## Failure-mode mapping (bug classes)

- CAS expected-value misuse (A10/A11): stale expected after failure -> wrong retry state.
- Lock-free assumption (async-signal-safety): non-lock-free `atomic_int` in a handler.
- volatile-as-atomic (A10): data race that compiles and "passes tests".
- Missing/weak memory order (A11): relaxed publish protocol.
- Cross-language port (A13): Rust `Result` payload vs C/C++ in-out `expected`.
