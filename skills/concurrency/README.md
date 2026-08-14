# concurrency — Skills

Concurrency bugs compile. These skills cover the memory-ordering model (Relaxed/Acquire/Release/SeqCst), the C11/C++11/Rust atomics API, and lock/condvar discipline — so you reason about happens-before edges instead of guessing.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `atomics-c11-cpp11-rust` | Use when writing, reviewing, or porting atomic code in C11, C++20, or Rust — API lookup for _Atomic / std::atomic / std::sync::atomic, compare_exchange semantics (expected in-out, weak spurious failure), memory_order/Ordering validity, lock-free guarantees, and cross-language porting. | source-backed | `skills/concurrency/atomics-c11-cpp11-rust` |
| `concurrency-condvar-and-spurious-wakeup` | Use when writing, reviewing, or debugging condition-variable code (std::condition_variable, C11 cnd_*) — pairing wait with a predicate and mutex, handling spurious and lost wakeups (CON36-C), choosing notify_one vs notify_all, or fixing a program that occasionally hangs. | source-backed | `skills/concurrency/concurrency-condvar-and-spurious-wakeup` |
| `concurrency-deadlock-and-lock-ordering` | Use when writing or reviewing code that acquires two or more locks — detecting ABBA deadlock, enforcing consistent lock ordering (CON35-C), choosing std::lock/std::scoped_lock, avoiding recursive/try_lock hazards, or interpreting a TSan/helgrind lock-order report. | source-backed | `skills/concurrency/concurrency-deadlock-and-lock-ordering` |
| `memory-ordering-reasoning` | Use when reasoning about atomic operations, memory ordering, happens-before, or lock-free synchronization in C11/C++11/Rust — choosing Relaxed vs Acquire/Release vs SeqCst, diagnosing "compiles but races at runtime", or explaining why ordering matters across architectures (x86 vs ARM). Teaches the ordering model and how to verify it. | source-backed | `skills/concurrency/memory-ordering-reasoning` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
