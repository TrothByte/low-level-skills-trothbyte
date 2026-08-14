# Memory Ordering & Atomics — Reference

Sources: C11 N1570 §5.1.2.4 & §7.17; C++20 [intro.races]/[atomics.order]; Rust Reference
(the Rust memory model mirrors C++20); Intel SDM Vol.3 §8; Arm ARM (weak ordering).

## 1. The four ordering families (C11/C++11/Rust)

- **RULE**: `relaxed` = atomicity only; `acquire` = loads only, subsequent reads/writes can't
  move before it; `release` = stores only, prior reads/writes can't move after it;
  `acq_rel` = both (RMW); `seq_cst` = acq_rel + a single total order.
- **WHY AI GETS IT WRONG**: treats ordering as "levels of strength" and picks arbitrarily;
  treats `relaxed` as "cheap and fine".
- **CORRECT REASONING**: ordering is about edges in happens-before. Relaxed creates NO edges.
  Use it only for counters where order doesn't matter (stats).
- **EXAMPLE**: `AtomicUsize::fetch_add(1, Relaxed)` for a stats counter — fine.
- **COUNTEREXAMPLE** (bad): `ready.store(true, Relaxed)` in a flag protocol — the reader can
  see `ready==true` without seeing the published data.
- **VERIFICATION**: TSan/Miri; litmus reasoning.
- **SOURCE**: N1570 §7.17.3; C++20 [atomics.order]; Rust std::sync::atomic.

## 2. Synchronizes-with / happens-before

- **RULE**: a release store S in thread A synchronizes-with an acquire load L in thread B iff
  L reads S or a later write in S's release sequence. Then everything sequenced-before S in A
  happens-before everything sequenced-after L in B.
- **WHY AI GETS IT WRONG**: assumes any two atomics "sync" each other.
- **CORRECT REASONING**: the edge exists only for the specific read that observes the release
  (or its release sequence). A load that reads a different value creates no edge.
- **EXAMPLE** (correct):
  ```
  // A: data = 42;            (normal store)
  // A: ready.store(true, Release);
  // B: if (ready.load(Acquire)) { print(data); }   // sees 42
  ```
- **COUNTEREXAMPLE** (bad): `ready.load(Relaxed)` — no edge; `data` read is racy.
- **VERIFICATION**: prove the edge by the read-value condition; TSan.
- **SOURCE**: N1570 §5.1.2.4; Rust nomicon "races".

## 3. Data race is UB

- **RULE**: two accesses to the same non-atomic object, at least one write, without a
  happens-before edge between them → data race → UB.
- **WHY AI GETS IT WRONG**: "it's just two threads reading/writing; worst case stale data".
- **CORRECT REASONING**: the optimizer may exploit the race (e.g. assume no other thread
  writes), producing wrong code — not just "stale values".
- **EXAMPLE** (bad): `static mut COUNTER: i64` incremented from two threads (Rust unsafe).
- **COUNTEREXAMPLE** (good): `AtomicI64` with `fetch_add(1, Relaxed)`.
- **VERIFICATION**: TSan (C/C++); Rust safe code won't compile; `cargo +nightly miri`.
- **SOURCE**: N1570 §5.1.2.4p4; Rust Reference "data race" in UB list.

## 4. x86 vs ARM: the asm trap

- **RULE**: x86 is TSO — loads aren't reordered with loads, stores with stores, and
  loads-with-earlier-stores are not reordered; so plain `mov` is often as strong as acquire.
  ARM/POWER/RISC-V are weakly ordered — the compiler must emit barriers/dmb.
- **WHY AI GETS IT WRONG**: reads x86 asm (identical for Relaxed/Acquire/SeqCst loads), then
  assumes ordering "doesn't matter" or "is free".
- **CORRECT REASONING**: the C++ memory model is architecture-independent. Verify semantics at
  the model level; asm differences appear when you cross to ARM/AArch64/RISC-V.
- **EXAMPLE** (x86 asm, Rust): `store(SeqCst)` → `xchg` (or `mov`+`mfence`); `store(Relaxed)` → `mov`.
  `fetch_add(SeqCst)` → `lock xadd`. On AArch64, release store → `stlr`, acquire load → `ldar`.
- **VERIFICATION**: `rustc --emit=asm` (native) vs Godbolt AArch64/RISC-V.
- **SOURCE**: Intel SDM Vol.3 §8.2 (TSO); Arm ARM "The AArch64 memory model".

## 5. Volatile is NOT an atomic

- **RULE**: `volatile` guarantees neither atomicity nor ordering; it only forces
  read/write at the abstract-machine level. Two threads mutating a `volatile` non-atomic is
  still a data race.
- **WHY AI GETS IT WRONG**: "volatile prevents caching, so it syncs threads."
- **CORRECT REASONING**: volatile is for MMIO/ISR-shared flags (single producer/consumer
  without concurrent writers), NOT for inter-thread synchronization.
- **COUNTEREXAMPLE** (bad): `volatile int flag` as a thread-ready flag.
- **VERIFICATION**: TSan flags it; Rust won't even allow it in safe code.
- **SOURCE**: N1570 §5.1.2.3; Rust Reference "volatile" vs atomics.

## Quick decision table

| Goal | Correct choice |
|---|---|
| stats counter, order irrelevant | `Relaxed` |
| publish data then signal | `Release` store + `Acquire` load |
| once/futex-style flag | `AcqRel` RMW or `SeqCst` |
| full global order needed | `SeqCst` |
| anything else | reason about edges first, don't guess |

## Verified asm facts (rustc 1.97.1, x86-64, opt-level 2)

Compiled `#[no_mangle] pub extern "C"` atomic wrappers. On x86:

| Operation | Generated asm | Note |
|---|---|---|
| `store(v, Relaxed)` | `movl %edx, (%rcx)` | plain store |
| `store(v, SeqCst)` | `xchgl %edx, (%rcx)` | locked exchange — full barrier |
| `load(Acquire)` | `movl (%rcx), %eax` | plain load (x86 loads are acquire-strong) |
| `fetch_add(1, Relaxed)` | `lock xaddl %eax, (%rcx)` | lock prefix needed for atomicity |
| `fetch_add(1, SeqCst)` | `lock xaddl %eax, (%rcx)` | identical to Relaxed on x86 |

Teaching points:
- On x86, RMW `fetch_add` is identical for Relaxed and SeqCst — the `lock` prefix gives
  atomicity AND total RMW order. The difference between orders shows up on weakly-ordered
  targets (AArch64: `stlr`/`ldar`, RISC-V: `fence`), and in reordering opportunities for
  non-RMW code on the compiler side.
- The observable store difference (plain `mov` vs `xchg`) is real on x86 for SeqCst vs
  Relaxed; for load, x86's TSO makes even `Acquire` a plain `mov`.

## Common failure modes (mapped to bug classes)

- A11 (wrong ordering): flag protocol with Relaxed — TSan/ Miri catches.
- A10 (data race): non-atomic shared counter — TSan catches; Rust won't compile in safe code.
- A13 (unsafe impl Send/Sync): claiming thread-safety without an ordering proof.
