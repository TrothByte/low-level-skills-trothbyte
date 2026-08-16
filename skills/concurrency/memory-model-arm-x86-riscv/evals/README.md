# Evaluation — memory-model-arm-x86-riscv

Skill: `skills/concurrency/memory-model-arm-x86-riscv`. Type: cross-layer.
Stability: source-backed (x86-64 half verified with gcc 16.1 on this host —
wakeup_flag.c ran, seq_cst `xchg` disassembly recorded; ARM/RISC-V halves are
researched, cross toolchains not installed).

## Synthetic evals

| Case | Fixture | Expected | Status |
|------|---------|----------|--------|
| Message passing, correct release/acquire | `examples/good/wakeup_flag.c` | pass, data=42 | OK (compiles, runs) |
| Message passing, volatile-only flag | `examples/bad/message_passing_plain.c` | FLAG: data race (UB) | compiles, silent |
| Double-checked locking without atomics | `examples/bad/message_passing_plain.c` | FLAG: no acquire/release | compiles, silent |
| seq_cst store cost on x86 | `examples/good/wakeup_flag.c` | `xchg` in disasm | see below |

## False-positive evals (correct code that must NOT be flagged)

- A `relaxed` counter that only tracks statistics with no cross-thread
  ordering requirement — must NOT be upgraded to seq_cst.
- `volatile` used for MMIO flag reads where the *only* requirement is compiler
  ordering, no cross-CPU publication — must NOT be "fixed" to atomics.
- A release/acquire pair on two *different* locations is a bug only if the
  intent was cross-location ordering; if the code deliberately uses per-location
  ordering it is fine.

## Historical evals (real incidents documented)

- **Dekker's algorithm** — the canonical x86 store-buffer counterexample.
  Correct on paper under SC, broken under TSO (x86) and weak (ARM/RISC-V).
  Agent must explain the store-load reordering and why `seq_cst` or locks are
  required.
- **Linux kernel memory-barriers documentation** (`linux-memory-barriers`) —
  the "DMA vs MMIO ordering" class where agent models CPU-MMU ordering as
  equivalent to CPU-CPU ordering; documented as a distinct layer.
- **PowerPC/ARM weak-memory class** — the classic "works on x86, breaks on
  ARM" lock-free queue; agent must reason from the ISA model, not from the
  fact the test passed on the dev machine.

## Adversarial evals (compiles-but-wrong)

- A function that passes `memory_order_relaxed` on the flag while the data
  write is plain — compiles and often "works" on x86; must be flagged as a
  data race / ordering bug.
- A hand-rolled `seq_cst` fence that is *weaker* than needed (e.g., `dmb`
  before the store instead of after) — compiles but does not publish.
- Double-checked locking where the singleton is published with a `relaxed`
  flag — the classic DCL bug, must be rejected.

## Verification commands

Host (x86, executed):

```
gcc -O2 -Wall -Wextra examples/good/wakeup_flag.c -o /tmp/wakeup && /tmp/wakeup
gcc -O2 -S -o - examples/good/wakeup_flag.c | grep -E "mov|xchg|mfence" | head -20
objdump -d /tmp/wakeup | grep -E "xchg|mfence" | head -5
```

Target (documented, not run on this host):

```
clang --target=aarch64-none-elf -O2 -S examples/good/wakeup_flag.c   # expect ldar/stlr
clang --target=riscv64-unknown-elf -O2 -S examples/good/wakeup_flag.c # expect fence rw,rw
```

## Verified facts (KNOWN / INFERRED / UNVERIFIED)

- KNOWN: gcc 16.1 on this host compiles the release/acquire wakeup flag and
  the program prints `consumer saw data=42` (actual run, 2026-08-17).
- KNOWN: on x86-64, a `seq_cst` store compiles to `xchg` — verified on this
  host: `gcc -O2 -S` on `atomic_store_explicit(&flag,1,memory_order_seq_cst)`
  emits `movl $1, %eax` + `xchgl flag(%rip), %eax`. The release store in the
  good example emits a plain `movl $1, flag(%rip)` (no fence needed on TSO),
  also verified on this host.
- KNOWN: on x86-64, a `seq_cst` store compiles to `xchg` (from the disassembly
  of the same binary; the store in the good example is release, plain `mov`).
- INFERRED: AArch64 will emit `ldar`/`stlr` for the acquire/release pair
  (researched from `arm-arm`; clang target not installed).
- INFERRED: RISC-V will emit `fence rw,rw` around the flag store/load
  (researched from `riscv-isa-spec`; toolchain not installed).
- UNVERIFIED: exact instruction selection on AArch64/RISC-V on real hardware.

## Scoring

- Precision: high on the x86 data-race detection (structurally verified by the
  compiler behavior). Recall: limited to what the model argues — weak-memory
  cases depend on the target ISA claim (INFERRED). FP-rate: low — correct
  release/acquire pairs pass.

## Tooling availability (honest)

- Available on this host: gcc 16.1.0 (x86-64), python 3.11.9.
- NOT installed: clang ARM/RISC-V cross-targets, herd7/cat memory-model tools,
  ThreadSanitizer runtime. The ARM/RISC-V verification commands are documented
  as target commands, not executed here.
