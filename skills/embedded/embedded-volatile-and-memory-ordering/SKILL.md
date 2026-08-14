---
name: embedded-volatile-and-memory-ordering
description: Use when writing, reviewing, or debugging embedded C that accesses memory-mapped I/O (MMIO) registers or shares flags with an interrupt handler — volatile vs non-volatile access, why -O2 changes behavior, why volatile is not atomic, device vs normal memory attributes, and barriers for ordering. Teaches volatile rules and verification.
---

# Embedded volatile & Memory Ordering for MMIO and ISR Flags

## When to use

- Writing or reviewing firmware that reads/writes MMIO registers (status,
  control, FIFO) through pointers or CMSIS-style accessors.
- Sharing a flag or data word between an ISR and the main loop on one core.
- Debugging "works at -O0, hangs or misbehaves at -O2".
- Deciding whether a register access needs `volatile` and whether it also
  needs a barrier (ordering), and why the two are different questions.
- Reviewing driver code that uses `readl`/`writel`/`__IOM`-style accessors or
  hand-rolled pointer casts.

## When not to use

- Inter-thread synchronization on multi-threaded/MP systems (mutexes, C11
  atomics, `std::atomic`) — use `memory-ordering-reasoning`.
- Kernel memory barriers (`smp_mb`, RCU, `dma_*` API) — use
  `kernel-rcu-memory-barriers`.
- A one-shot memory-mapped buffer that is only read once by software (plain
  data, e.g. a packed boot header) — `volatile` is not needed there.
- Choosing cacheability/attribute registers on ARMv8-M MPU (device vs normal
  MAIR slots) — see `embedded-mpu-trustzone`.

## What the agent often gets wrong

- "`volatile` makes the access atomic and thread-safe." It provides neither;
  a `volatile` counter still loses updates under a race.
- "It compiles and passes tests at -O0, so it is correct." The optimizer only
  exploits the missing `volatile` at higher optimization levels.
- "`volatile` alone gives ordering." It forces the access to memory but places
  no ordering on surrounding accesses; hardware/compiler reordering still needs
  a barrier.
- "Any cast to a non-volatile pointer is harmless." Casting away `volatile`
  silently restores the bug (load caching/elimination).
- "MMIO is just memory, so normal accesses work." MMIO is device memory: side
  effects on every read/write, so the compiler must not merge, reorder, or drop
  accesses, and the hardware must be mapped as device (non-cacheable).
- "`-O2` only affects speed." Without `volatile`, `-O2` can remove the polling
  loop entirely or read a register only once.
- "Volatile fixes UB." It does not; it only prevents the optimizer from
  assuming it may elide/reorder *that object's* accesses.

## How to reason correctly

1. Classify the object: a hardware register (MMIO) → always access through a
   `volatile`-qualified pointer/accessor; a flag shared with an ISR on the same
   core → `volatile`; a flag shared between threads/cores → atomics.
2. Ask "who writes, who reads, when": single writer, single reader, no
   concurrent writers → `volatile` is sufficient. Two or more threads both
   writing (or one writing while another writes) → atomics, because it is a
   data race.
3. Separate two questions: (a) will the compiler preserve the access?
   (`volatile`) and (b) in what order do accesses become visible to the device
   or other cores? (barriers / memory ordering).
4. Check the memory attribute: map MMIO as device/non-cacheable in the MMU/MPU;
   normal SRAM shared with the device is fine as normal memory but still needs
   `volatile` (or atomics) plus ordering for flags.
5. Verify at `-O2` by inspecting asm, never only at `-O0`.

## What to verify

- Every MMIO pointer and accessor is `volatile`-qualified; no
  non-volatile casts on the device path.
- ISR-shared flags are `volatile` (or `_Atomic` for multi-thread/MP), and the
  protocol is single-producer/single-consumer.
- Ordering requirements are explicit: a DMB/DSB or compiler barrier where the
  device or a peer core must observe accesses in order.
- At `-O2`, the asm still performs one memory access per source-level access
  (no cached register reuse, no eliminated polls).

## How to verify

```
gcc -O2 -S examples/good/mmio_volatile.c -o -     # two loads per double poll
gcc -O2 -S examples/bad/mmio_no_volatile.c -o -   # one load, second folded

gcc -Wall -Wextra -Werror -O2 examples/good/mmio_volatile.c -o out && ./out
gcc -Wall -Wextra -Werror -O2 examples/good/isr_flag_volatile.c -o out && ./out
gcc -Wall -Wextra -Werror -O2 examples/bad/isr_flag_no_volatile.c -o out && ./out  # exits 1
gcc -Wall -Wextra -Werror -O2 examples/bad/volatile_not_atomic.c -o out && ./out   # < expected
```

## Where the knowledge comes from

- ISO C11 N1570 §5.1.2.3, §6.7.3 (`volatile` semantics, observable behavior)
- GCC manual — Optimize Options (why `-O2` elides/hoists non-volatile accesses)
- Arm Architecture Reference Manual — memory types (Device vs Normal),
  barriers (DMB/DSB/ISB)
- Arm CMSIS-5 headers (`__IOM`/`__IM`/`__OM`, `core_cm*.h`)
- Linux kernel `memory-barriers.txt` — I/O barriers (`wmb`, `readl`/`writel`)

## Related skills

- `memory-ordering-reasoning` — atomics/ordering vs `volatile` (require of)
- `compiler-ub-assumptions` — why the optimizer exploits missing `volatile`
- `embedded-mpu-trustzone` — device vs normal memory attributes in MAIR/MPU
- `atomics-c11-cpp11-rust` — when `volatile` must be replaced by atomics

## Evaluation

- Synthetic: MMIO double-poll with and without `volatile`; ISR flag with and
  without `volatile`; `volatile` counter under a race.
- False-positive: a correct `volatile` MMIO accessor and a correct
  single-producer/single-consumer volatile ISR flag must NOT be flagged.
- Adversarial: code that "passes" at -O0 but the `-O2` asm shows the poll was
  removed (compiler caches the register); agent must find the missing
  `volatile`/cast and the ordering gap (barrier absent for MMIO writes).
