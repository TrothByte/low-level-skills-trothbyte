# Zig Allocators and Memory Management — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.
Version markers: KNOWN / INFERRED / UNVERIFIED.

## 1. Allocator as an explicit parameter — no hidden allocation

- **RULE**: Zig has no default allocator. Any function or library that allocates must take
  an `Allocator` parameter (or document a fixed policy). The flow chart in the langref
  starts from "Are you making a library? Accept an Allocator".
- **WHY AI GETS IT WRONG**: writes `fn parse() []const u8 { ... allocate ... }` with no
  allocator argument, mirroring languages with a hidden global heap.
- **CORRECT REASONING**: the caller must choose the allocator so it can control lifetime,
  performance, and failure behavior; code without an allocator parameter cannot be reused
  in embedded or overcommit-free contexts.
- **EXAMPLE** (bad):
  ```zig
  fn duplicate(s: []const u8) []u8 {
      const buf = std.heap.page_allocator.dupe(u8, s) catch unreachable;
      return buf; // no Allocator parameter: caller cannot free or pick a policy
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn duplicate(allocator: std.mem.Allocator, s: []const u8) ![]u8 {
      return allocator.dupe(u8, s); // "caller owns the returned memory"
  }
  ```
- **VERIFICATION**: review + `zig test` with `std.testing.allocator`; a hidden-allocation
  function compiles fine but the reviewer must flag it.
- **SOURCE**: zig-langref §Memory (Choosing an Allocator), §Memory (Lifetime and Ownership).

## 2. Choosing an allocator follows the langref flow chart

- **RULE**: libc-linked code → `std.heap.c_allocator`; max bytes comptime-known →
  `std.heap.FixedBufferAllocator`; all-or-nothing lifetime (CLI, request handler) →
  `std.heap.ArenaAllocator` (bounded variant: FixedBufferAllocator over an arena chunk);
  tests → `std.testing.allocator`; proving OutOfMemory handling →
  `std.testing.FailingAllocator`; otherwise a general purpose allocator: in Debug
  `std.heap.DebugAllocator`, in ReleaseFast `std.heap.smp_allocator` (master-era names;
  `GeneralPurposeAllocator` was the 0.15-era spelling).
- **WHY AI GETS IT WRONG**: defaults everything to a G.P. allocator; or pastes the
  0.14-era `GeneralPurposeAllocator(.{})` into 0.16/0.17 code where the recommended names
  changed; or uses an arena for a long-lived object graph that must shrink.
- **CORRECT REASONING**: match allocator to lifetime. Arenas free all at once — great for
  per-request/CLI lifetimes, wrong for objects that must individually die early.
  FixedBufferAllocator never touches the heap — right when the bound is comptime-known.
- **EXAMPLE** (bad):
  ```zig
  var gpa = std.heap.GeneralPurposeAllocator(.{}){}; // 0.15-era; see KNOWN note
  defer _ = gpa.deinit();
  ```
- **COUNTEREXAMPLE** (good, 0.16/0.17):
  ```zig
  var debug_allocator = std.heap.DebugAllocator(.{}){};
  defer _ = debug_allocator.deinit();
  const gpa = debug_allocator.allocator();
  ```
- **VERIFICATION**: `zig build test`; exact `DebugAllocator` config fields and whether
  `GeneralPurposeAllocator` still exists in 0.16.0 — INFERRED (version-sensitive, check
  the pinned std/heap.zig).
- **SOURCE**: zig-langref §Memory (Choosing an Allocator); zig-std-source (std/heap.zig);
  zig-release-notes 0.16.0 (heap.ThreadSafe Allocator Removed).

## 3. std.testing.allocator detects leaks

- **RULE**: the default test runner reports leaks from `std.testing.allocator` (0.16:
  "[SafeAllocator] (err): leaked [...]"); a leaking test still prints "All 1 tests passed"
  followed by "1 tests leaked memory" and exits nonzero. `error.SkipZigTest` skips; use
  `@import("builtin").is_test` to detect a test build.
- **WHY AI GETS IT WRONG**: claims a test "passed" while ignoring the leak report; or uses
  `page_allocator` in tests so leaks are invisible.
- **CORRECT REASONING**: pair every allocation with `defer ...deinit(...)`. Unmanaged
  containers (0.15+ default) take the allocator per call: `var list: std.ArrayList(u21) =
  .empty; try list.append(gpa, x); defer list.deinit(gpa);`.
- **EXAMPLE** (bad):
  ```zig
  test "detect leak" {
      const gpa = std.testing.allocator;
      var list: std.ArrayList(u21) = .empty;
      try list.append(gpa, 'x'); // missing defer list.deinit(gpa);
      try std.testing.expectEqual(@as(usize, 1), list.items.len);
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  test "no leak" {
      const gpa = std.testing.allocator;
      var list: std.ArrayList(u21) = .empty;
      defer list.deinit(gpa);
      try list.append(gpa, 'x');
      try std.testing.expectEqual(@as(usize, 1), list.items.len);
  }
  ```
- **VERIFICATION**: `zig test examples/bad/leak.zig` reports a leak and fails;
  `zig test examples/good/leak_detection.zig` passes.
- **SOURCE**: zig-langref §Zig Test (Report Memory Leaks); zig-std-source (std/testing.zig).

## 4. Free exactly once, with the allocator that allocated

- **RULE**: every allocation has exactly one free. Mixing allocators or freeing twice is
  Illegal Behavior; debug allocators track live allocations and detect double-free and
  use-after-free (0.15/0.16 DebugAllocator-based testing allocator; the 0.16.0
  `ArenaAllocator` is thread-safe and lock-free, so a single owner may share it across
  threads — but `deinit` must still be called exactly once).
- **WHY AI GETS IT WRONG**: calls `destroy` twice, or `arena.deinit()` in two error paths,
  and reports an opaque crash; or assumes double-free silently corrupts like C.
- **CORRECT REASONING**: `defer` guarantees once-per-scope-exit; for error paths use
  `errdefer` so the free runs only when the error propagates. On an arena, allocation is
  `arena.allocator()`; freeing is only ever `arena.deinit()` once.
- **EXAMPLE** (bad):
  ```zig
  test "double free" {
      const gpa = std.testing.allocator;
      const p = try gpa.create(u32);
      gpa.destroy(p);
      gpa.destroy(p); // double free: detected by the testing allocator
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  test "single free" {
      const gpa = std.testing.allocator;
      const p = try gpa.create(u32);
      defer gpa.destroy(p);
      p.* = 1;
      try std.testing.expectEqual(@as(u32, 1), p.*);
  }
  ```
- **VERIFICATION**: `zig test examples/bad/double_free.zig` fails; good passes.
- **SOURCE**: zig-langref §Memory (Lifetime and Ownership), §Illegal Behavior;
  zig-release-notes 0.16.0 (heap.ArenaAllocator thread-safe and lock-free).

## 5. Slice lifetimes inside containers

- **RULE**: `std.ArrayList(T).items` is valid only until the next resize (append, insert,
  shrink). Taking a pointer into `items` and using it after an `append` is use-after-free
  territory. The langref documents this explicitly.
- **WHY AI GETS IT WRONG**: stores `const p = &list.items[0]`, appends, then reads `p.*` —
  the buffer may have been reallocated and copied.
- **CORRECT REASONING**: either re-fetch `list.items[i]` after each mutation, or avoid
  resizes while holding references. Ownership and lifetime are the two questions to answer
  for every pointer: who frees, and when does it go stale?
- **EXAMPLE** (bad):
  ```zig
  var list: std.ArrayList(u32) = .empty;
  defer list.deinit(gpa);
  try list.append(gpa, 1);
  const p = &list.items[0];
  try list.append(gpa, 2);      // may move the buffer
  std.debug.print("{d}\n", .{p.*}); // stale read
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  var list: std.ArrayList(u32) = .empty;
  defer list.deinit(gpa);
  try list.append(gpa, 1);
  try list.append(gpa, 2);
  std.debug.print("{d}\n", .{list.items[0]}); // re-fetch after mutations
  ```
- **VERIFICATION**: review-time rule; `zig test` in Debug usually still "passes" — the
  staleness is caught by review, not by the compiler (UNVERIFIED: may trip the Debug
  allocator's freed-memory guards depending on capacity growth).
- **SOURCE**: zig-langref §Memory (Lifetime and Ownership).

## 6. error.OutOfMemory is data, not doom

- **RULE**: allocation failure is `error.OutOfMemory` and Zig libraries propagate it with
  `try`. Handling it is a feature (test with `std.testing.FailingAllocator`), not
  busywork, because not all systems overcommit.
- **WHY AI GETS IT WRONG**: writes `catch unreachable` everywhere, or claims OOM handling
  is pointless on Linux because of overcommit.
- **CORRECT REASONING**: Windows does not overcommit; embedded and real-time systems do
  not; libraries must be reusable in all of them. OOM is an error value, exactly like
  `error.NoSpaceLeft`.
- **EXAMPLE** (bad):
  ```zig
  fn build_path(allocator: std.mem.Allocator, base: []const u8) []u8 {
      return allocator.dupe(u8, base) catch unreachable; // hides OOM
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn build_path(allocator: std.mem.Allocator, base: []const u8) ![]u8 {
      return allocator.dupe(u8, base);
  }
  ```
- **VERIFICATION**: test with `std.testing.FailingAllocator`; `zig test` asserts the
  error path returns `error.OutOfMemory`.
- **SOURCE**: zig-langref §Memory (Heap Allocation Failure); zig-std-source
  (std/testing.zig — FailingAllocator).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| API design | allocate only via an `Allocator` parameter; document ownership |
| Library | accept `Allocator`; never pick a global policy |
| CLI / request cycle | `ArenaAllocator` + `defer arena.deinit()` |
| comptime-known bound | `FixedBufferAllocator` — never touches the heap |
| Tests | `std.testing.allocator` (leaks reported); `FailingAllocator` for OOM |
| General purpose | Debug: `DebugAllocator`; ReleaseFast: `smp_allocator` (0.16+ names) |
| Free | exactly once, with the same allocator; `defer`/`errdefer` pair every alloc |
| Containers | unmanaged by default (0.15+); pass `gpa` per call; items stale after resize |
| OOM | propagate `error.OutOfMemory` with `try`; verify with `FailingAllocator` |
