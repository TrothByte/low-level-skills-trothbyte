# Volatile & Memory-Mapped I/O — Reference

Sources: ISO C11 N1570 §5.1.2.3 & §6.7.3 (volatile semantics); GCC manual
(Optimize Options); Arm ARM (memory types, barriers); Arm CMSIS-5 headers;
Linux kernel memory-barriers.txt.

## 1. volatile forces every access to memory

- **RULE**: an access through a `volatile`-qualified lvalue cannot be
  eliminated, reordered with other volatile accesses, or cached in a register
  by the optimizer; it must hit memory each time (N1570 §6.7.3p6-7, §5.1.2.3p6).
- **WHY AI GETS IT WRONG**: assumes "the compiler reads the variable when I
  write the code", so `volatile` looks unnecessary.
- **CORRECT REASONING**: without `volatile` the compiler may fold two reads of
  a register into one, hoist a poll out of a loop, or drop the whole function.
- **EXAMPLE (bad)**: two reads of a simulated status register through a
  non-volatile pointer compile at `-O2` to ONE load (`movl ...; addl %eax,%eax`).
- **COUNTEREXAMPLE (good)**: the same double read through a `volatile` pointer
  compiles to TWO memory loads.
- **VERIFICATION**: `gcc -O2 -S` — count the loads; see Verified asm facts.
- **SOURCE**: `iso-c11-n1570` §5.1.2.3p6, §6.7.3p7; `gcc-manual`.

## 2. volatile is NOT atomic

- **RULE**: `volatile` guarantees neither atomicity nor exclusion; two threads
  mutating a `volatile` non-atomic object race, and reads/writes can be torn.
  Atomicity requires `_Atomic`/`atomic_*` (N1570 §5.1.2.4, §7.17).
- **WHY AI GETS IT WRONG**: "volatile stops caching, so threads sync" — it
  stops caching but not interleaving or tearing.
- **CORRECT REASONING**: a read-modify-write (`counter++`) on a `volatile`
  object is still load-then-store; two threads can interleave in the gap and
  lose updates. On x86 the plain RMW has no `lock` prefix.
- **EXAMPLE (bad)**: two threads doing `volatile int c; c = tmp; Sleep(2); c =
  tmp + 1;` (wide RMW window) — final value is ~50 instead of 100 (updates
  lost), reproduced 3/3 runs on x86.
- **COUNTEREXAMPLE (good)**: `atomic_fetch_add` or `lock`-prefixed RMW keeps
  the full count; TSan also flags the volatile version as a race.
- **VERIFICATION**: run `examples/bad/volatile_not_atomic.c`; asm shows plain
  `mov`/`add`/`mov`, no `lock`.
- **SOURCE**: `iso-c11-n1570` §5.1.2.4p4, §7.17.3; `gcc-manual`.

## 3. volatile is NOT ordering

- **RULE**: `volatile` places no ordering on other (even volatile) accesses; the
  compiler and hardware may reorder them relative to device-side expectations.
  Ordering is provided by barriers (DMB/DSB), compiler barriers, or atomic
  memory orders.
- **WHY AI GETS IT WRONG**: treats "goes to memory" as "goes in program order".
- **CORRECT REASONING**: for MMIO, two questions are separate — (a) preserve the
  access (`volatile`), (b) order it with the device or other cores (barrier).
  A FIFO/control sequence like "write config, then start" needs both.
- **EXAMPLE (bad)**: `reg = 1; reg_start = 1;` with plain non-volatile stores —
  `-O2` can reorder or merge them; device sees an invalid sequence.
- **COUNTEREXAMPLE (good)**: volatile stores + a compiler barrier/DMB between
  them, or a `writel` accessor with documented ordering.
- **VERIFICATION**: inspect `-O2` asm for reordering/merging; on weak targets
  check for the missing DMB/DSB.
- **SOURCE**: `arm-arm` (memory model, barriers); `gcc-manual` (asm volatile);
  `linux-memory-barriers`.

## 4. MMIO is device memory, not normal memory

- **RULE**: memory-mapped I/O must be mapped with a device (non-cacheable)
  attribute so the hardware does not buffer accesses; the compiler side still
  needs `volatile`. Normal memory is cacheable and the hardware may merge or
  buffer accesses to it.
- **WHY AI GETS IT WRONG**: assumes "MMIO is just an address, treat it like an
  array" — the device sees every read as a side effect (status pop, FIFO drain).
- **CORRECT REASONING**: two layers — software (`volatile`/atomics) and
  hardware (device vs normal attribute in the MPU/MMU MAIR/TCR). A volatile
  access to normal-memory-mapped register can still be buffered by the cache.
- **EXAMPLE (bad)**: a register region mapped as Normal cacheable — reads return
  stale cached lines even though the access is volatile.
- **COUNTEREXAMPLE (good)**: MAIR slot `Device-nGnRnE` (or `Device`/strongly
  ordered) for MMIO; Normal WBWA only for SRAM/DDR shared as data.
- **VERIFICATION**: review MPU/MMU attribute setup; on Cortex-M check
  `MPU_RASR`/MAIR encoding.
- **SOURCE**: `arm-arm` (memory types); `cmsis` (MPU/MAIR register encodings).

## 5. Use CMSIS accessors, not naked casts

- **RULE**: CMSIS models registers as `__IOM` (= `volatile`) members inside
  per-peripheral structs and `__STATIC_INLINE` read/write helpers; register
  access must go through `volatile`-typed pointers at all times.
- **WHY AI GETS IT WRONG**: declares `uint32_t *reg = (uint32_t*)0x40000000;`
  and reads through it — the cast drops volatile, so `-O2` caches the read.
- **CORRECT REASONING**: declare `volatile uint32_t *` (or a CMSIS peripheral
  pointer) and keep the qualifier on every intermediate alias; `__IOM` is the
  portable way to spell "device register".
- **EXAMPLE (bad)**: `uint32_t *s = (uint32_t *)0xE000E010;` then polling
  `*s` — load hoisted/removed at `-O2`.
- **COUNTEREXAMPLE (good)**: `volatile uint32_t *s = (volatile uint32_t
  *)0xE000E010;` or `SysTick_CTRL` from CMSIS.
- **VERIFICATION**: `gcc -O2 -S` shows the load survives; `-Wcast-qual` (when
  enabled) catches accidental drops.
- **SOURCE**: `cmsis` (`core_cm*.h`, `__IOM`/`__IM`/`__OM`, `__STATIC_INLINE`
  helpers).

## 6. Single-producer/single-consumer ISR flags: volatile is correct

- **RULE**: a flag written by the ISR and read by the main loop (or vice versa)
  on a single core, where exactly one side writes, is the canonical `volatile`
  use — it does not need atomics and is not a data race.
- **WHY AI GETS IT WRONG**: over-corrects with `_Atomic`/locks "because it is
  shared", or under-corrects by dropping `volatile` so the loop is optimized
  into a single check.
- **CORRECT REASONING**: `volatile` forces the poll to re-read memory each
  iteration (the ISR's store is the only other writer). If two writers exist
  (e.g. the flag is also cleared from the main loop AND another thread), the
  protocol needs atomics or a critical section.
- **EXAMPLE (bad)**: non-volatile `int flag; while (!flag) {}` — `-O2` loads
  once before the loop and spins forever; the ISR store is never observed.
- **COUNTEREXAMPLE (good)**: `volatile int flag; while (!flag) {}` — the loop
  reloads every iteration and exits when the ISR writes.
- **VERIFICATION**: run `examples/good|bad/isr_flag_*`; `-O2 -S` shows the
  volatile loop reloading vs the non-volatile single load.
- **SOURCE**: `iso-c11-n1570` §5.1.2.3p5-6; `gcc-manual`.

## 7. Why -O2 changes behavior without volatile

- **RULE**: at `-O2` the optimizer hoists invariant loads out of loops, CSEs
  repeated loads, and deletes dead stores/loads; non-volatile device reads are
  "invariant" in its model because nothing in the visible code writes them.
- **WHY AI GETS IT WRONG**: tests at `-O0`/`-g` pass, then the production build
  behaves differently and the agent blames the scheduler or hardware.
- **CORRECT REASONING**: without `volatile` the compiler is allowed to assume
  the memory is unchanged between reads; with it, every access is observable
  behavior that must be emitted (N1570 §5.1.2.3p6).
- **EXAMPLE (bad)**: `poll()` that reads a register twice compiles at `-O2` to
  one `movl` + `addl %eax,%eax`; a `while (!reg) {}` loop collapses to a single
  check (GCC 16.1, x86-64, reproduced).
- **COUNTEREXAMPLE (good)**: the same code with `volatile` emits one memory
  access per source access.
- **VERIFICATION**: `gcc -O2 -S` comparison; see Verified asm facts.
- **SOURCE**: `gcc-manual` (Optimize Options, -fstrict-volatile-bitfields aside);
  `iso-c11-n1570` §5.1.2.3.

## Verified asm facts (GCC 16.1, MinGW x86-64, -O2)

`poll(uint32_t *r)` returning `r[1] + r[1]` (register bank simulated as array):

| Variant | Generated asm | Behavior |
|---|---|---|
| non-volatile | `movl 4(%rax),%eax; addl %eax,%eax` | ONE load, second read folded |
| volatile | `movl 4(%rdx),%eax; addl 4(%rdx),%eax` | TWO loads from memory |

`while (*f == 0) {}` flag poll with an ISR thread that writes after 5 ms:

| Variant | Generated asm | Result |
|---|---|---|
| non-volatile | single load at entry, loop collapsed | times out (exit 1), never sees ISR |
| volatile | `movl flag(%rip),%edx` reloaded per iteration | exits 0, sees ISR |

`volatile int c; c = tmp; sleep; c = tmp + 1;` from 2 threads (wide RMW window):

| Variant | Result |
|---|---|
| volatile | final ~50 of 100 (updates lost), 3/3 runs |
| `_Atomic`/lock | 100 (no lost updates) |

Teaching points:
- The observable difference is at the asm level: register caching of a
  non-volatile load vs a fresh memory read each access.
- `volatile` fixes "the compiler dropped the access"; barriers fix "the device
  or other core saw accesses out of order"; atomics fix "two writers interleaved".
- On x86 plain aligned loads/stores are single instructions, but the RMW still
  races; on ARM the missing `volatile` shows the same load caching plus a
  missing DMB.

## Common failure modes

- A11 (missing volatile): MMIO read cached/eliminated at -O2 — asm inspection
  catches.
- B11 (missing barrier): volatile stores reordered vs device protocol — needs
  DMB/DSB or compiler barrier.
- B13 (volatile as atomics): shared counter lost updates — TSan/lost-update
  demo catches.
