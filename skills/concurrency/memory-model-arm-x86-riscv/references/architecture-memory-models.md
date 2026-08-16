# Architecture Memory Models — ARM / x86 / RISC-V

Sources: Intel SDM Vol.3A §8; Arm Architecture Reference Manual (A-profile)
memory model; RISC-V ISA spec (RVWMO); ISO C11 N1570 §7.17.3; LLVM LangRef.
The x86-side facts below were cross-checked with gcc 16.1 output on this host
(x86-64, TSO). ARM/RISC-V facts are researched from the specs, not executed.

## 1. x86-64 is TSO with exactly one reordering

- **RULE**: x86-64 implements a total-store-order-like model: (1) loads are not
  reordered with loads, (2) stores are not reordered with stores, (3) stores are
  not reordered with older loads, (4) loads *may* be reordered with older stores
  to *different* locations. There is no rule (5) "nothing ever reorders".
- **WHY AI GETS IT WRONG**: people remember "x86 is strongly ordered" and forget
  the store-load reordering that powers the classic Dekker counterexample.
- **CORRECT REASONING**: the store-buffer allows a load to read from the buffer
  while a store is in flight; a later load of a different address can effectively
  move ahead of an earlier store. That is the ONE hole — and it is the reason
  `seq_cst` stores on x86 compile to `xchg` (or `mov` + `mfence`), not plain `mov`.
- **EXAMPLE**: thread A stores to `x` then to `flag`; thread B loads `flag`,
  then loads `x`. On x86 the load of `x` can happen before the store of `flag`
  becomes visible — the classic publish bug if done with non-atomic variables.
- **COUNTEREXAMPLE**: "x86 so no fence needed for Dekker" — Dekker's algorithm
  (two `flag` writes + two counters) relies on load-load and store-load ordering
  that TSO does not give; the implementation in `examples/bad/double_checked.c`
  fails on real hardware behavior when the compiler models it.
- **VERIFICATION**: `gcc -O2 -S` — a `seq_cst` store produces `xchg` (verified
  on this host). `relaxed` store produces plain `mov`.
- **SOURCE**: `intel-sdm` Vol.3A §8.2 (memory ordering), verified with gcc 16.1.

## 2. C11 `memory_order` maps to different hardware sequences per ISA

- **RULE**: C11 defines `relaxed`, `consume`, `acquire`, `release`,
  `acq_rel`, `seq_cst`. The hardware implementation differs: on x86, only
  `seq_cst` stores need `xchg`/`mfence`; on AArch64, acquire/release map to
  `LDAR`/`STLR` (or `DMB`); on RISC-V they map to `fence rw,rw` (or load/store
  with acquire/release bits in newer extensions).
- **WHY AI GETS IT WRONG**: assuming one universal implementation, e.g. "all
  atomics become `lock`-prefixed instructions" (x86-only thinking), or that
  `seq_cst` is free.
- **CORRECT REASONING**: per-ISA: (a) x86: `relaxed`=mov, `release` store=mov,
  `seq_cst` store=xchg/mfence, acquire load=mov; (b) AArch64: `relaxed`=ldr/str,
  `acquire` load=ldar, `release` store=stlr, `seq_cst`=ldar/stlr with implicit
  ordering; (c) RISC-V: `fence rw,rw` around the access for acq/rel, `fence
  iorw,iorw` for seq_cst.
- **EXAMPLE**: `atomic_store_explicit(&flag, 1, memory_order_seq_cst)` on x86
  compiles to `xchg` — verified on this host.
- **COUNTEREXAMPLE**: `atomic_store_explicit(&flag, 1, memory_order_relaxed)`
  on AArch64 compiles to plain `str` — no ordering with the prior data store.
- **VERIFICATION**: `gcc -S` / `clang -S` with the target triple; compare
  instruction sequence per architecture.
- **SOURCE**: `iso-c11-n1570` §7.17.3; `llvm-langref` (atomics); gcc 16.1
  empirical.

## 3. RISC-V RVWMO: p0/p1/p2 orders and cumulativity

- **RULE**: RVWMO orders events with the preserved program order (p0: same
  address, p1: loads before dependent loads, p2: strong-order pairs like
  AMOs and `fence rw,rw`) plus global memory order; cumulativity means a fence
  also orders other harts' views that happen-before the fence.
- **WHY AI GETS IT WRONG**: treating RISC-V as "anything goes" or as "ARM with
  different mnemonics" without knowing the preserved orders.
- **CORRECT REASONING**: the model is documented as a set of axioms
  (I-Ordering, Atomicity, Preservation, Global-Order); the practical takeaway is
  that acquire/release semantics come from `fence rw,rw` (or amoswap with aq/rl)
  and dependency-ordering from address/data dependencies.
- **EXAMPLE**: a release store followed by a `fence rw,rw` establishes the
  message-passing guarantee on RISC-V.
- **COUNTEREXAMPLE**: a bare `fence` (default `iorw,iorw`) inserted randomly
  "just to be safe" is stronger than needed and slows every hart.
- **VERIFICATION**: `clang --target=riscv64-unknown-elf -S` on this host would
  emit `fence rw,rw`; toolchain not installed — documented as target check.
- **SOURCE**: `riscv-isa-spec` (RVWMO).

## 4. AArch64: acquire/release instructions and the dependency rule

- **RULE**: AArch64 has `LDAR`/`LDAPR` (acquire loads), `STLR` (release
  stores), and `DMB`/`DSB` barriers; loads are ordered by address/data
  dependencies, giving the "cumulativity + dependency" model.
- **WHY AI GETS IT WRONG**: assuming that "ARM needs a DMB after every store"
  (over-fencing) or that a plain `str` after `dmb` gives release semantics
  (incomplete — the release must be on the store that publishes).
- **CORRECT REASONING**: for a publish pattern, `STLR` on the flag is
  sufficient; a separate `DMB` is not needed and is a performance hit. For a
  seqlock reader, the read-side must use `LDAR`-like acquire on the sequence
  counter.
- **EXAMPLE**: `examples/good/wakeup_flag.c` with acquire/release on `flag`
  gives ldar/stlr on AArch64 (target), mov-only on x86 (verified).
- **COUNTEREXAMPLE**: `double_checked.c` uses non-atomic `volatile` — compiles
  cleanly on x86 but is a data race under C11 and breaks on AArch64.
- **VERIFICATION**: `clang --target=aarch64-none-elf -S`; on-host check is x86
  only, so the ARM command is documented as a target command.
- **SOURCE**: `arm-arm` (memory model, barriers chapter).
