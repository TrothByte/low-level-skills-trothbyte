# Evaluation — embedded-volatile-and-memory-ordering

Skill: `skills/embedded/embedded-volatile-and-memory-ordering`.
Stability target: `evaluated`.

## Verification commands (host, GCC 16.1 on PATH)

```
# good examples must exit 0
gcc -Wall -Wextra -Werror -O2 examples/good/mmio_volatile.c -o out && ./out
gcc -Wall -Wextra -Werror -O2 examples/good/isr_flag_volatile.c -pthread -o out && ./out

# bad examples reproduce the failure (nonzero exit)
gcc -Wall -Wextra -Werror -O2 examples/bad/mmio_no_volatile.c -o out && ./out   # exits 0, bug is in the asm
gcc -Wall -Wextra -Werror -O2 examples/bad/isr_flag_no_volatile.c -pthread -o out && ./out   # exits 1
gcc -Wall -Wextra -Werror -O2 examples/bad/volatile_not_atomic.c -pthread -o out && ./out    # exits 1

# the canonical demonstration: compiler caches/removes the load without volatile
gcc -O2 -S examples/bad/mmio_no_volatile.c -o bad.s
gcc -O2 -S examples/good/mmio_volatile.c -o good.s
```

## Synthetic evals

- **easy/negative**: a volatile MMIO read helper used through a volatile pointer
  — must NOT be flagged; every access is preserved in asm.
- **medium/negative**: MMIO double-poll without `volatile` — agent must predict
  that `-O2` folds the two loads into one (register caching) and fix the
  pointer qualifier.
- **hard/negative**: ISR flag protocol — agent must identify missing `volatile`
  and explain why the poll loop is removed at `-O2`; must also NOT convert it
  to atomics unless there are two writers.
- **ambiguous**: a `volatile` ISR flag vs a `_Atomic` flag — correct answer is
  "volatile is fine for single-producer/single-consumer on one core"; atomics
  only when two threads/cores write.

## False-positive evals

- A correct `volatile` MMIO accessor (`__IOM`-style) — must NOT be flagged as a
  race or "needs atomics".
- A single-producer/single-consumer volatile ISR flag — must NOT be flagged.
- A non-`volatile` one-shot read of a packed boot header (plain data) — must
  NOT be flagged as a missing volatile.

## Adversarial evals

- **AD (compiles and passes at -O0)**: firmware that polls a status register
  with a non-volatile pointer; the agent must show `-O2 -S` evidence that the
  second load is cached and that a missing barrier would let the device see
  reordered config/start writes.
- **AD (volatile as atomic)**: a shared counter "fixed" with `volatile` by a
  previous agent; the eval expects identification of the lost-update race and a
  fix to atomics/locks, with TSan or the lost-update demo as evidence.

## Verified facts (recorded 2026-08-14, GCC 16.1 x86-64 MinGW)

1. MMIO double-poll: non-volatile `poll_twice` compiles to
   `movl 4(%rax),%eax; addl %eax,%eax` (ONE load, second read folded); the
   volatile variant emits two loads: `movl 4(%rdx),%eax; addl 4(%rdx),%eax`.
   With a static (never-written) array the whole non-volatile function becomes
   just `ret` (loads eliminated entirely).
2. ISR flag poll: non-volatile `while (*f == 0) {}` collapses to a single
   entry-time load; the loop never re-reads, so the ISR store is missed and the
   program times out (exit 1). The volatile variant reloads
   `movl flag(%rip),%edx` every iteration and exits 0.
3. volatile-not-atomic: two threads doing read/usleep/write on a volatile
   counter finish at ~50 (of 100 expected) — updates lost, 3/3 runs.

## Scoring

- detection: names the missing `volatile`/qualifier drop or the false atomic
  assumption, and the `-O2` optimization responsible (CSE/hoisting/elimination).
- reasoning: separates the three mechanisms — volatile (access preservation),
  barriers (ordering), atomics (atomicity) — and applies the right one.
- fix: minimal correct change; does not add atomics to an SPSC ISR flag and
  does not add barriers where ordering is not required.
- verification: uses `-O2 -S` asm evidence and/or the runnable examples.
