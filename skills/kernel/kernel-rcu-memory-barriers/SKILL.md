---
name: kernel-rcu-memory-barriers
description: Use when writing or reviewing Linux kernel code that needs memory barriers, READ_ONCE/WRITE_ONCE, or RCU — publish-subscribe patterns, rcu_assign_pointer/rcu_dereference, synchronize_rcu, or atomic-context rules like no sleeping in spinlocks. Teaches the kernel memory model and why it differs from C11 atomics.
---

# Linux Kernel Memory Barriers & RCU

## When to use

- Writing or reviewing kernel code that shares memory between CPUs, interrupts,
  or workqueue/process contexts (sysfs attributes, seqlocks, RCU data
  structures, flag+payload protocols).
- Deciding between spinlock, RCU, and atomic/RMW for a given data structure.
- Debugging "works on my machine but corrupts under SMP" symptoms or lockdep /
  KCSAN / "sleeping function called from invalid context" splats.
- Explaining why a patch needs `READ_ONCE`/`WRITE_ONCE`, a barrier, or
  `rcu_dereference` where a plain access was used.

## When not to use

- User-space C11/C++11/Rust atomics and ordering — use
  `memory-ordering-reasoning` (different memory model).
- Lock ordering, deadlock analysis, mutex/semaphore lifecycle — use
  `concurrency-deadlock-and-lock-ordering`.
- `volatile` for MMIO device registers (single-CPU, hardware-visible) — use
  `embedded-volatile-and-memory-ordering`.
- Code with no shared, concurrently-accessed data (no barriers needed).

## What the agent often gets wrong

- "WRITE_ONCE is a barrier." It is anti-tearing/anti-fusion only; it gives no
  ordering. Pair it with `smp_mb()` or RCU when a payload follows.
- "READ_ONCE/WRITE_ONCE are enough for flag+payload." On ARM/POWER/RISC-V the
  hardware reorders; the flag must be released and the payload read after an
  acquire — barriers or RCU are required.
- "x86 tests pass, so the ordering is fine." x86 TSO hides ordering bugs; the
  same code races on weakly-ordered CPUs. Reason at the kernel model level.
- "`rcu_dereference` is just a cast." A plain `p = g_ptr` races with the
  writer's `rcu_assign_pointer` (release store); readers must use
  `rcu_dereference()` inside `rcu_read_lock()`.
- "kmalloc(GFP_KERNEL) inside a spinlock is fine." GFP_KERNEL may sleep; a
  spinlock disables preemption -> sleep-in-atomic-context bug. Use GFP_ATOMIC
  or pre-allocate before taking the lock.
- "synchronize_rcu() frees the memory." It only waits for a grace period; the
  caller still has to `kfree` the old object, and it must not be called while
  holding a spinlock or inside an RCU read-side section.
- "call_rcu() runs the callback immediately." It runs after a grace period, in
  softirq context; the object must stay alive until then and the callback
  cannot sleep.
- "smp_mb() is the same as a C11 seq-cst fence." The C11 fence only orders
  atomic operations; the kernel's `smp_mb()` orders plain accesses too.

## How to reason correctly

1. Identify what is shared, and between which contexts (process, softirq,
   hardirq, NMI, RCU reader). Different contexts impose different rules.
2. Decide the required ordering direction: which store must be visible before
   another store (release side), which load must not be reordered before other
   loads (acquire side).
3. Pick the primitive: `WRITE_ONCE`/`READ_ONCE` for a single access,
   `smp_mb`/`smp_wmb`/`smp_rmb` for explicit ordering, `rcu_assign_pointer` +
   `rcu_dereference` for publish-subscribe, spinlock for mutual exclusion.
4. Pair barriers and release/acquire: each writer barrier must have a matching
   reader barrier; an acquire that reads the released value forms the
   happens-before edge.
5. Classify the context for sleeping: spinlock-held, interrupt, and RCU
   read-side sections cannot sleep; GFP_ATOMIC is for atomic context.
6. Free RCU-protected objects only after a grace period
   (`synchronize_rcu()`/`call_rcu()`), never while readers may still hold them.

## What to verify

- Every shared plain access is covered by `READ_ONCE`/`WRITE_ONCE` or
  `rcu_dereference`; no plain loads/stores on variables another context writes.
- Release/acquire pairs exist: payload before flag store, flag load before
  payload loads, or `rcu_assign_pointer` before free.
- No sleeping functions (`kmalloc(GFP_KERNEL)`, `mutex_lock`, `copy_to_user`,
  `synchronize_rcu`) in atomic or RCU read-side context.
- `rcu_dereference` used inside `rcu_read_lock()` (or with a documented
  protection annotation); object lifetime outlives readers.
- `-Wall -Wextra -Werror -O2` clean; lockdep/KCSAN builds run clean.

## How to verify

Host compile + run (ordering logic only; no kernel headers needed):

```
gcc -Wall -Wextra -Werror -O2 -c examples/bad/bad_snippets.c
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_snippets.c
gcc -Wall -Wextra -Werror -O2 -pthread examples/demo/ordering_demo.c -o /tmp/ordering_demo
/tmp/ordering_demo fenced
```

Target kernel commands (documented, NOT run on this host):

```
# enable lockdep, KCSAN, KASAN, debug-atomic-sleep
make defconfig
scripts/config -e PROVE_LOCKING -e PROVE_RCU -e KCSAN -e KASAN \
               -e DEBUG_ATOMIC_SLEEP
make -j$(nproc)
# boot under QEMU and check dmesg for splats
qemu-system-x86_64 -kernel arch/x86/boot/bzImage -append "console=ttyS0 nokaslr"
# and run the new code's path, e.g. modprobe your_test_module
./scripts/checkpatch.pl --strict your.patch
```

KCSAN reports data races (missing READ_ONCE/WRITE_ONCE); lockdep/RCU lockdep
reports RCU usage violations; CONFIG_DEBUG_ATOMIC_SLEEP reports sleeping in
atomic context.

## Where the knowledge comes from

- `linux-memory-barriers` — memory-barriers.txt: barrier semantics, pairing,
  compiler barriers, RCU section
- `linux-rcu` — Documentation/RCU/: whatisRCU, checklist, rcu_dereference,
  lockdep
- `kernel-coding-style` — kernel conventions referenced by examples
- `ldd3` — ch.5 spinlocks/atomic context, ch.8 kmalloc GFP flags

## Related skills

- `memory-ordering-reasoning` — user-space C11/C++11/Rust atomics; distinct
  model, do not translate between them mechanically
- `concurrency-deadlock-and-lock-ordering` — lock ordering and sleeping rules
- `embedded-volatile-and-memory-ordering` — volatile vs synchronization
- `c-undefined-behavior` — a data race is UB in the kernel too
- `compiler-ub-assumptions` — why the compiler exploits unchecked plain accesses

## Evaluation

Historical adversarial: CVE-2016-5195 (Dirty COW) — check-then-act TOCTOU on
the COW PTE between the page-fault path and the write path; agent must name the
TOCTOU window and the lock/atomic re-check that closes it (a memory barrier
does NOT close it). Synthetic: missing WRITE_ONCE race, RCU deref without
`rcu_dereference`, publish without barrier, `kmalloc(GFP_KERNEL)` in spinlock —
each must be detected and fixed. False-positive: correct
`rcu_assign_pointer`/`rcu_dereference`, `WRITE_ONCE` latch without payload,
and a paired `smp_mb` flag+payload must NOT be flagged.
