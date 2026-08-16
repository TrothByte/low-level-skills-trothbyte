---
name: memory-model-arm-x86-riscv
description: Use when reasoning about cross-thread ordering on ARM, x86, or RISC-V: lock-free code, message passing, publication, atomic orderings, memory barriers, seq_cst vs relaxed, and porting TSO-only code to weak-memory machines. Teaches each architecture's memory model and how to map C11/Rust atomics to hardware.
---

# Memory Model Reasoning: ARM / x86 / RISC-V

## When to use

- Lock-free data structures, message passing, or publication patterns where one
  thread must observe another thread's writes.
- Porting code that was written and tested on x86 to ARM (AArch64) or RISC-V —
  the classic "it worked on my laptop" trap.
- Choosing C11 `memory_order_*` / Rust `Ordering` levels for a specific pattern.
- Reviewing hand-written barriers or fences in portable code.
- Debugging heisenbugs where a race "fixes" under a debugger or a different core.
- Estimating the real cost of `seq_cst` vs `relaxed` (not a free choice).

## When not to use

- Single-threaded code with no atomics, fences, or shared state.
- Only need compiler-side reordering control (plain `volatile` / compiler
  barriers) — no CPU ordering involved.
- Already debugging on one specific core and only need the ordering rules for
  that ISA: load the per-ISA table instead of the full reasoning flow.
- The code is fully synchronized by a mutex/RW-lock and will never switch to
  lock-free — ordering was chosen by the lock.

## What the agent often gets wrong

- "x86 is total-store-order, so nothing reorders" — x86-64 TSO still allows
  *store-to-load* reordering; only TSO's 4 ordering rules hold, not a total order.
- "seq_cst is the safe default, always use it" — on ARM/RISC-V `seq_cst` stores
  compile to `STLR`/`fence rw,rw` and on x86 to `xchg`; it is both more
  expensive and not a substitute for reasoning about the actual ordering needed.
- "acquire/release is just a fence pair" — release is a one-way operation on
  writes, acquire one-way on reads; they must be paired on the *same location*
  to establish happens-before (B2).
- "relaxed means no ordering at all" — relaxed still has coherence (same
  location, same thread), you just lose cross-location ordering (A10).
- "ARM/RISC-V are weak = anything can happen" — both have precise rules:
  ARMv8 (AArch64) preserves *dependent* reads (address dependencies) and
  RISC-V's RVWMO defines p0/p1/p2 orders + cumulativity.
- "volatile gives ordering" — `volatile` gives access atomicity guarantees only
  where the ISA supports it and blocks *compiler* reordering, never CPU
  reordering (C11 `volatile` has no multi-thread ordering semantics).
- Writing a spin loop on a non-atomic variable and expecting the compiler not
  to hoist it — the loop is UB and may be optimized out (B7).

## How to reason correctly

1. Identify the exact cross-thread pattern: (a) publish-then-consume,
   (b) message passing (write data, then write flag), (c) RMW coordination
   (counter, lock-free queue), or (d) Dekker/peterson mutual exclusion.
2. Determine the architecture's model: x86-64 = TSO (4 ordering rules, only
   store-load reorder); AArch64 = weak model + hardware coherence (ldar/stlr,
   dependencies, dmb); RISC-V = RVWMO (fence rw,rw, acquire/release bits on
   loads/stores/AMOs, p0-p2 + cumulativity).
3. Map the C11/Rust ordering to the required pattern:
   - publish/consume one-way: `release` store + `acquire` load on the same
     variable (the "flag").
   - RMW that must see latest: `acquire_release` / `acq_rel` (e.g., a counter
     used to signal completion).
   - full mutual exclusion without locks: `seq_cst` (rarely; verify with a
     model, prefer locks).
   - order-independent counters/shared stats: `relaxed` is fine.
4. Never put the ordering in a random fence: place it at the publication /
   consumption point and pair acquire with release on the same location.
5. If a pattern is subtle (Dekker, seqlock, MPMC queue), verify it with a
   formal model (herd7/cat) or a runtime stress test — do not argue it from
   intuition.
6. Verify the compiler emitted the intended instructions (objdump/-S) —
   especially that `seq_cst` didn't silently degrade or `relaxed` didn't
   accidentally become a fence.

## What to verify

- No data race: every write to a shared location is atomic, or all accesses are
  ordered by a happens-before chain (C11 §6.5p5: data race = UB).
- `release` store and `acquire` load target the *same* location.
- The chosen ordering is *sufficient*, not just plausible — write out which
  write becomes visible to which read.
- On x86: compiler output contains the expected plain `mov` for relaxed and
  `xchg`/`mfence` for seq_cst store; no unexpected `mfence` added.
- On ARM/RISC-V (documented, not run here): `STLR`/`LDAR` or `dmb`/`fence`
  present exactly where intended.

## How to verify

Host-verifiable (this repo runs on x86):

```
gcc -O2 -Wall -Wextra examples/good/wakeup_flag.c -o /tmp/wakeup && /tmp/wakeup
gcc -O2 -S -o - examples/good/wakeup_flag.c   # read the store/load instructions
objdump -d /tmp/wakeup | grep -E "mov|xchg|mfence" | head -20
```

Cross-architecture (documented, target toolchain not on this host):

```
clang --target=aarch64-none-elf -O2 -S examples/good/wakeup_flag.c   # expect ldr/str + dmb or ldar/stlr
clang --target=riscv64-unknown-elf -O2 -S examples/good/wakeup_flag.c # expect fence rw,rw or amoswap
# formal model: herd7 + arm.cfg / riscv.cfg on the pattern
```

## Where the knowledge comes from

- `intel-sdm` — Vol.3A §8 (x86 memory ordering, TSO rules)
- `arm-arm` — AArch64 memory model and barriers
- `riscv-isa-spec` — RVWMO (memory model section of the unprivileged spec)
- `iso-c11-n1570` — §6.5p5 (data race/UB), §7.17.3 (memory_order semantics)
- `llvm-langref` — atomic ordering and fences in IR (acquire/release/seq_cst)
- `linux-memory-barriers` — kernel-side ordering documentation and DMA rules

## Related skills

- `memory-ordering-reasoning` — cross-language ordering model (recommend)
- `atomics-c11-cpp11-rust` — API-level ordering tables (require)
- `kernel-rcu-memory-barriers` — kernel barrier rules for RCU/driver code (recommend)
- `concurrency-actual-parallelism-detection` — prove real parallelism exists before ordering matters (recommend)
- `gpu-memory-model-coherence` — GPU coherence model contrasts with CPU TSO (recommend)
- `embedded-volatile-and-memory-ordering` — volatile vs ordering on embedded targets (recommend)

## Evaluation

Synthetic: message-passing pattern with wrong ordering (relaxed flag) must be
flagged; Dekker on ARM/RISC-V must be flagged as not portable; correct
release/acquire pair approved. Adversarial: code "works on x86" but relies on
TSO; a `seq_cst`-free lock that provably needs it. Historical: Linux/PowerPC
memory ordering bug classes and the classic x86 store-buffer store-load
reordering (Dekker failure) — explain why the pattern breaks on ARM/RISC-V but
appears to work on x86. False-positive: a correct `relaxed` counter with no
ordering requirements must NOT be flagged.
