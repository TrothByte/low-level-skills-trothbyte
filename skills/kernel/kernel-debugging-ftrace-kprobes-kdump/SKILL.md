---
name: kernel-debugging-ftrace-kprobes-kdump
description: Use when debugging or reviewing Linux kernel problems that need instrumentation — ftrace function graphs, tracepoints, kprobes, dynamic debug, kgdb/kdb, or kdump analysis. Prevents guessed debug knobs and filesystem confusion by requiring the real tracefs/debugfs path and exact command before any claim.
---

# Kernel Debugging: ftrace, kprobes, dyndbg, kgdb, kdump

## When to use

- Debugging kernel crashes, hangs, or wrong behavior where the agent proposes
  instrumenting the kernel instead of guessing the cause.
- Writing/using ftrace (function_graph, tracepoints), kprobes, dynamic debug
  (`pr_debug`), kgdb/kdb, or kdump/crash workflows.
- Reviewing kernel-debug advice for invented files, sysctls, or syntax.
- Interpreting `/proc/kallsyms`, `/proc/iomem`, `/sys/kernel/tracing/*`,
  `/sys/kernel/debug/*`.

## When not to use

- Kernel internals *behavior* (scheduler/MM/VFS claims) — use
  `kernel-scheduler-mm-vfs-internals`.
- User-space debugging (gdb/ASan) — use `dwarf-debug-info`,
  `sanitizer-report-reading`, `crash-triage`.
- Containers/cgroups — use `kernel-container-internals`.

## What the agent often gets wrong

- Confusing tracefs and debugfs: putting `dynamic_debug/control` under
  `/sys/kernel/tracing/` or reading kprobes from `/proc/kprobes/events`.
- Inventing sysctls and files (`/proc/sys/kernel/kgdb`) that never existed.
- Wrong kprobe syntax (`func:arg1` instead of `p:name func args`), or
  assuming kprobes exist without CONFIG_KPROBE_EVENTS.
- Reading `trace_pipe` (consumes the buffer) and then claiming "no events".
- Forgetting the stop→configure→start→capture sequence for ftrace.
- Assuming `/proc/vmcore` exists on a normal boot (kdump runs a crash kernel).
- Claiming gdb can attach to a live `/proc/kcore` instead of using kgdb's
  serial stub.

## How to reason correctly

1. Pick the instrument by cost and availability: tracepoint (cheapest, stable)
   → ftrace function/function_graph → kprobe (needs symbol) → kgdb (full stop).
2. Name the exact control file and verify it exists/is writable before
   writing: `[ -w "$TR/kprobe_events" ]`, `[ -f "$CTRL" ]`.
3. Follow the ftrace sequence: stop tracing, select tracer/events, start,
   trigger workload, stop, read `trace` (not `trace_pipe` unless you want to
   consume).
4. For kgdb: kgdboc parameter + sysrq `g` or `kgdbwait`; no sysctl.
5. For kdump: verify `crashkernel=` and `/proc/iomem` first; `/proc/vmcore`
   is only meaningful inside the crash kernel.
6. If a knob does not exist on this kernel config, say so instead of "it
   should work" — check CONFIG_* in `/boot/config` or `/proc/config.gz`.

## What to verify

- The file path exists and the mount is the right filesystem (tracefs vs
  debugfs).
- The exact write syntax (kprobe_events grammar, dyndbg `+p`/`-p`).
- CONFIG symbols (FTRACE, KPROBE_EVENTS, DYNAMIC_DEBUG, KGDB,
  KGDB_SERIAL_CONSOLE, MAGIC_SYSRQ, KEXEC/KEXEC_FILE).
- The trace buffer was actually captured while the workload ran.
- For kdump: the crashkernel reservation is present before expecting
  `/proc/vmcore`.

## How to verify

```
findmnt /sys/kernel/tracing
[ -f /sys/kernel/debug/dynamic_debug/control ] && echo "dyndbg ok"
cat /proc/cmdline | tr ' ' '\n' | grep -E "crashkernel|kgdb"
echo 'p:my_open do_sys_openat2 dfd=$arg1 filename=$arg2' > /sys/kernel/tracing/kprobe_events
echo 1 > /sys/kernel/tracing/events/kprobes/my_open/enable
# ftrace function graph (good/ftrace_function_graph.sh), kgdb (good/kgdb_serial.sh),
# kdump (good/kdump_vmcore_check.sh)
```

Boot a kernel under QEMU with `-serial stdio` and `kgdbwait`/`crashkernel=` for
repeatable runs. On this host (no Linux, no QEMU) these are documented, not run.

## Where the knowledge comes from

- `kernel-trace-docs` — ftrace, kprobe/trace events, tracefs documentation.
- `kernel-source` — kernel/trace/*, drivers/tty/serial/kgdboc.c, kexec/core.c,
  Documentation/dev-tools/kgdb.rst, Documentation/admin-guide/kdump.

## Related skills

- `kernel-scheduler-mm-vfs-internals` — the code under test; verify behavior
  claims there, instrument here.
- `kernel-uaccess-safety`, `kernel-atomic-context`, `kernel-rcu-memory-barriers`
  — kernel context rules that constrain where you may instrument.
- `qemu-system-setup` — running the kernel that these tools debug.

## Evaluation

- Synthetic: bad fixtures must be recognized — invented kprobe syntax, dyndbg
  in the wrong filesystem, a nonexistent kgdb sysctl, `/proc/vmcore` on a
  normal boot.
- False-positive: correct sequences (stop→configure→start→capture, debugfs
  dyndbg control, kgdboc+sysrq) must pass unmodified.
- Adversarial: a plausible-but-fake tracefs path and a "should work without
  CONFIG_KPROBE_EVENTS" claim must be caught by the config-check rule.
- Historical: no curated kernel-debug CVE corpus; the nonexistent-knob class
  is reproduced as fixtures (UNVERIFIED against a live kernel).
- Researched gap: all commands are exact and documented but were not run — a
  QEMU/Linux host is required to make this skill source-backed.
