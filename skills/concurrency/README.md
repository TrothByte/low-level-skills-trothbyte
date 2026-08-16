# concurrency — Skills

Concurrency bugs compile.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `atomics-c11-cpp11-rust` | Use when writing, reviewing, or porting atomic code in C11, C++20, or Rust — API lookup for _Atomic / std::atomic / std::sync::atomic, compare_exchange semantics (expected in-out, weak spurious failure), memory_order/Ordering validity, lock-free guarantees, and cross-language porting. | common | source-backed | `skills/concurrency/atomics-c11-cpp11-rust` |
| `concurrency-actual-parallelism-detection` | Use when verifying that concurrent code actually executes in parallel — distinguishing real parallelism from "fake" thread-safe code (CONCUR ST class) and catching concurrency-limit bypasses (codex#37653: 86 processes, kernel panic). Requires measuring wall-clock scaling and real overlap, not just counting threads or using primitives. | unique | source-backed | `skills/concurrency/concurrency-actual-parallelism-detection` |
| `concurrency-condvar-and-spurious-wakeup` | Use when writing, reviewing, or debugging condition-variable code (std::condition_variable, C11 cnd_*) — pairing wait with a predicate and mutex, handling spurious and lost wakeups (CON36-C), choosing notify_one vs notify_all, or fixing a program that occasionally hangs. | common | source-backed | `skills/concurrency/concurrency-condvar-and-spurious-wakeup` |
| `concurrency-deadlock-and-lock-ordering` | Use when writing or reviewing code that acquires two or more locks — detecting ABBA deadlock, enforcing consistent lock ordering (CON35-C), choosing std::lock/std::scoped_lock, avoiding recursive/try_lock hazards, or interpreting a TSan/helgrind lock-order report. | common | source-backed | `skills/concurrency/concurrency-deadlock-and-lock-ordering` |
| `memory-model-arm-x86-riscv` | Use when reasoning about cross-thread ordering on ARM, x86, or RISC-V: lock-free code, message passing, publication, atomic orderings, memory barriers, seq_cst vs relaxed, and porting TSO-only code to weak-memory machines. Teaches each architecture's memory model and how to map C11/Rust atomics to hardware. | cross-layer | source-backed | `skills/concurrency/memory-model-arm-x86-riscv` |
| `memory-ordering-reasoning` | Use when reasoning about atomic operations, memory ordering, happens-before, or lock-free synchronization in C11/C++11/Rust — choosing Relaxed vs Acquire/Release vs SeqCst, diagnosing "compiles but races at runtime", or explaining why ordering matters across architectures (x86 vs ARM). Teaches the ordering model and how to verify it. | improved | source-backed | `skills/concurrency/memory-ordering-reasoning` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
