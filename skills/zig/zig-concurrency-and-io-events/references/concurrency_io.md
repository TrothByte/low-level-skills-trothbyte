# Zig Concurrency and IO Events — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.
Version markers: KNOWN / INFERRED / UNVERIFIED.

## 1. std.Thread.spawn / join

- **RULE**: `std.Thread.spawn(.{}, func, .{args})` returns a thread; `thread.join()`
  waits for it. Threads must be joined (or the program must outlive them); a spawned
  thread that is never joined can keep running after main/test exit.
- **WHY AI GETS IT WRONG**: spawns threads and returns without joining, "because the test
  passed"; the race detector/luck hides it.
- **CORRECT REASONING**: join every handle you spawn in tests and before the program ends;
  for fire-and-forget, scope the thread's lifetime explicitly.
- **EXAMPLE** (bad):
  ```zig
  test "never joined" {
      const t = try std.Thread.spawn(.{}, worker, .{});
      _ = t; // test returns while worker may still run
  }
  fn worker() void { @atomicStore(u32, &flag, 1, .seq_cst); }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  test "joined" {
      const t = try std.Thread.spawn(.{}, worker, .{});
      t.join();
      try std.testing.expectEqual(@as(u32, 1), flag);
  }
  ```
- **VERIFICATION**: `zig test examples/good/threads.zig` — deterministic once joined;
  the unjoined variant is flaky.
- **SOURCE**: zig-std-source (std/Thread.zig); zig-langref §Thread Local Variables
  (threadlocal example uses spawn/join).

## 2. Data races are Illegal Behavior — synchronize shared state

- **RULE**: a data race (concurrent unsynchronized access, at least one write) is Illegal
  Behavior. Use one mechanism per shared variable: atomics (`@atomicRmw`,
  `@atomicLoad`/`@atomicStore`, `@cmpxchgStrong`/`@cmpxchgWeak`) or a lock
  (`std.Thread.Mutex`/`RwLock`). `threadlocal` gives per-thread state instead.
- **WHY AI GETS IT WRONG**: increments a shared counter with `x += 1` "because it usually
  works"; the interleaving never collides in a short test run, so the agent declares it
  correct.
- **CORRECT REASONING**: races are timing-dependent; a passing test proves nothing. Pick
  the synchronization primitive, use it consistently, and never read/write the same
  location both atomically and plainly.
- **EXAMPLE** (bad):
  ```zig
  var counter: u32 = 0;
  fn worker() void { counter += 1; } // plain read-modify-write: race
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  var counter: u32 = 0;
  fn worker() void { _ = @atomicRmw(u32, &counter, .Add, 1, .monotonic); }
  ```
- **VERIFICATION**: `zig test examples/bad/data_race.zig` may pass — the reviewer must
  flag it by construction (verification is reasoning + TSan-style tooling, absent here);
  `zig test examples/good/atomic_counter.zig` passes deterministically.
- **SOURCE**: zig-langref §Atomics, §Illegal Behavior; zig-std-source (std/atomic.zig).

## 3. Atomics and ordering

- **RULE**: `@atomicLoad`, `@atomicStore`, `@atomicRmw`, `@cmpxchgStrong/Weak` take an
  `AtomicOrder` (0.16+: `std.lang.AtomicOrder`; 0.15: `std.builtin.AtomicOrder`;
  ordering values and semantics match C11/Rust: `.monotonic`, `.acquire`, `.release`,
  `.acq_rel`, `.seq_cst`).
- **WHY AI GETS IT WRONG**: uses `.seq_cst` everywhere "to be safe" without reasoning, or
  picks `.monotonic` for a flag that must establish a happens-before edge.
- **CORRECT REASONING**: ordering must match the data flow: release-store + acquire-load
  for publish/consume; `.seq_cst` only where a total order is required. `@atomicRmw` is
  the classic fetch-add; CAS loops use `@cmpxchgWeak`.
- **EXAMPLE** (bad):
  ```zig
  const ok = @atomicLoad(bool, &flag, .monotonic);
  // then reading data that the producer wrote BEFORE storing flag — no happens-before
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  // producer:
  @atomicStore(u32, &data, 42, .release);
  @atomicStore(bool, &flag, true, .release);
  // consumer:
  if (@atomicLoad(bool, &flag, .acquire)) {
      const value = @atomicLoad(u32, &data, .acquire); // sees 42
  }
  ```
- **VERIFICATION**: `zig test examples/good/atomic_counter.zig`; ordering-correctness
  itself is reasoned, matching `memory-ordering-reasoning` (KNOWN: same model as C11).
- **SOURCE**: zig-langref §Atomics, §Builtin Functions (@atomicLoad/@atomicStore/
  @atomicRmw/@cmpxchgStrong/@cmpxchgWeak); zig-std-source (std/lang/AtomicOrder or
  std/builtin.zig per version).

## 4. Thread.Pool removed in 0.16

- **RULE**: `std.Thread.Pool` was removed in 0.16.0. Threaded execution is provided by
  `std.Io.Threaded` (INFERRED surface) or your own pool built on `std.Thread`.
- **WHY AI GETS IT WRONG**: writes `var pool: std.Thread.Pool = undefined; try
  pool.init(...)` from 0.15 tutorials — fails on 0.16+.
- **CORRECT REASONING**: check the pinned std/Io/Threaded.zig (or the LangRef/Io docs) for
  the current name; do not guess fields.
- **EXAMPLE** (bad, fails on 0.16+):
  ```zig
  var pool: std.Thread.Pool = undefined;
  try pool.init(.{ .allocator = gpa });
  ```
- **COUNTEREXAMPLE** (good, 0.16+):
  ```zig
  // consult std/Io/Threaded.zig for the pinned version — INFERRED:
  // var threaded = try std.Io.Threaded.init(io, gpa);
  ```
- **VERIFICATION**: `zig test examples/bad/thread_pool_016.zig` fails on 0.16+ with a
  missing-`Thread.Pool` error (exact text UNVERIFIED).
- **SOURCE**: zig-release-notes 0.16.0 (Thread.Pool Removed); zig-std-source
  (std/Io/Threaded.zig).

## 5. async/await are gone (0.15); concurrency lives in std.Io

- **RULE**: `async`/`await` keywords and `@frameSize` were removed in 0.15.0; the evented
  model moved into the `std.Io` interface (I/O as an Interface), where an `Io` instance is
  passed through call chains like an `Allocator`.
- **WHY AI GETS IT WRONG**: writes `async fn foo()` from pre-0.15 material, or expects
  `std.fs` to do blocking I/O without an `io` parameter.
- **CORRECT REASONING**: I/O functions now take `io: *std.Io` (or use the instance from
  `std.process.Init`); blocking and evented backends are implementations of the same
  interface.
- **EXAMPLE** (bad, fails on 0.15+):
  ```zig
  async fn readFile() ![]u8 { ... }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  pub fn main(init: std.process.Init) !void {
      const io = init.io;
      const contents = try std.Io.Dir.cwd().readFileAlloc(io, "f.txt", init.arena.allocator(), .{} );
  }
  ```
- **VERIFICATION**: `zig test examples/good/main_016.zig` (from zig-version-migration)
  demonstrates the io-passing pattern.
- **SOURCE**: zig-release-notes 0.15.1 (async and await keywords removed); zig-langref
  §Async Functions (regressed; plan: stackless coroutines in the Io interface).

## 6. io_uring via std.Io.Linux.Uring

- **RULE**: io_uring is Linux-only: an SQ (submission queue) and CQ (completion queue)
  shared with the kernel, entered via `io_uring_enter`. Zig wraps it as
  `std.Io.Linux.Uring` (INFERRED API surface; Linux target only).
- **WHY AI GETS IT WRONG**: pastes liburing C code into Zig; claims it works on Windows/
  macOS; ignores that each `Io` instance owns its uring.
- **CORRECT REASONING**: learn the SQ/CQ mechanics from io-uring-docs, then read
  std/Io/Linux/Uring.zig for the exact wrapper; guard usage with
  `@import("builtin").target.os.tag == .linux`.
- **EXAMPLE** (bad):
  ```zig
  const ring = std.Io.Linux.Uring.init(32); // invented signature
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  // read the pinned std/Io/Linux/Uring.zig before writing; guard with a Linux check:
  if (builtin.target.os.tag == .linux) { /* verified wrapper usage here */ }
  ```
- **VERIFICATION**: cross-compile `zig build -Dtarget=x86_64-linux-gnu test`; execution
  requires Linux or QEMU (absent on this host) — UNVERIFIED here.
- **SOURCE**: io-uring-docs (submission/completion queues); zig-std-source
  (std/Io/Linux/Uring.zig).

## 7. Single-threaded builds and threadlocal

- **RULE**: `@import("builtin").single_threaded` is true under `-fsingle-threaded`; in
  that mode `threadlocal` variables act as ordinary globals. Single-threaded correctness
  is a valid end state — adding threads "for parallelism" without a measured need is fake
  parallelism.
- **WHY AI GETS IT WRONG**: flags correct single-threaded code as "needs threads"
  (CONCUR's single-thread failure category), or adds a thread pool that is never actually
  used concurrently.
- **CORRECT REASONING**: first determine the actual concurrency requirement; a program
  that is correct single-threaded should stay single-threaded unless profiling demands
  otherwise.
- **EXAMPLE** (bad): a sequential pipeline refactored to spawn one worker that does the
  same work in the same order — no parallelism gained, all race risk added.
- **COUNTEREXAMPLE** (good):
  ```zig
  test "single-threaded is correct" {
      // genuinely sequential computation, no threads involved
      try std.testing.expectEqual(@as(u32, 6), sumOneToThree());
  }
  ```
- **VERIFICATION**: `zig build test -fsingle-threaded` passes for the good case; the
  refactor adds no measurable speedup.
- **SOURCE**: zig-langref §Single Threaded Builds, §Thread Local Variables.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Threads | `std.Thread.spawn(.{}, fn, .{args})` + `join()`; join what you spawn |
| Races | Illegal Behavior; one mechanism per variable (atomic or lock) |
| Atomics | `@atomicRmw`/`@atomicLoad`/`@atomicStore`/`@cmpxchg*` + explicit `AtomicOrder` |
| Ordering | `.monotonic`/`.acquire`/`.release`/`.acq_rel`/`.seq_cst` — same model as C11 |
| Thread.Pool | removed in 0.16 → `std.Io.Threaded` or own pool |
| async/await | removed in 0.15 → `std.Io` interface |
| io_uring | Linux-only; `std.Io.Linux.Uring` (INFERRED surface); SQ→CQ, `io_uring_enter` |
| Single-threaded | `builtin.single_threaded`; `threadlocal` = global; correct ST code stays ST |
