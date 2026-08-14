#ifndef KERNEL_RCU_MB_LINUX_STUBS_H
#define KERNEL_RCU_MB_LINUX_STUBS_H

/* Host-side stand-ins for Linux kernel primitives so the examples compile on a
 * plain host compiler. Each stub carries a comment stating what the real kernel
 * primitive does; these stubs do NOT emulate kernel semantics, they only let
 * the ordering logic be exercised in user space. */

#include <stddef.h>
#include <stdlib.h>

/* Compiler barrier: prevents GCC from moving memory accesses across it.
 * This is the kernel's `barrier()` definition. */
#define barrier() asm volatile("" ::: "memory")

/* READ_ONCE/WRITE_ONCE: the kernel definitions. The volatile access prevents
 * the compiler from tearing, merging, or caching the single load/store. */
#define READ_ONCE(x) (*(volatile typeof(x) *)&(x))
#define WRITE_ONCE(x, val) (*(volatile typeof(x) *)&(x) = (val))

/* smp_rmb()/smp_wmb(): on the host only the compiler-barrier part matters;
 * on real x86 they are also empty (TSO) and on ARM/RISC-V they are hardware
 * fences (dmb ish). smp_mb() is a full barrier; the C11 seq-cst fence below
 * emits a hardware fence on x86 but, unlike the kernel's smp_mb(), the
 * standard does not guarantee it orders plain accesses -- so we add the
 * compiler barrier too. */
#define smp_rmb() barrier()
#define smp_wmb() barrier()
#define smp_mb() do { barrier(); __atomic_thread_fence(__ATOMIC_SEQ_CST); } while (0)

/* Release/acquire pair used by RCU publish-subscribe. */
#define smp_store_release(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)
#define smp_load_acquire(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)

/* rcu_assign_pointer() is a release store; rcu_dereference() is READ_ONCE
 * plus address-dependency/acquire ordering on the real kernel. */
#define rcu_assign_pointer(p, v) smp_store_release(&(p), (v))
#define rcu_dereference(p) READ_ONCE(p)

/* RCU read-side critical section: in the kernel it disables preemption so
 * readers are not migrated away from the dereference and grace periods can
 * complete. Host stub is a no-op. */
#define rcu_read_lock() do { } while (0)
#define rcu_read_unlock() do { } while (0)

/* synchronize_rcu() waits for all pre-existing readers (a grace period). It
 * blocks and cannot be emulated on the host; kernel-only. */
static inline void synchronize_rcu(void) { /* kernel-only: grace-period wait */ }

/* call_rcu() schedules a callback to run after a grace period, in softirq
 * context. The object must stay alive until the callback frees it. */
struct rcu_head { struct rcu_head *next; void (*func)(struct rcu_head *head); };
static inline void call_rcu(struct rcu_head *head,
                            void (*func)(struct rcu_head *head))
{
    (void)head; (void)func;
    /* real kernel: callback fires after a grace period, not here */
}

/* spinlock: a real kernel spinlock disables preemption; sleeping while
 * holding one is the atomic-context bug taught in examples/bad. */
typedef struct { unsigned long raw; } spinlock_t;
static inline void spin_lock_init(spinlock_t *l) { (void)l; }
static inline void spin_lock(spinlock_t *l) { (void)l; }
static inline void spin_unlock(spinlock_t *l) { (void)l; }

/* kmalloc/kfree: GFP_KERNEL may sleep (page reclaim); GFP_ATOMIC may not. */
#define GFP_KERNEL 0x00000010u
#define GFP_ATOMIC 0x00000020u
static inline void *kmalloc(size_t size, unsigned int flags)
{
    (void)flags;
    return malloc(size);
}
static inline void kfree(void *p) { free(p); }

#endif /* KERNEL_RCU_MB_LINUX_STUBS_H */
