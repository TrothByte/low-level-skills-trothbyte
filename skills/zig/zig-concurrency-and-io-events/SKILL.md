---
name: zig-concurrency-and-io-events
description: Use when writing or reviewing Zig concurrency: std.Thread spawn/join, atomics and memory ordering, thread-local state, std.Io evented I/O and io_uring, and single-threaded correctness. Prevents data races, fake parallelism, and 0.16 Thread.Pool removals. Version-pinned to Zig 0.15-0.17.
---

# Zig Concurrency and IO Events

## When to use

- Writing multithreaded Zig: `std.Thread.spawn`/`join`, `threadlocal`, mutexes, atomics.
- Choosing atomic operations and their memory orderings (`@atomicLoad`/`@atomicRmw`/
  `@cmpxchg*` with `std.lang.AtomicOrder`).
- Designing evented or async I/O around `std.Io` (including `std.Io.Linux.Uring` on Linux).
- Reviewing whether a program actually needs threads, or whether single-threaded
  execution is already correct.

## When not to use

- Memory-ordering theory in general — see `memory-ordering-reasoning` and
  `atomics-c11-cpp11-rust`.
- Kernel concurrency (RCU, atomic contexts) — see `kernel-rcu-memory-barriers` and
  `kernel-atomic-context`.
- Deadlock/lock-ordering analysis — see `concurrency-deadlock-and-lock-ordering`.
- io_uring kernel API design itself — see io-uring-docs (this skill covers Zig's wrapper).

## What the agent often gets wrong

- "Fake parallelism": marking code thread-safe with mutexes/atomics when it never spawns
  a thread — or worse, claiming a single-threaded program is concurrent (the CONCUR
  single-thread category). Correct single-threaded code must NOT be flagged as needing
  threads.
- Shared-mutable state without synchronization — data races are Illegal Behavior; the
  Debug allocator/`@atomicStore` won't catch them, and `std.Thread.Mutex` exists for a
  reason.
- Using `std.Thread.Pool` on 0.16+ — it was removed; use `std.Io.Threaded` or your own
  pool (INFERRED surface; verify per pin).
- Forgetting `thread.join()` — spawned threads are detached-resource-ish; without join the
  test/program may end before work completes or leak thread resources.
- Believing async exists as a keyword — `async`/`await` were removed in 0.15; evented
  behavior lives in the `std.Io` interface.
- Porting io_uring code verbatim from C/liburing into `std.Io.Linux.Uring` without
  checking the wrapper's API — SQ/CQ mechanics transfer, Zig names do not.

## How to reason correctly

1. Establish whether threads are needed at all: profile first; if the workload is
   sequential, single-threaded code is correct — do not refactor it into fake parallelism.
2. If threads are needed, spawn with `std.Thread.spawn(.{}, fn, .{args})` and `join`
   each handle; use `threadlocal` for per-thread state.
3. For shared data, pick one mechanism per variable: an atomic type (`@atomicRmw`
   fetch-add, `@cmpxchgStrong` CAS) with an explicit `AtomicOrder`, or a lock
   (`std.Thread.Mutex` etc.) around the critical section. Never mix a plain load/store
   with atomic access to the same location.
4. Remember ordering: `.acquire`/`.release`/`.seq_cst`/`.monotonic` come from the same
   model as C11/Rust (see `memory-ordering-reasoning`); pick the weakest correct one.
5. For I/O concurrency, pass an `std.Io` instance around (0.15+ design); on Linux,
   `std.Io.Linux.Uring` wraps io_uring (SQ → CQ, `io_uring_enter`); treat it as
   Linux-only and version-sensitive.
6. Respect single-threaded builds: `@import("builtin").single_threaded` is true when
   `-fsingle-threaded`; in that mode `threadlocal` variables behave as globals, and
   threading APIs are unavailable/stubbed.

## What to verify

- Every shared-memory access is through the same synchronization mechanism (atomic or
  lock); no plain data race.
- Every `spawn` has a matching `join` (or documented detach), and the program does not
  exit before threads finish.
- `AtomicOrder` is chosen deliberately; `.seq_cst` everywhere is a smell but not an error.
- No `std.Thread.Pool` on 0.16+; no `async`/`await` keywords anywhere (0.15+).
- `std.Io.Linux.Uring` usage is behind a Linux target check and version-verified.
- Single-threaded code is not flagged as "needs threads".

## How to verify

```
zig test examples/good/threads.zig
zig test examples/good/atomic_counter.zig
zig test examples/bad/data_race.zig             # race: must be flagged (may "pass")
zig test examples/bad/thread_pool_016.zig        # fails on 0.16+: Thread.Pool removed
zig test examples/good/single_threaded.zig       # correct single-threaded code (FP case)
zig build test -fsingle-threaded                  # single-threaded build check
```

io_uring examples require Linux; on this host (Windows) they are researched only:
```
zig build -Dtarget=x86_64-linux-gnu test
```
to be executed on a Linux host or under QEMU (absent here).

## Where the knowledge comes from

- zig-std-source (std/Thread.zig — spawn/join, Mutex, WaitGroup; std/atomic.zig;
  std/Io/Threaded.zig; std/Io/Linux/Uring.zig).
- zig-langref §Thread Local Variables, §Atomics, §Async Functions, §Single Threaded
  Builds, §Builtin Functions (@atomicLoad, @atomicStore, @atomicRmw, @cmpxchgStrong,
  @cmpxchgWeak).
- zig-release-notes 0.15.x (async and await keywords removed) and 0.16.0 (Thread.Pool
  Removed; heap.ArenaAllocator thread-safe; I/O as an Interface).
- io-uring-docs (submission/completion queues).
- memory-ordering-reasoning and atomics-c11-cpp11-rust (existing skills, same ordering
  model).

## Related skills

- `memory-ordering-reasoning` — the ordering semantics behind AtomicOrder.
- `atomics-c11-cpp11-rust` — cross-language atomic API comparison.
- `zig-allocators-and-memory-management` — thread-safe ArenaAllocator (0.16+).
- `zig-error-model-and-defers` — error handling inside spawned functions.
- `concurrency-deadlock-and-lock-ordering` — when locks are the wrong tool.

## Evaluation

- Synthetic: unsynchronized shared counter, missing join, `std.Thread.Pool` on 0.16+,
  `async`/`await` keywords, ported-liburing-verbatim code — must be caught; good
  threads/atomics/single-threaded examples must pass.
- False-positive: correct single-threaded code must NOT be flagged as needing threads;
  a `Mutex`-protected counter and a `seq_cst` fetch-add are correct, not "overkill".
- Historical: `async`/`await` removal (0.15) and `Thread.Pool` removal (0.16) are the
  regression targets.
- Adversarial: a data race that "passes" deterministically on one run (thread interleaving
  never collides in CI) — the race must be found by reasoning, not by test luck; and an
  io_uring program that only compiles on Linux, misclaimed as portable.
- Commands and recorded results: `evals/README.md`.
