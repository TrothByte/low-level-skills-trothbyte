# kernel — Skills

The Linux kernel is the largest low-level system in the world. These skills cover uaccess safety, RCU and memory barriers, and atomic-context rules — the mistakes that become CVE-2022-0185 and Dirty COW.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `kernel-atomic-context` | Use when writing, reviewing, or debugging Linux kernel code that runs in atomic context: interrupt handlers, bottom halves, spinlock-held or preemption-disabled regions. Covers what is forbidden there (sleeping kmalloc/mutex/schedule), GFP_ATOMIC, irqsave/bh lock variants, deferring to process context, and verifying with lockdep. | source-backed | `skills/kernel/kernel-atomic-context` |
| `kernel-rcu-memory-barriers` | Use when writing or reviewing Linux kernel code that needs memory barriers, READ_ONCE/WRITE_ONCE, or RCU — publish-subscribe patterns, rcu_assign_pointer/rcu_dereference, synchronize_rcu, or atomic-context rules like no sleeping in spinlocks. Teaches the kernel memory model and why it differs from C11 atomics. | source-backed | `skills/kernel/kernel-rcu-memory-barriers` |
| `kernel-uaccess-safety` | Use when writing, reviewing, or fixing Linux kernel driver code that exchanges data with user space — read/write, ioctl, mmap, poll/fasync, copy_to_user/copy_from_user, get_user/put_user, access_ok, compat_ioctl. Teaches fault-safe copying, size validation, and the -EFAULT/-ENOTTY/SIGSEGV symptom classes. | source-backed | `skills/kernel/kernel-uaccess-safety` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
