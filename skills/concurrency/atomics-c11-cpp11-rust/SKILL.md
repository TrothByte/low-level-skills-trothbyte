---
name: atomics-c11-cpp11-rust
description: Use when writing, reviewing, or porting atomic code in C11, C++20, or Rust — API lookup for _Atomic / std::atomic / std::sync::atomic, compare_exchange semantics (expected in-out, weak spurious failure), memory_order/Ordering validity, lock-free guarantees, and cross-language porting.
---

# Atomics API: C11 / C++20 / Rust

## When to use

- Writing or reviewing code that uses C11 `_Atomic`/`<stdatomic.h>`, C++ `std::atomic`, or Rust `std::sync::atomic`.
- Porting atomic code from one of the three languages to another (the API table in `references/atomics-api.md`).
- Checking whether a `compare_exchange`/`compare_exchange_weak` loop is written correctly.
- Deciding whether a type is lock-free and what the ABI looks like (x86 `lock` prefix, AArch64 `ldxr`/`stxr`).

## When not to use

- Choosing *which* memory order creates the right happens-before edge — use `memory-ordering-reasoning`.
- Mutex/condvar synchronization — use `concurrency-deadlock-and-lock-ordering`.
- `volatile` for MMIO/hardware registers — use `embedded-volatile-and-memory-ordering`.
- C++ `std::atomic_ref`, wait/notify (`atomic_wait`), or futex-style optimization — out of scope.

## What the agent often gets wrong

- "CAS returns bool, so I only need to check the result." `expected` is **in-out**: on failure it is overwritten with the observed value. Reusing it as the pre-call assumption is a bug.
- "`int` is always lock-free." In C/C++ only `atomic_flag` is guaranteed lock-free; check
  `ATOMIC_*_LOCK_FREE` / `is_lock_free` / `is_always_lock_free`. (Rust 1.97+ differs: all
  available `std` atomics are guaranteed lock-free; the portability concern is type
  availability, e.g. `AtomicU64` on 32-bit targets.)
- "`volatile` is like an atomic." It is neither atomic nor ordered; concurrent access is still a data race.
- "I forgot the memory order and it compiled, so it's fine." The default in C11/C++ is `seq_cst`; Rust has no default and `load`/`store` accept only a subset of `Ordering` (invalid ones panic in debug, UB otherwise).
- "`compare_exchange_weak` and `..._strong` are interchangeable." Weak may fail spuriously; only strong must not.
- "The Rust API maps 1:1 to C/C++." Rust's `compare_exchange` returns `Result<T, T>`; the `Err` payload is the observed value — there is no separate strong/weak pair.

## How to reason correctly

1. Identify the operation kind: load, store, exchange, RMW (fetch_add/CAS).
2. Write the C11 name, C++20 name, and Rust name side by side (see the mapping in `references/atomics-api.md`) — semantics first, spelling second.
3. For CAS: decide who owns the retry. `weak` needs a loop (spurious failure); `strong` is for one-shot transitions. Track `expected` as in-out in C/C++; in Rust use the `Err` payload.
4. For ordering: pick the memory order per operation validity (acquire only on loads/success-CAS, release only on stores/success-CAS, acq_rel/seq_cst on RMW). Ask whether the default (seq_cst) is what you really want.
5. For lock-freedom: check the macro/`is_lock_free` before relying on it in a signal handler or ISR.
6. Verify: compile and run the examples, inspect asm per target, and for race/ordering use TSan/Miri (see `memory-ordering-reasoning`).

## What to verify

- CAS loops actually terminate and the post-failure `expected` value is used (never the stale pre-call value).
- `weak` CAS is inside a loop; `strong` is used for one-shot gates.
- Memory order matches operation kind (no acquire stores, no release loads, valid CAS failure order).
- Lock-free claims are backed by `ATOMIC_*_LOCK_FREE` / `is_lock_free` / `is_always_lock_free`
  (C/C++), or by the Rust 1.97 lock-free guarantee plus `cfg(target_has_atomic)` portability.
- Cross-language port preserves semantics: e.g. Rust `compare_exchange` failure payload == C/C++ updated `expected`.

## How to verify

```
# C11
gcc -std=c11 -Wall -Wextra -Werror -O2 examples/good/c11_atomic_good.c -o out && ./out
# C++20
g++ -std=c++20 -Wall -Wextra -Werror -O2 examples/good/cpp20_atomic_good.cpp -o out && ./out
# Rust
rustc --edition 2021 examples/good/rust_atomic_good.rs -o out && ./out
# Bad examples compile (that is the trap) but are semantically wrong; see evals/README.md
gcc -std=c11 -Wall -Wextra -Werror -O2 examples/bad/c11_atomic_bad.c -o out && ./out
# asm per target (x86-64 here; compare with Godbolt for AArch64/RISC-V)
rustc --edition 2021 --emit=asm -C opt-level=2 examples/good/rust_atomic_good.rs
```

## Where the knowledge comes from

- ISO C11 N1570 §6.7.2.4, §5.1.2.4, §7.17 (`iso-c11-n1570`)
- ISO C++20 N4861 [atomics] (`iso-cpp20-n4861`)
- The Rust Reference memory model + std `std::sync::atomic` (`rust-reference`)
- Intel SDM Vol.3 §8 — locked operations, TSO (`intel-sdm`)
- Arm ARM AArch64 memory model — `ldar`/`stlr`, `ldxr`/`stxr` (`arm-arm`)

## Related skills

- `memory-ordering-reasoning` — which order creates which edge (require of)
- `c-undefined-behavior` — data race is UB; what the optimizer may do
- `compiler-ub-assumptions` — why a "works on my machine" atomic can break at `-O2`

## Evaluation

Synthetic: C11/C++20/Rust CAS retry loops (weak vs strong), expected in-out on failure,
memory-order validity per operation, lock-free macro checks. Adversarial: a "fixed"
`compare_exchange_strong` one-shot that still races because the failure path reused the
stale expected; Rust `store(Ordering::Acquire)` runtime panic; missing lock-free check in a
signal-handler path. False-positive: correct CAS loop with updated expected, relaxed stats
counter, strong CAS one-shot, must NOT be flagged. Cross-language porting: translate the
same publish/consume protocol between C11, C++20, and Rust and require identical semantics.
