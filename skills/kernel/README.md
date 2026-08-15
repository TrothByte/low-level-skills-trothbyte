# kernel — Skills

The Linux kernel is the largest low-level system in the world. These skills cover uaccess safety, RCU and memory barriers, and atomic-context rules — the mistakes that become CVE-2022-0185 and Dirty COW.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `kernel-api-drift-migration` | Use when porting or reviewing kernel code across kernel versions: sys_call_table unexport, DRM fbdev-to-client_setup, IIO migrations, symbol availability via kallsyms, and API signature changes. Prevents code that compiles but silently does nothing after an API disappears, and pins kernel versions before claiming compatibility. | researched | `skills/kernel/kernel-api-drift-migration` |
| `kernel-atomic-context` | Use when writing, reviewing, or debugging Linux kernel code that runs in atomic context: interrupt handlers, bottom halves, spinlock-held or preemption-disabled regions. Covers what is forbidden there (sleeping kmalloc/mutex/schedule), GFP_ATOMIC, irqsave/bh lock variants, deferring to process context, and verifying with lockdep. | source-backed | `skills/kernel/kernel-atomic-context` |
| `kernel-container-internals` | Use when writing or reviewing container-adjacent kernel claims — namespaces, cgroups v2, overlayfs, OCI/runc bundles, seccomp, and capability semantics. Prevents v1-era cgroup knobs, userns-root myths, and seccomp-as-sandbox overstatements from passing as facts. | researched | `skills/kernel/kernel-container-internals` |
| `kernel-debugging-ftrace-kprobes-kdump` | Use when debugging or reviewing Linux kernel problems that need instrumentation — ftrace function graphs, tracepoints, kprobes, dynamic debug, kgdb/kdb, or kdump analysis. Prevents guessed debug knobs and filesystem confusion by requiring the real tracefs/debugfs path and exact command before any claim. | researched | `skills/kernel/kernel-debugging-ftrace-kprobes-kdump` |
| `kernel-driver-char-device-lifecycle` | Use when writing or reviewing Linux character device drivers: file_operations dispatch, copy_from_user/copy_to_user return handling, cdev/class/device creation and teardown symmetry, module unload cleanup, and reference counting. Prevents unchecked user copies, inverted read/write contracts, double class_destroy, and unloading while the device is open. | researched | `skills/kernel/kernel-driver-char-device-lifecycle` |
| `kernel-module-build-out-of-tree` | Use when building, packaging, or fixing Linux kernel modules outside the kernel tree: Kbuild Makefiles, Kconfig, kernel-headers dependency and version binding, MODULE_LICENSE/EXPORT_SYMBOL placement, and mismatch between a module built against one kernel and loaded on another. Prevents "unknown symbol", "disagrees about version", and silently-wrong module builds. | researched | `skills/kernel/kernel-module-build-out-of-tree` |
| `kernel-rcu-memory-barriers` | Use when writing or reviewing Linux kernel code that needs memory barriers, READ_ONCE/WRITE_ONCE, or RCU — publish-subscribe patterns, rcu_assign_pointer/rcu_dereference, synchronize_rcu, or atomic-context rules like no sleeping in spinlocks. Teaches the kernel memory model and why it differs from C11 atomics. | source-backed | `skills/kernel/kernel-rcu-memory-barriers` |
| `kernel-scheduler-mm-vfs-internals` | Use when writing or reviewing claims about Linux internals — the fair scheduler (CFS vs EEVDF), vruntime and lag, buddy/SLUB allocation, kmalloc vs vmalloc, and VFS dcache/inode behavior. Prevents stale pre-6.6 scheduler lore and invented /proc fields from passing as kernel knowledge. | researched | `skills/kernel/kernel-scheduler-mm-vfs-internals` |
| `kernel-uaccess-safety` | Use when writing, reviewing, or fixing Linux kernel driver code that exchanges data with user space — read/write, ioctl, mmap, poll/fasync, copy_to_user/copy_from_user, get_user/put_user, access_ok, compat_ioctl. Teaches fault-safe copying, size validation, and the -EFAULT/-ENOTTY/SIGSEGV symptom classes. | source-backed | `skills/kernel/kernel-uaccess-safety` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
