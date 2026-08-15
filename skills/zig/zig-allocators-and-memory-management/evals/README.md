# Evaluation — zig-allocators-and-memory-management

Skill: `skills/zig/zig-allocators-and-memory-management`.
Stability target: `researched`. Toolchain: zig is NOT installed on this host; the code
targets the 0.15–0.17 API surface (verified against langref master, std/heap.zig and the
0.16.0 release notes). Verification commands below are the recorded plan, not run results.

## Synthetic evals

| Case | Fixture | Expected | Command |
|---|---|---|---|
| easy/negative | `bad/double_free.zig` | testing allocator detects double destroy (fails) | `zig test` |
| easy/negative | `bad/leak.zig` | runner reports leak, exit nonzero | `zig test` |
| medium/negative | `bad/hidden_alloc.zig` | compiles but must be flagged by review | `zig test` + review |
| medium/negative | review case | stale `list.items` pointer used after append | review |
| positive | `good/cli_arena.zig` | passes; arena pattern | `zig test` |
| positive | `good/fixed_buffer.zig` | passes; bounded + OOM case | `zig test` |
| positive | `good/leak_detection.zig` | passes; zero leaks | `zig test` |

## False-positive evals (correct code must not be flagged)

- `good/cli_arena.zig` — `defer arena.deinit()` at end of main: not a leak, it is the pattern.
- `good/fixed_buffer.zig` — fixed-buffer `error.OutOfMemory` on exhaustion is correct, not a bug.
- Deliberate `error.OutOfMemory` propagation with `try` — correct convention.
- Re-fetching `list.items[i]` after a documented resize — correct; do not flag as "redundant".

## Historical evals

- 0.16.0: `heap.ArenaAllocator` became thread-safe and lock-free; `heap.ThreadSafeAllocator`
  was removed. Claims that arenas are single-thread-only are stale on 0.16+.
- 0.16.0: migration to "Unmanaged" containers — `heap.MemoryPoolUnmanaged` added;
  `PriorityQueue`/`PriorityDequeue` lost their allocator field. Code using the managed
  variant must be migrated.
- Langref master recommends `DebugAllocator`/`smp_allocator`; 0.15-era
  `GeneralPurposeAllocator` naming must be checked per pinned version.

## Adversarial evals

- A leak/double-free fixture specifically constructed to bypass the testing allocator
  (e.g. allocating with `page_allocator` inside a test) — must be caught by review.
- A function that returns an arena-allocated pointer after the caller already ran
  `arena.deinit()` — the ownership rule "caller owns / arena owns" must be traced.
- An `errdefer`/`defer` pair that frees twice on the error path (defer + errdefer both
  freeing the same pointer) — exactly-once rule.

## Verified facts

- KNOWN (from langref and 0.16.0 release notes; not run on this host):
  - The default test runner reports leaks from `std.testing.allocator` and exits nonzero
    ("1 tests leaked memory").
  - `std.testing.FailingAllocator` exists for exercising `error.OutOfMemory`.
  - 0.16.0 made `heap.ArenaAllocator` thread-safe and lock-free; removed
    `heap.ThreadSafeAllocator`; added `heap.MemoryPoolUnmanaged`.
  - Langref flow chart: library→Allocator param; libc→`c_allocator`; comptime bound→
    `FixedBufferAllocator`; CLI/cycle→`ArenaAllocator`; tests→`std.testing.allocator`;
    general purpose→`DebugAllocator` (Debug) / `smp_allocator` (ReleaseFast).
- UNVERIFIED (needs zig on this host): exact leak/double-free diagnostic text; whether
  `GeneralPurposeAllocator` is still present in 0.16.0; whether the stale-items case trips
  DebugAllocator guards.

## Target toolchains (absent, documented)

- zig 0.15.2 / 0.16.0 / 0.17.0-dev: not installed. First execution plan: install via
  `zigup` or ziglang.org/download, then run the commands in SKILL.md §How to verify.
