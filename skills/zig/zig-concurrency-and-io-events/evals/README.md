# Evaluation — zig-concurrency-and-io-events

Skill: `skills/zig/zig-concurrency-and-io-events`.
Stability target: `researched`. Toolchain: zig is NOT installed on this host (Windows);
io_uring execution additionally requires Linux. Verification commands below are the
recorded plan, not run results.

## Synthetic evals

| Case | Fixture | Expected | Command |
|---|---|---|---|
| easy/negative | `bad/async_keyword.zig` | fails on 0.15+: `async` removed | `zig test` |
| easy/negative | `bad/thread_pool_016.zig` | fails on 0.16+: `Thread.Pool` removed | `zig test` |
| medium/negative | `bad/data_race.zig` | may "pass" — flagged by reasoning (race) | `zig test` + review |
| positive | `good/threads.zig` | passes; spawn/join + threadlocal | `zig test` |
| positive | `good/atomic_counter.zig` | passes; `@atomicRmw(.Add)` | `zig test` |
| positive | `good/single_threaded.zig` | passes; ST code must NOT be flagged | `zig test` |

## False-positive evals (correct code must not be flagged)

- `good/single_threaded.zig` — correct single-threaded code must not be flagged as needing
  threads (CONCUR single-thread category).
- `good/atomic_counter.zig` — `@atomicRmw(..., .Add, 1, .monotonic)` is correct and not
  "over-engineered".
- `good/threads.zig` — `threadlocal` per-thread state and joined spawn/join pairs.
- `builtin.single_threaded`-aware code and `-fsingle-threaded` builds — correct.

## Historical evals

- 0.15.0: `async`/`await` keywords and `@frameSize` removed — `bad/async_keyword.zig`.
- 0.16.0: `std.Thread.Pool` removed — `bad/thread_pool_016.zig`; I/O as an Interface
  (Writergate follow-through) made `std.Io` the evented abstraction.
- 0.16.0: `heap.ArenaAllocator` became thread-safe and lock-free — arena-sharing claims
  must be version-dated.

## Adversarial evals

- A data race that passes CI deterministically because the increment interleaving never
  collides in short runs — the gate is construction-based reasoning (single mechanism per
  variable), not test luck.
- A "thread pool" that spawns one worker performing the same sequential work — fake
  parallelism; must be flagged.
- An io_uring program claimed portable to Windows/macOS — must be gated on
  `builtin.target.os.tag == .linux` (verification on Linux/QEMU, absent here).

## Verified facts

- KNOWN (from release notes and langref; not run on this host):
  - `async`/`await` and `@frameSize` removed in 0.15.0; async model moves into the
    `std.Io` interface.
  - `std.Thread.Pool` removed in 0.16.0.
  - Data races are Illegal Behavior; `@atomicRmw`/`@cmpxchg*`/`@atomicLoad`/`@atomicStore`
    take an `AtomicOrder` (ordering model matches C11/Rust — `memory-ordering-reasoning`).
  - `threadlocal` variables become ordinary globals in single-threaded builds.
- INFERRED: `std.Io.Threaded` and `std.Io.Linux.Uring` API surface (version-fluctuating;
  verify against std/Io/Threaded.zig and std/Io/Linux/Uring.zig per pin); `AtomicOrder`
  namespace moved from `std.builtin` to `std.lang` between 0.15 and 0.16.
- UNVERIFIED (needs zig + Linux on this host): io_uring wrapper behavior; exact
  `Thread.Pool`/`async` diagnostics.

## Target toolchains (absent, documented)

- zig 0.15.2 / 0.16.0 / 0.17.0-dev: not installed. io_uring verification requires a Linux
  host or QEMU (absent). Cross-compile plan: `zig build -Dtarget=x86_64-linux-gnu test`.
