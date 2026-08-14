---
name: memory-ordering-reasoning
description: Use when reasoning about atomic operations, memory ordering, happens-before, or lock-free synchronization in C11/C++11/Rust — choosing Relaxed vs Acquire/Release vs SeqCst, diagnosing "compiles but races at runtime", or explaining why ordering matters across architectures (x86 vs ARM). Teaches the ordering model and how to verify it.
---

# Memory Ordering & Atomics Reasoning (C11/C++11/Rust)

## When to use

- Choosing a memory order for atomic loads/stores/RMWs.
- Reviewing lock-free code for data races that compile and "pass tests".
- Understanding why `Relaxed` is wrong for a flag/once-protocol.
- Reading x86 vs ARM atomic asm and explaining the difference.

## When not to use

- Synchronization that isn't lock-free (mutexes/condvars) — use `concurrency-deadlock-and-lock-ordering`.
- Kernel memory barriers (`smp_mb`, RCU) — use `kernel-rcu-memory-barriers`.
- `volatile` for MMIO/hardware registers — `embedded-volatile-and-memory-ordering`.

## What the agent often gets wrong

- "`Relaxed` is fine; it's still atomic." Relaxed gives only atomicity, NO ordering. A
  `Relaxed` flag write does not synchronize-with the reader that sets up other data.
- "`SeqCst` everywhere is always correct." It is sufficient but not always necessary, and
  masks the real ordering; on non-x86 it is expensive. It does not make data-race-free-by-construction either.
- "`volatile` is like an atomic." `volatile` is neither atomic nor ordered; never use it for inter-thread sync.
- "On x86 the asm looks the same, so ordering doesn't matter." x86 TSO hides ordering
  differences in asm; the same code is a data race on ARM/POWER/RISC-V. The C++ memory model
  is architecture-independent — reason at the model level, not the asm level.
- "Acquire/Release is automatic." They must be PAIRED: an acquire can only synchronize-with a
  release that writes the value the acquire reads (or reads from the release sequence).

## How to reason correctly

1. Every synchronization is a "synchronizes-with" edge: a Release store S in thread A and an
   Acquire load L in thread B (that reads S, or a later write in the release sequence) create
   happens-before(A-before-S, B-after-L).
2. Without such an edge, two threads accessing a non-atomic object (one writing) is a data
   race = UB.
3. Use the message-passing pattern for the basics: release-store the "ready" flag AFTER
   publishing the data; acquire-load it BEFORE reading the data.
4. Choose the weakest ordering that still forms the required edges — do not default to SeqCst.
5. Verify with TSan/Miri; on x86 also verify with asm that the RMW has the `lock` prefix.

## What to verify

- The sync edge exists: every acquire has a matching release, and the read really observes
  the published value.
- The flag protocol is correct: data published before `release`, read after `acquire`.
- RMW operations (fetch_add, CAS) use the intended order; on x86 they carry a `lock` prefix.

## How to verify

```
# Rust: compile the atomic functions and read the asm
rustc --emit=asm -C opt-level=2 atomics.rs    # compare store(Relaxed) vs store(SeqCst)
# Data-race detection
cargo +nightly miri run   # or TSan if available
```

## Where the knowledge comes from

- C11 N1570 §5.1.2.4, §7.17; C++20 [atomics.order]; Rust Reference memory model
- Intel SDM Vol.3 ch.8 (x86 memory ordering); ARM ARM (weakly-ordered)

## Related skills

- `atomics-c11-cpp11-rust` — the API reference table (require)
- `compiler-ub-assumptions` — data race is UB; the optimizer exploits it
- `c-undefined-behavior` — race as UB class A10/A11

## Evaluation

Adversarial (AD-01): a "flag + data" protocol with Relaxed ordering that compiles and passes
naive tests — the agent must identify the missing sync edge and fix to release/acquire.
Synthetic: pairing rules, x86 vs ARM asm divergence. False-positive: correct acquire/release
pair must NOT be "strengthened" to SeqCst unnecessarily.
