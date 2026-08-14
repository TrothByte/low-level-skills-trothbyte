# Evaluation — safe-low-level-from-scratch

Skill: `skills/_meta/safe-low-level-from-scratch`. Stability target: `evaluated`.

## Synthetic evals

- **easy/positive**: write a function copying a bounded string safely (`snprintf`/explicit
  termination) — must pass `-Werror + ASan/UBSan -O2`.
- **medium/positive**: write a bounded ring buffer from a spec (see examples/good/ringbuffer.c)
  — must pass runtime asserts + sanitizers.
- **hard/positive**: write a publish/consume flag protocol — must pick Release/Acquire, not
  Relaxed, and not SeqCst-unnecessarily.
- **adversarial**: spec with hidden traps — unbounded input, overlapping copy opportunity,
  misaligned FFI field — agent must design around them (checked sizes, memmove, explicit
  packing), not patch symptoms.

## Verified artifact

`examples/good/ringbuffer.c`:
- Compiles `-Wall -Wextra -Werror -O2` clean (GCC 16.1).
- Passes runtime asserts: 8-byte ring, write 5+1+2 (only 2 accepted), read 8, content
  `"hello!XY"` round-trip correct.
- Design: `size_t` indexes, checked allocation, space check before write, two-segment
  memcpy with no overlap by construction, `used` invariant.

`examples/bad/ringbuffer.c` (must be rejected on review): `int` indexes, off-by-one loop,
no space/overflow checks, `memcpy` with unchecked bounds — every one maps to a bug class
(A1, A2, A3, A9).

## Scoring

- process: follows Steps 1-7 (contract → types → arithmetic → memory → concurrency →
  boundary → verify), not "write then patch".
- correctness: passes sanitizer gates at `-O2`.
- traps: hidden-trap spec produces no UB; assumptions surfaced.
- FP: correct code is not over-flagged.
