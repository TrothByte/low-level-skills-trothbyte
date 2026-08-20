# Evaluation — workqueue-flush-and-cancellation

Skill: `skills/kernel/workqueue-flush-and-cancellation`. Stability target:
`evaluated`.

## Verified facts (host, this run)

- Examples compile clean with `gcc -Wall -Wextra -Werror -O2` using
  self-contained stubs (`examples/stubs.h`) — no kernel headers required.
- The good example runs with all assertions passing (exit 0) and prints
  "ALL CHECKS PASSED".
- The bad example compiles and runs; it reproduces the use-after-free class
  (work_struct freed while still PENDING) and prints a "BUG reproduced"
  diagnostic (exit 0) without crashing the harness.
- The emulator is deterministic and single-threaded: a `run_pending_work_emu`
  loop executes one pending item at a time, so runs are reproducible.

| Example | Compile | Run | Observation |
|---|---|---|---|
| good/good_flush.c | 0 | 0 | ALL CHECKS PASSED; flush true/false, cancel_work_sync, no run after free |
| bad/bad_flush.c | 0 | 0 | "BUG reproduced: work ran after work_struct freed" |

NOT verified on this host (documented targets, do NOT claim to have run):
kernel build with lockdep, kernel build with KASAN, QEMU VM boot, syzkaller.

## Historical bug-class evals

Workqueue flush/cancel bugs generally have no public CVE; they are documented
kernel bug classes fixed per-driver by commit. No CVE numbers are assigned or
claimed here.

| Class | Fixture | Detect | Fix | Verify |
|---|---|---|---|---|
| UAF: `cancel_work_sync()`/`flush_work()` missing before `kfree()` of the `work_struct` or the resources its function dereferences | driver teardown/`.remove` paths; the kernel's `find_worker_executing_work()` busy-hash exists because work items are recycled while still executing (kernel-source) | pending/running item executes freed memory (KASAN: use-after-free in kworker) | stop the producer, then `cancel_work_sync` (or flush), then free — contract in `Documentation/core-api/workqueue.rst` (kernel-workqueue-docs) | KASAN VM: teardown reproducer must be KASAN-clean |
| Deadlock: `flush_work()`/`flush_workqueue()` called from within the same work item waits for the running worker — self-wait | any work function that flushes its own item or its own workqueue | hang on teardown; lockdep workqueue flush/cancel annotations report the dependency (kernel-lockdep-docs) | never flush from inside the item; use the cancel path or restructure | lockdep build: boot, run the driver, watch for "possible recursive locking" / WQ-flush warning |

The modern `flush_work()`/`cancel_work_sync()` semantics these docs describe
were introduced by the concurrency-managed workqueue (cmwq) rework (Tejun
Heo, merged in v2.6.36) and are documented in
`Documentation/core-api/workqueue.rst` (kernel-workqueue-docs, kernel-source).

Each eval: DETECT (find the missing flush/cancel) -> EXPLAIN (which contract
was violated) -> FIX (add the correct call in the right order) -> VERIFY
(lockdep/KASAN clean + deterministic stub run).

## Synthetic evals

- easy/positive: proper teardown (stop producer -> `cancel_work_sync` ->
  `kfree` -> verify idle) must NOT be flagged.
- easy/negative: `kfree(work)` immediately after `queue_work` (no flush, no
  cancel) must be flagged.
- medium/negative: `cancel_work()` then `kfree(work_struct)` must be flagged
  (async cancel gives no guarantee; racing enqueue or a running instance
  survives).
- medium/negative: `flush_work()` called from inside the very work item must
  be flagged (self-wait deadlock).
- hard/negative: `cancel_work_sync()` called with a spinlock held that the
  work function takes must be flagged (sleep in atomic context / deadlock).
- hard/negative: module exit returns without draining/destroying its
  workqueue and with work still queued must be flagged.

## Adversarial evals

- A work item that re-queues itself: `cancel_work()` "succeeds" yet the item
  keeps running — the agent must require `cancel_work_sync()` plus a flag
  that stops the self-reschedule.
- "flush_work then kfree" with a live producer: flush offers no exclusion
  against concurrent `queue_work` — the agent must not declare the resource
  safe after flush alone.
- `cancel_work_sync` invoked from hard-IRQ or BH context on a non-BH
  workqueue — must be flagged despite "compiling and working in a smoke
  test".
- `queue_work_on` with a CPU that can be offline during teardown.

## False-positive evals (correct code must not be flagged)

- Full teardown: disable producer, `cancel_work_sync`, `kfree`, verify no
  item runs — do NOT flag.
- `flush_work` on an already-idle item followed by a free (flush returned
  false: nothing was pending/running) — do NOT flag.
- `cancel_work()` used only to drop a pending notification for an item that
  is never freed and never re-queued — do NOT flag.
- `destroy_workqueue` after `drain_workqueue`/`flush_workqueue` with all
  `delayed_work` cancelled — do NOT flag.
- A work function that re-queues itself for a bounded polling loop with
  proper `cancel_work_sync` teardown — do NOT flag.

## Verification commands

Host (self-contained stubs — recorded this run):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_flush.c -o /tmp/good_flush
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_flush.c -o /tmp/bad_flush
/tmp/good_flush   # prints ALL CHECKS PASSED, exit 0
/tmp/bad_flush    # prints BUG reproduced: work ran after work_struct freed, exit 0
```

Target (kernel) — documented only, NOT run here:

```
# lockdep: flush-from-work-context and lock-inversion deadlocks
#   CONFIG_LOCKDEP; boot and run the driver teardown; watch for
#   "possible recursive locking detected" / workqueue lockdep annotations
make defconfig && make -j$(nproc)          # CONFIG_LOCKDEP=y, CONFIG_DEBUG_LOCK_ALLOC=y
qemu-system-x86_64 -kernel arch/x86/boot/bzImage \
  -append "console=ttyS0" -nographic

# KASAN VM: free-then-run UAF reported on the kworker access
#   CONFIG_KASAN=y + QEMU; exercise device remove with work in flight
```

## Scoring

- precision: every flagged pattern maps to a real workqueue contract
  (flush/cancel semantics, context rules, unload order).
- recall: each bad snippet is detected.
- FP-rate: good snippets (including self-requeueing polling items with proper
  teardown) produce zero flags.
