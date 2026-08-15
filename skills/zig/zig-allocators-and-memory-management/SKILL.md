---
name: zig-allocators-and-memory-management
description: Use when designing Zig allocation: choosing an allocator, passing Allocator through APIs, arena lifetime, leak/double-free detection with std.testing.allocator, and OutOfMemory handling. Prevents hidden allocation, wrong-lifetime frees, and container item-invalidation bugs. Version-pinned to Zig 0.15-0.17.
---

# Zig Allocators and Memory Management

## When to use

- Designing APIs that allocate: choose to take an `Allocator` parameter, never allocate
  behind the caller's back.
- Picking an allocator per use case (arena, fixed buffer, testing, general purpose) and
  per build mode.
- Debugging leaks, double frees, or use-after-free in Zig code and its test suite.
- Reviewing pointer/slice ownership and lifetime (who frees, when does a slice go stale).

## When not to use

- Global allocation strategy (mmap/vmalloc/SLUB) — that is a kernel/OS topic.
- C `malloc`/`free` interop beyond `std.heap.c_allocator` and `std.c` — see
  `zig-ffi-c-interop`.
- Pure value/stack code with no dynamic allocation — no allocator needed.
- Performance comparison of allocators — measure with `performance-measurement-discipline`.

## What the agent often gets wrong

- Allocating inside a function and returning the pointer without an `Allocator` parameter —
  Zig has no hidden allocator; the caller must own the policy.
- Using `std.testing.allocator` in production code (or production allocators in tests),
  losing leak/double-free detection exactly where it matters.
- Forgetting `defer list.deinit(gpa)` after `list.append(gpa, ...)` — the test runner
  reports the leak, the agent blames the runner.
- Double-`deinit` (arena freed twice, or `destroy` after `free`), which DebugAllocator
  detects — agents call it "random crash".
- Holding a slice into an `ArrayList` across an `append` — the items pointer moves on
  resize; this is documented, not a bug.
- Assuming `GeneralPurposeAllocator` is the 0.16/0.17 default: the langref flow chart now
  recommends `std.heap.DebugAllocator` (Debug) and `std.heap.smp_allocator` (ReleaseFast);
  `heap.ThreadSafeAllocator` was removed in 0.16.
- Treating `error.OutOfMemory` as unrecoverable — Zig convention is to propagate it.

## How to reason correctly

1. Follow the langref allocator flow chart: library → accept `Allocator`; libc → `c_allocator`;
   comptime-bounded max bytes → `FixedBufferAllocator`; CLI/request-per-cycle → `ArenaAllocator`
   (free everything with `deinit`); test → `std.testing.allocator` (or `FailingAllocator`
   to prove OutOfMemory handling); otherwise → `DebugAllocator`/`smp_allocator` in main.
2. Thread the allocator explicitly: every function that allocates takes an `Allocator`
   parameter and documents who owns the result ("caller owns", "borrowed").
3. Match free to allocator and to ownership: `arena.deinit()` frees the whole arena at
   once; `allocator.destroy`/`allocator.free` for individual items; never mix allocators.
4. Track lifetime: an `ArrayList(T).items` slice is valid only until the next resize.
5. In tests, allocate through `std.testing.allocator` so the runner reports leaks; use
   `defer` (and `errdefer` for error paths) to pair every allocation with its free.

## What to verify

- No function allocates without receiving an `Allocator` (except `init` of the allocator
  itself).
- Every allocation is paired with exactly one free path (`defer`/`errdefer`), and free
  uses the same allocator that allocated.
- `std.testing.allocator` used in tests; `zig test` reports zero leaks.
- Arena lifetime does not outlive references into it; no `arena.deinit()` twice.
- Containers used unmanaged style with explicit `gpa` argument (0.15+ default).
- `error.OutOfMemory` is propagated with `try`, not swallowed or wrapped in `catch unreachable`
  except where the operation genuinely cannot fail.

## How to verify

```
zig test examples/good/cli_arena.zig          # arena pattern
zig test examples/good/fixed_buffer.zig       # bounded no-heap allocation
zig test examples/good/leak_detection.zig     # testing allocator, zero leaks
zig test examples/bad/leak.zig                # runner: N leaks detected (fails)
zig test examples/bad/double_free.zig         # allocator catches double free (fails)
zig build test                                # project test step
```

Researched — zig not installed on this host; commands are the recorded verification plan.
Expected diagnostics (leak report, double-free) are KNOWN behavior of the testing/`DebugAllocator`
paths; exact text UNVERIFIED.

## Where the knowledge comes from

- zig-langref §Memory (Choosing an Allocator, Heap Allocation Failure, Lifetime and
  Ownership), §Zig Test (Report Memory Leaks), §comptime (namespace-level constants are in
  the global data section, stack vars die with the frame).
- zig-std-source (std/heap.zig — ArenaAllocator, FixedBufferAllocator, c_allocator,
  DebugAllocator, smp_allocator, page_allocator; std/testing.zig — allocator,
  FailingAllocator).
- zig-release-notes 0.16.0 (heap.ArenaAllocator thread-safe and lock-free; heap.ThreadSafe
  Allocator removed; Migration to "Unmanaged" Containers — heap.MemoryPoolUnmanaged added).

## Related skills

- `zig-error-model-and-defers` — `defer`/`errdefer` are the pairing mechanism; OutOfMemory
  is an error union.
- `zig-fuzzer-and-testing` — `std.testing.allocator` leak detection in fuzz/test runs.
- `zig-ffi-c-interop` — `c_allocator` and the C ABI boundary for allocator callbacks.
- `zig-concurrency-and-io-events` — ArenaAllocator became thread-safe in 0.16; thread-local
  allocators.
- `zig-comptime-metaprogramming` — comptime-known size bounds make FixedBufferAllocator valid.

## Evaluation

- Synthetic: hidden allocation, missing `deinit`, double free, wrong-allocator free,
  stale-items use, `std.testing.allocator` in production — each must be caught;
  good arena/fixed-buffer/testing-allocator code must pass.
- False-positive: `defer arena.deinit()` at the end of a CLI main, slices invalidated by
  documented resizes, `error.OutOfMemory` propagation — must NOT be flagged.
- Historical: the 0.16.0 arena thread-safety change and the ThreadSafeAllocator removal are
  regression targets for version-sensitive allocator claims.
- Adversarial: leak and double-free fixtures under `std.testing.allocator`; a function that
  returns an arena-allocated pointer whose arena is already destroyed by the caller.
- Commands and recorded results: `evals/README.md`.
