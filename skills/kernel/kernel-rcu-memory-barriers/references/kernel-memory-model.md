# Linux Kernel Memory Model: Barriers, READ_ONCE/WRITE_ONCE, and RCU

Scope: SMP ordering and RCU rules for kernel code, per the Linux kernel memory
model (LKMM). Claim status: all normative claims are KNOWN with the primary
source cited per rule; host-stub behavior is host-specific and marked
`[host-only]`. Nothing UNVERIFIED is asserted as stable.
Each entry: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. smp_mb() / smp_rmb() / smp_wmb()

- **RULE**: `smp_mb()` orders all memory accesses on the calling CPU (no load
  or store crosses it in either direction). `smp_wmb()` orders stores against
  later stores; `smp_rmb()` orders loads against later loads. Barriers are
  PAIRING primitives: a writer barrier without a matching reader barrier does
  nothing useful.
- **WHY AI GETS IT WRONG**: puts a barrier on only one side; thinks a barrier
  "synchronizes" the two CPUs directly; or believes barriers are needed
  everywhere when one pair at the release/acquire boundary suffices.
- **CORRECT REASONING**: a barrier constrains the order of accesses on one CPU;
  together with cache-coherence propagation it creates a happens-before edge.
  The reader must pair each writer barrier so the required ordering (payload
  stores before flag store on the writer; flag load before payload loads on the
  reader) exists on both sides.
- **EXAMPLE** (bad):
  ```c
  /* writer */ data = 1; smp_wmb(); ready = 1;
  /* reader: no pairing barrier - may read stale data after ready == 1 */
  if (ready) use(data);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  /* writer */ WRITE_ONCE(data, 1); smp_wmb(); WRITE_ONCE(ready, 1);
  /* reader */ if (READ_ONCE(ready)) { smp_rmb(); use(READ_ONCE(data)); }
  ```
- **VERIFICATION**: LKMM litmus tests (tools/memory-model); host: compiler
  barrier `asm volatile("" ::: "memory")` demo (examples/demo/ordering_demo.c);
  target: lockdep/KCSAN builds.
- **SOURCE**: `linux-memory-barriers` (SMP barrier pairing, smp_mb/smp_rmb/
  smp_wmb semantics); `linux-rcu` (same rules apply inside RCU sections).

## 2. READ_ONCE / WRITE_ONCE

- **RULE**: `READ_ONCE(x)` / `WRITE_ONCE(x, v)` perform a single, untearable
  access that the compiler cannot merge, split, cache, or eliminate. They are
  NOT barriers and NOT atomic RMW operations.
- **WHY AI GETS IT WRONG**: treats them as acquire/release fences; uses a plain
  access "because the compiler is usually fine"; or expects `WRITE_ONCE` to
  order a payload published earlier.
- **CORRECT REASONING**: WRITE_ONCE/READ_ONCE kill tearing and register
  caching of one access. Ordering of *other* data requires the surrounding
  `smp_mb()`/RCU machinery. A plain access to a variable another context writes
  is a data race (UB) regardless of how often it happens to work.
- **EXAMPLE** (bad):
  ```c
  void set_flag(int v) { ready = v; }       /* plain store: may be fused */
  int spin(void) { while (!ready) ; return ready; }  /* plain load: cached */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  void set_flag(int v) { WRITE_ONCE(ready, v); }
  int spin(void) { int v; while ((v = READ_ONCE(ready)) == 0) ; return v; }
  ```
- **VERIFICATION**: compile at -O2 and inspect asm (single mov, no hoisting);
  KCSAN flags the plain-access data race.
- **SOURCE**: `linux-memory-barriers` (compiler access/volatile rules);
  `linux-rcu` (rcu_dereference documentation defines it as READ_ONCE-based).

## 3. rcu_read_lock() / rcu_read_unlock()

- **RULE**: these delimit an RCU read-side critical section. In the kernel they
  disable preemption; readers must not sleep, and writers never wait for
  readers. A grace period cannot complete while this section is open.
- **WHY AI GETS IT WRONG**: treats it as a lock that excludes writers ("readers
  block writers"), or sleeps inside it ("it's just a marker").
- **CORRECT REASONING**: RCU is reader-writer lock-free on the read path:
  readers mark their section so `synchronize_rcu()` can know when all
  pre-existing readers have exited before old objects are freed. Writers
  publish and defer the free; they never block on readers.
- **EXAMPLE** (bad):
  ```c
  rcu_read_lock();
  mutex_lock(&m);              /* sleeping in RCU read-side: illegal */
  p = rcu_dereference(g_ptr);
  rcu_read_unlock();
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  rcu_read_lock();
  p = rcu_dereference(g_ptr);
  if (p) use(p->value);
  rcu_read_unlock();
  ```
- **VERIFICATION**: lockdep with CONFIG_PROVE_RCU emits "suspicious RCU usage"
  splats; CONFIG_DEBUG_ATOMIC_SLEEP catches the sleep.
- **SOURCE**: `linux-rcu` (whatisRCU, checklist: read-side sections may not
  sleep); `ldd3` ch.10 (interrupt/atomic context rules).

## 4. rcu_dereference()

- **RULE**: every load of an RCU-protected pointer must use
  `rcu_dereference(p)` (or a documented protected variant), inside an
  `rcu_read_lock()` section. It is READ_ONCE plus address-dependency/acquire
  ordering so payload writes are visible when the new pointer is seen.
- **WHY AI GETS IT WRONG**: dereferences the raw pointer ("it's word-sized, a
  plain load is atomic"), or puts `rcu_dereference` on the write side, or skips
  `rcu_read_lock()`.
- **CORRECT REASONING**: the writer's `rcu_assign_pointer()` is a release
  store; a plain load would be a data race (UB) and would get no ordering.
  `rcu_dereference()` makes it a single atomic-style load with the acquire
  edge. `__rcu` sparse annotations and `make C=1` enforce this.
- **EXAMPLE** (bad):
  ```c
  struct foo *p = g_ptr;      /* plain load: races with rcu_assign_pointer */
  return p->value;
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  rcu_read_lock();
  struct foo *p = rcu_dereference(g_ptr);
  int v = p ? p->value : -1;
  rcu_read_unlock();
  ```
- **VERIFICATION**: KCSAN data-race report on the plain load; sparse `__rcu`
  warnings; checkpatch flags dereferencing an RCU pointer without protection.
- **SOURCE**: `linux-rcu` (rcu_dereference documentation, checklist);
  `linux-memory-barriers` (RCU subsection).

## 5. rcu_assign_pointer()

- **RULE**: every store of an RCU-protected pointer uses `rcu_assign_pointer`.
  It is a release store: payload writes before it are visible to readers that
  observe the new pointer. The old object stays alive for in-flight readers and
  is freed only after a grace period.
- **WHY AI GETS IT WRONG**: publishes with a plain assignment (loses release
  ordering), or frees the old pointer immediately after publishing the new one.
- **CORRECT REASONING**: publish order = release. Readers using
  `rcu_dereference` see payload writes via the acquire/dependency edge. The
  previous pointer's object must be freed after `synchronize_rcu()`/callback
  (grace period), never while readers may still dereference it.
- **EXAMPLE** (bad):
  ```c
  g_ptr = new_item;          /* plain store: release ordering lost */
  kfree(old_item);           /* may still be in use by readers */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  new_item->value = 42;
  rcu_assign_pointer(g_ptr, new_item);
  synchronize_rcu();
  kfree(old_item);
  ```
- **VERIFICATION**: KCSAN; sparse `__rcu`; asm inspection (release store).
- **SOURCE**: `linux-rcu` (rcu_assign_pointer docs, checklist);
  `linux-memory-barriers` (RCU subsection).

## 6. synchronize_rcu()

- **RULE**: blocks until all pre-existing readers have left their critical
  sections (a grace period); after it returns, old objects may be freed. It may
  sleep and therefore cannot be called in atomic context or from inside an RCU
  read-side critical section.
- **WHY AI GETS IT WRONG**: thinks it frees the object, thinks it returns
  immediately, or calls it while holding a spinlock or inside `rcu_read_lock`.
- **CORRECT REASONING**: it only waits. Because the caller itself may be a
  reader, calling it from a read-side section waits on itself (never completes
  for that CPU); calling it in atomic context is a sleep-in-atomic bug.
- **EXAMPLE** (bad):
  ```c
  spin_lock(&lock);
  synchronize_rcu();         /* sleeps while holding a spinlock */
  spin_unlock(&lock);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  rcu_assign_pointer(g_ptr, new_item);
  synchronize_rcu();         /* outside locks, outside read-side */
  kfree(old_item);
  ```
- **VERIFICATION**: lockdep/RCU lockdep splats; target-only (not host-emulatable).
- **SOURCE**: `linux-rcu` (whatisRCU, checklist: grace-period rules);
  `linux-memory-barriers` (RCU subsection).

## 7. call_rcu()

- **RULE**: schedules a callback to run after a grace period, in softirq
  context (BH disabled). It may be called from atomic context (that is its
  purpose); the callback itself must not sleep, and the object must stay alive
  until the callback runs and frees it.
- **WHY AI GETS IT WRONG**: frees the object right after `call_rcu()` (use-after
  free before the callback), or sleeps inside the callback.
- **CORRECT REASONING**: `call_rcu(head, func)` only registers work. `func`
  runs later, after a grace period, and is where the memory is freed. Between
  registration and callback the object is still referenced by RCU.
- **EXAMPLE** (bad):
  ```c
  call_rcu(&obj->rcu, obj_free);   /* obj_free kfree()s obj */
  kfree(obj);                      /* UAF: frees before the callback runs */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  call_rcu(&obj->rcu, obj_free);   /* obj_free does the kfree(obj) itself */
  ```
- **VERIFICATION**: KASAN use-after-free reports; target-only.
- **SOURCE**: `linux-rcu` (whatisRCU: call_rcu usage); `ldd3` ch.5 (brief RCU).

## 8. Why kernel barriers differ from C11 atomics

- **RULE**: the kernel uses its own memory model (LKMM, tools/memory-model,
  Documentation/memory-barriers.txt) built on READ_ONCE/WRITE_ONCE, asm
  compiler barriers, and arch-specific fences. It does not use C11
  `<stdatomic.h>` ordering on ordinary kernel shared data. A C11 seq-cst fence
  only orders atomic operations; the kernel's `smp_mb()` also orders plain
  accesses.
- **WHY AI GETS IT WRONG**: writes `atomic_thread_fence(memory_order_seq_cst)`
  in kernel code thinking it is `smp_mb()`, or translates the C11 model to the
  kernel 1:1.
- **CORRECT REASONING**: in the kernel, plain (non-atomic) accesses are part of
  the model and are ordered by barriers/RCU once correctly written; in C11,
  plain accesses get nothing from fences. On the host, `__atomic_thread_fence`
  is only a pragmatic stand-in for the demo; it is not equivalent to the real
  `smp_mb()`. Use kernel primitives in kernel code.
- **EXAMPLE** (bad): `__atomic_thread_fence(__ATOMIC_SEQ_CST)` used as
  `smp_mb()` around plain kernel accesses on a weakly-ordered CPU.
- **COUNTEREXAMPLE** (good): `smp_mb()` / `smp_store_release()` /
  `smp_load_acquire()` and the kernel's RCU primitives.
- **VERIFICATION**: tools/memory-model litmus tests; asm diff at -O2.
- **SOURCE**: `linux-memory-barriers` (LKMM definition, compiler barrier);
  `linux-rcu` (rcu_dereference/assign_pointer are defined on kernel primitives,
  not C11 atomics).

## 9. Atomic context: no sleeping

- **RULE**: code running in atomic context (spinlock held, interrupt handler,
  BH disabled, preemption disabled, RCU read-side) must not call sleeping
  functions: `kmalloc(GFP_KERNEL)`, `mutex_lock`, `down_interruptible`,
  `schedule`, `synchronize_rcu`, `copy_to/from_user` (can fault/sleep). Use
  `GFP_ATOMIC`, pre-allocate, or defer work.
- **WHY AI GETS IT WRONG**: "kmalloc rarely sleeps on my workload", "spinlocks
  only matter on PREEMPT kernels", "copy_to_user is just a copy".
- **CORRECT REASONING**: the rule is unconditional. GFP_KERNEL can sleep on page
  reclaim even when memory seems available; `copy_to_user` can fault on swapped
  pages; sleeping in a spinlock deadlocks the lock holder. CONFIG_DEBUG_ATOMIC_SLEEP
  turns this into a build-time-checked runtime splat.
- **EXAMPLE** (bad):
  ```c
  spin_lock(&lock);
  buf = kmalloc(4096, GFP_KERNEL);   /* may sleep while preemption disabled */
  spin_unlock(&lock);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  buf = kmalloc(4096, GFP_KERNEL);   /* allocate before taking the lock */
  spin_lock(&lock);
  use(buf);
  spin_unlock(&lock);
  kfree(buf);
  ```
- **VERIFICATION**: CONFIG_DEBUG_ATOMIC_SLEEP ("BUG: sleeping function called
  from invalid context"); lockdep.
- **SOURCE**: `ldd3` ch.5 (spinlocks: no sleeping while held), ch.8 (GFP_KERNEL
  may sleep); `linux-rcu` (read-side sections may not sleep).

## 10. lockdep / RCU lockdep annotations

- **RULE**: build with CONFIG_PROVE_LOCKING and CONFIG_PROVE_RCU; use
  `rcu_read_lock_held()`, `lockdep_assert_held()`, and the protected variants
  (`rcu_dereference_protected`) so violations are caught at runtime. KCSAN
  (CONFIG_KCSAN) detects the data races lockdep cannot see.
- **WHY AI GETS IT WRONG**: dismisses RCU/lockdep splats as false positives, or
  never adds the annotations that let lockdep verify a helper's contract.
- **CORRECT REASONING**: a helper that dereferences an RCU pointer must either
  be called under `rcu_read_lock()` or assert it via `rcu_read_lock_held()`;
  otherwise RCU lockdep splats on valid-but-unannotated code and misses real
  violations. The annotation is part of the API contract, not noise.
- **EXAMPLE** (bad): a helper doing `rcu_dereference(g_ptr)` with no caller-side
  guarantee and no `rcu_read_lock_held()` assertion.
- **COUNTEREXAMPLE** (good):
  ```c
  int read_item(void) {
      WARN_ON_ONCE(!rcu_read_lock_held());      /* documents the contract */
      return rcu_dereference(g_ptr)->value;
  }
  ```
- **VERIFICATION**: boot under QEMU with PROVE_LOCKING+KCSAN; dmesg splats.
- **SOURCE**: `linux-rcu` (lockdep/RCU-lockdep documentation, checklist);
  `linux-memory-barriers` (KCSAN-detected races class).

## Quick reference

| Situation | Use | Pairs with |
|---|---|---|
| single shared access | `READ_ONCE`/`WRITE_ONCE` | — (no ordering) |
| store-then-store order | `smp_wmb()` | reader `smp_rmb()` |
| load-then-load order | `smp_rmb()` | writer `smp_wmb()` |
| full ordering | `smp_mb()` | `smp_mb()` on the other side |
| publish pointer | `rcu_assign_pointer` | reader `rcu_dereference` |
| read published pointer | `rcu_dereference` in `rcu_read_lock()` | writer publish |
| wait for readers | `synchronize_rcu()` | then free |
| free in atomic context | `call_rcu` + free in callback | grace period |
