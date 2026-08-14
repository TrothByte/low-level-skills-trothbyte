# Evaluation — atomics-c11-cpp11-rust

Skill: `skills/concurrency/atomics-c11-cpp11-rust`. Stability target: `evaluated`.
Scope: the API layer (operation semantics, CAS expected in-out, weak vs strong,
memory-order validity, lock-free guarantees, ABI). Ordering *choice* (which order forms
which happens-before edge) is evaluated by `memory-ordering-reasoning`.

## Synthetic evals

- **easy/positive**: C11/C++20/Rust CAS retry loop that updates `expected` from the
  failure path — must be accepted as correct.
- **easy/negative**: `compare_exchange_weak` one-shot without a loop — must flag (spurious
  failure).
- **medium/negative**: C/C++ code that reads `expected` as the pre-CAS assumption after a
  failed CAS — must flag (expected is in-out).
- **medium/negative**: Rust `store(Ordering::Acquire)` — must flag (load-only order;
  rustc 1.97 rejects it at compile time via `invalid_atomic_ordering`, or panics at runtime
  if the lint is allowed).
- **hard/negative**: signal-handler path using `atomic_int` with no `ATOMIC_INT_LOCK_FREE`
  check — must flag (lock-free is not guaranteed).
- **cross-language**: translate the same publish/consume protocol between C11, C++20, and
  Rust — semantics (release store, acquire load) must survive the port, including the Rust
  `Result` payload translation.

## Adversarial evals

- **AD-01 (fixed-but-still-broken)**: a "corrected" one-shot gate using
  `compare_exchange_strong` that still reuses the stale expected value on the failure path
  to decide retry state. Agent must spot that the post-failure value (not the assumption)
  drives the retry.
- **AD-02 (compiles but UB)**: C++ `compare_exchange_strong(e, d, acq_rel, release)` — the
  invalid failure order is accepted silently by g++; agent must know the failure-order
  precondition and the single-order overload's mapping (acq_rel -> acquire, release ->
  relaxed).
- **AD-03 (sanitizer-blind)**: C/C++ volatile-flag sync that is ASan-clean — agent must know
  only TSan/Miri catch it.

## False-positive evals

- Correct CAS loop with refreshed `expected` — must NOT be flagged.
- `Relaxed` stats counter (`fetch_add`) — legitimate, must NOT be flagged.
- `compare_exchange_strong` one-shot — legitimate, must NOT be flagged for lacking a loop.
- `atomic_flag` usage — guaranteed lock-free, must NOT be flagged for missing `is_lock_free`.
- Rust `is_lock_free()` query — must NOT be "fixed" by assuming lock-free.

## Verification commands (toolchain: gcc 16.1, g++ 16.1, rustc 1.97.1)

```
gcc  -std=c11  -Wall -Wextra -Werror -O2 examples/good/c11_atomic_good.c   -o out && ./out
g++  -std=c++20 -Wall -Wextra -Werror -O2 examples/good/cpp20_atomic_good.cpp -o out && ./out
rustc --edition 2021 examples/good/rust_atomic_good.rs -o out && ./out
# bad examples must COMPILE (that is the trap); run documents the semantics
gcc  -std=c11  -Wall -Wextra -Werror -O2 examples/bad/c11_atomic_bad.c     -o out && ./out
g++  -std=c++20 -Wall -Wextra -Werror -O2 examples/bad/cpp20_atomic_bad.cpp -o out && ./out
rustc --edition 2021 examples/bad/rust_atomic_bad.rs -o out && ./out   # panics: see below
# asm (x86-64): verify atomicity/ordering lowering
rustc --edition 2021 --emit=asm -C opt-level=2 examples/good/rust_atomic_good.rs
```

## Verified facts (2026-08-14, x86-64 Windows/MSYS2)

- All three good examples exit 0. C11: `publish_once r=1 slot=1`, `claim_once=1`,
  `hits=3`, `consume=42`, `lockfree_ok=1`. C++20: identical semantics, plus
  `is_always_lock_free=1`. Rust: identical semantics.
- `ATOMIC_INT_LOCK_FREE == 2` on this target (C11 good example prints `lockfree_ok=1`);
  `std::atomic<int>::is_always_lock_free` is true.
- C11 bad: `assume_unchanged=2` (expected was overwritten), `one_shot_weak=1`,
  `consume_bad=42` (single-threaded demo — the race is latent), `consume_v=1`, `count=1`.
- C++20 bad: `assume_unchanged=2`, `one_shot_weak=1`, `bad_failure_order=1` (UB, accepted
  silently by g++), `consume_bad=42`, `consume_v=1`.
- Rust bad: prints `stale_current=0 slot=5` (the stale-value bug), then panics at
  `store_with_acquire` with `panicked at 'there is no such thing as an acquire store'`,
  exit code 101. Note the double guard: rustc 1.97 first rejects acquire stores at COMPILE
  time via the deny-by-default `invalid_atomic_ordering` lint; the bad example carries
  `#[allow(invalid_atomic_ordering)]` so the runtime panic can be demonstrated.
- Rust 1.97.1 API fact: `AtomicI32::is_lock_free()` no longer exists (E0599) — the std docs
  now guarantee all available atomic types are lock-free ("Portability" section), so the
  cross-language lock-free check applies to C/C++ only; in Rust the portability check is
  `cfg(target_has_atomic)`.
- Asm on x86-64 (rustc 1.97.1, opt-level 2): CAS -> `lock cmpxchgl`/`lock cmpxchgb`;
  `fetch_add` -> `lock incl` (LLVM picked `inc`; the sibling skill observed `lock xadd`);
  the invariant is the `lock` prefix on every RMW. On x86-64 TSO, store(SeqCst) -> `xchg`,
  load(Acquire) -> plain `mov` (see `memory-ordering-reasoning` verified-asm table).

## Scoring

- detection: names the bug class (expected in-out, spurious failure, invalid order,
  lock-free assumption, volatile-as-atomic).
- reasoning: explains the operation kind / order-validity rule, not "use a stronger order".
- fix: minimal correct change (add loop, use strong, refresh expected, check lock-free),
  does not over-strengthen to SeqCst blindly.
- verification: cites compile/run evidence, asm, or TSan/Miri per language.
