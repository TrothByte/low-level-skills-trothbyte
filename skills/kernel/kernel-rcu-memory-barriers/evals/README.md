# Evaluation — kernel-rcu-memory-barriers

Skill: `skills/kernel/kernel-rcu-memory-barriers`. Stability target: `evaluated`.

## Historical CVE eval (adversarial)

- **AD-01 (CVE-2016-5195 Dirty COW, TOCTOU)**: the COW page-fault path checks
  that the PTE is read-only and marks it writable (check), while the write path
  then writes through the mapping (act). A racing thread can fault and remap
  between the two, so a read-only file mapping becomes writable. The agent must:
  1. name the class: check-then-act TOCTOU race window, NOT a missing memory
     barrier; 2. explain why a barrier (smp_mb) does NOT close it — the window is
  between an old PTE state and a new one, closed only by an atomic/state re-check
  under the page-table lock (the upstream fix re-checks FAULT_FLAG_WRITE and the
  COW state after the fault); 3. state that this is kernel-internal (no
  READ_ONCE/WRITE_ONCE fix applies).
- Detect / Explain / Fix / Verify: the fix is a re-check + lock, verified by
  reproducing on old kernels and by the upstream commit's regression test.

## Synthetic evals

- **easy/positive**: `WRITE_ONCE` latch with no payload — must NOT be flagged.
- **easy/negative**: plain `g_ready = 1; if (g_ready) ...` — must flag missing
  WRITE_ONCE/READ_ONCE and the data race.
- **medium/negative**: flag+payload with only the writer barrier (no reader
  pairing) — must flag the missing acquire on the read side.
- **medium/negative**: RCU reader with plain `p = g_ptr` outside `rcu_read_lock`
  — must flag and fix with `rcu_dereference` inside `rcu_read_lock`.
- **hard/negative**: publish with plain store (no `rcu_assign_pointer`) plus
  immediate `kfree(old)` — must flag both missing release and premature free.
- **hard/negative**: `kmalloc(GFP_KERNEL)` inside a spinlock — must flag
  atomic-context sleep and fix by pre-allocating or GFP_ATOMIC.
- **ambiguous**: `smp_mb()` where a one-sided `smp_wmb()`/`smp_rmb()` pair
  suffices — must suggest weakening but mark as OPTIONAL, not a bug.

## False-positive evals (correct code must NOT be flagged)

- Correct `rcu_assign_pointer` + `rcu_dereference` publish-subscribe with
  `synchronize_rcu()` before `kfree` — no flag.
- `WRITE_ONCE`/`READ_ONCE` on a single flag with no payload — must NOT be
  reported as a missing ordering bug (no ordering is required).
- Paired `smp_wmb()`/`smp_rmb()` (or `smp_mb()`) flag+payload protocol — no flag.
- Plain payload store before a release (`rcu_assign_pointer`) inside a single
  writer — must NOT be flagged as missing WRITE_ONCE on the payload.
- `kmalloc` BEFORE taking the spinlock — no flag.

## Verification commands (host)

```
gcc -Wall -Wextra -Werror -O2 -c examples/bad/bad_snippets.c
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_snippets.c
gcc -Wall -Wextra -Werror -O2 -pthread examples/demo/ordering_demo.c -o /tmp/ordering_demo
/tmp/ordering_demo fenced
/tmp/ordering_demo racy        # UB demo; outcome is not a pass/fail
gcc -O2 -S examples/demo/ordering_demo.c -o /tmp/ordering_demo.s   # inspect fences
```

Target kernel verification (documented, NOT run on this host): build with
CONFIG_PROVE_LOCKING, CONFIG_PROVE_RCU, CONFIG_KCSAN, CONFIG_KASAN,
CONFIG_DEBUG_ATOMIC_SLEEP; boot under QEMU; exercise the code; read dmesg for
lockdep/KCSAN/"sleeping function" splats.

## Verified facts (recorded on 2026-08-14, host is Windows/MSYS2)

- gcc 16.1.0 (MSYS2), python 3.11.9; no kernel headers on this host.
- All three `-Wall -Wextra -Werror -O2` builds: exit 0 (see report for codes).
- `ordering_demo fenced`: exit 0, "PASS: 200000 rounds, all payloads consistent".
- `ordering_demo racy`: exit 0 on this host (x86 TSO + GCC), which is exactly the
  teaching point — the racy pattern is still a data race / UB and would
  reproduce on ARM/RISC-V; racy output is NOT a verification.
- `smp_mb()` stub emits a hardware fence on x86: `objdump -d` shows no `mfence`
  but 3 lock-prefixed ops / 9 xchg ops — GCC 16 implements the seq-cst fence as
  a locked `cmpxchg`/`xchg`, which is still a full hardware fence.
- Host compile + user-space demo are VERIFIED. Kernel build, lockdep/KCSAN/KASAN,
  QEMU, and checkpatch runs are documented-as-target only.
