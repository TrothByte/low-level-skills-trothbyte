# Kernel Debugging: ftrace, kprobes, dyndbg, kgdb, kdump — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to
registry/sources.yaml. Status tags: KNOWN = verifiable in kernel docs/source;
INFERRED = from secondary sources, confirm on target kernel/config.

## 1. tracefs lives at /sys/kernel/tracing; debugfs and tracefs are different filesystems

- **RULE**: ftrace and tracepoints are exposed by tracefs, mounted at
  `/sys/kernel/tracing` (also mounted under debugfs at
  `/sys/kernel/debug/tracing`). dynamic_debug and kprobe raw text live in
  debugfs. Writing an ftrace knob to a debugfs-only file fails.
- **WHY AI GETS IT WRONG**: both paths contain "debug/tracing" in memory, so
  models collapse them into one mount and put `dynamic_debug/control` under
  tracefs or read kprobes from `/proc/kprobes`.
- **CORRECT REASONING**: verify the mount with `findmnt /sys/kernel/tracing`
  and test the exact file with `-f`/`-w` before writing. Concretely:
  ftrace/tracepoints = tracefs (`.../tracing/`); dynamic_debug control and
  kprobe registration text = debugfs.
- **EXAMPLE** (bad): `echo "module x +p" > /sys/kernel/tracing/dynamic_debug/control`
  (ENOENT — wrong filesystem).
- **COUNTEREXAMPLE** (good): write to `/sys/kernel/debug/dynamic_debug/control`
  after `[ -f "$CTRL" ]`.
- **VERIFICATION**: `findmnt /sys/kernel/tracing`; `[ -f /sys/kernel/debug/dynamic_debug/control ]`.
  Researched — not run on this host (no Linux).
- **SOURCE**: kernel-trace-docs (tracefs, ftrace.txt); kernel-source.

## 2. Kprobes: kprobe_events syntax and $argN; prefer tracepoints

- **RULE**: kprobes are added via `p:name func args`, `r:name`, `t:name`
  lines in `<tracefs>/kprobe_events` (CONFIG_KPROBE_EVENTS). Arguments are
  `$arg1..$argN` (function args), `$retval`, `$stack`, `$comm`. `echo >
  kprobe_events` clears all. If a tracepoint exists for the site, use it —
  cheaper and version-stable.
- **WHY AI GETS IT WRONG**: models invent the control file (`/proc/kprobes`),
  a "func:arg1" syntax, or assume kprobes work without CONFIG_KPROBE_EVENTS.
- **CORRECT REASONING**: the line grammar is `p:SYM[+offs]|MEMADDR [FETCHARGS]`;
  name each event (`p:my_open do_sys_openat2 dfd=$arg1`) and enable the
  matching `events/kprobes/<name>/enable`. Function names resolve via
  kallsyms at registration time.
- **EXAMPLE** (bad): `echo "do_sys_openat2:arg1" > /proc/kprobes/events`
  (path and syntax both invented).
- **COUNTEREXAMPLE** (good):
  ```sh
  echo 'p:my_open do_sys_openat2 dfd=$arg1 filename=$arg2' > "$TR/kprobe_events"
  echo 1 > "$TR/events/kprobes/my_open/enable"
  ```
- **VERIFICATION**: `cat "$TR/kprobe_events"` shows the registered line;
  `cat "$TR/trace"` after triggering the path. Researched — not run here.
- **SOURCE**: kernel-trace-docs (kprobetrace.rst); kernel-source (kernel/trace/trace_kprobe.c).

## 3. ftrace workflows: current_tracer, set_event, trace buffer

- **RULE**: ftrace is driven by tracefs files: `current_tracer` (nop,
  function, function_graph), `set_event` (tracepoint enable), `set_ftrace_filter`
  (function filtering), and `trace`/`trace_pipe` (read-only ring buffer).
  Always `echo 0 > tracing_on` first, then re-enable after capturing.
- **WHY AI GETS IT WRONG**: models describe "ftrace --function" like a perf
  command, forget the tracing_on toggle, or read `trace_pipe` (consumes) and
  then claim the buffer is empty.
- **CORRECT REASONING**: sequence is: stop tracing → select tracer/events →
  start tracing → trigger workload → stop → read `trace`. `trace_pipe`
  drains; `trace` is a snapshot-able copy.
- **EXAMPLE** (bad): enabling function_graph and immediately `cat trace`
  with no sleep/trigger between — empty buffer mistaken for "no calls".
- **COUNTEREXAMPLE** (good):
  ```sh
  echo 0 > "$TR/tracing_on"; echo function_graph > "$TR/current_tracer"
  echo 1 > "$TR/tracing_on"; sleep 1; echo 0 > "$TR/tracing_on"
  head -30 "$TR/trace"
  ```
- **VERIFICATION**: run the good fixture `ftrace_function_graph.sh` on a
  Linux host. Researched — not run here.
- **SOURCE**: kernel-trace-docs (ftrace.rst); kernel-source.

## 4. dyndbg: debugfs control, pr_debug gates

- **RULE**: dynamic debug (`pr_debug`/`dev_dbg` calls) is toggled by writing
  to `/sys/kernel/debug/dynamic_debug/control` (CONFIG_DYNAMIC_DEBUG). Format:
  `module NAME +p`, `func foo +p`, `file fs/* +p`. The tracefs path does not
  exist; the debugfs mount is required.
- **WHY AI GETS IT WRONG**: same filesystem confusion as rule 1; also agents
  claim `-p` disables (correct) but pair it with the wrong file.
- **CORRECT REASONING**: after writing `+p`, re-read `control` to confirm the
  call site got the flag (`^fs/mydrv` lines). Without CONFIG_DYNAMIC_DEBUG the
  `pr_debug` calls compile out entirely — check the symbol, not the log.
- **EXAMPLE** (bad): writing `+p` to `/sys/kernel/tracing/dynamic_debug/control`.
- **COUNTEREXAMPLE** (good): `echo -n "module mydrv +p" > /sys/kernel/debug/dynamic_debug/control`.
- **VERIFICATION**: `grep -c "mydrv" /sys/kernel/debug/dynamic_debug/control`.
  Researched — not run here.
- **SOURCE**: kernel-trace-docs (dynamic-debug.rst, in kernel docs);
  kernel-source (lib/dynamic_debug.c).

## 5. kgdb/kdb: kgdboc + sysrq-g; there is no kgdb sysctl

- **RULE**: kgdb talks over a console via the `kgdboc` kernel parameter or
  the `kgdboc.kgdboc` module parameter; you enter the stub with
  `echo g > /proc/sysrq-trigger` (CONFIG_MAGIC_SYSRQ) or boot with
  `kgdbwait`. kdb is the in-kernel shell over the same console.
- **WHY AI GETS IT WRONG**: models invent `/proc/sys/kernel/kgdb` and other
  plausible sysctls, or suggest running gdb against `/proc/kcore` on a live
  non-stopped kernel.
- **CORRECT REASONING**: name the real knobs: cmdline `kgdboc=ttyS0,115200`,
  module param `kgdboc.kgdboc`, sysrq `g`, cmdline `kgdbwait`, configs
  CONFIG_KGDB/KGDB_SERIAL_CONSOLE/MAGIC_SYSRQ. `gdb vmlinux` attaches to the
  stopped stub over serial, not to a live `/proc/kcore`.
- **EXAMPLE** (bad): `echo 1 > /proc/sys/kernel/kgdb` (ENOENT).
- **COUNTEREXAMPLE** (good): `echo g > /proc/sysrq-trigger` with kgdboc set.
- **VERIFICATION**: boot with `kgdbwait` in QEMU serial, connect `gdb` to the
  stub. Researched — not run here.
- **SOURCE**: kernel-trace-docs (kgdb/kdb docs, Documentation/dev-tools/kgdb.rst);
  kernel-source (drivers/tty/serial/kgdboc.c, kernel/panic.c).

## 6. kdump: crashkernel= reservation, /proc/vmcore only in the crash kernel

- **RULE**: kdump reserves memory via the `crashkernel=size` cmdline; on a
  crash the kernel boots a special crash kernel that exposes the old kernel's
  memory at `/proc/vmcore` for `crash(8)`/`vmcore-dmesg`. On a normal boot
  there is no `/proc/vmcore`.
- **WHY AI GETS IT WRONG**: agents assume `/proc/vmcore` is always readable,
  or claim the primary kernel can dump itself (it cannot — the crash kernel
  runs and saves it).
- **CORRECT REASONING**: verify `crashkernel=` in `/proc/cmdline` and a
  `Crash` region in `/proc/iomem`; only then expect `/proc/vmcore` inside the
  crash kernel. `cat /proc/vmcore` on a normal boot proves nothing about kdump.
- **EXAMPLE** (bad): `cat /proc/vmcore` on a normal boot described as a dump.
- **COUNTEREXAMPLE** (good): grep `/proc/cmdline` for `crashkernel=`, then
  check `/proc/iomem`, then (in crash kernel) use `crash`/`vmcore-dmesg`.
- **VERIFICATION**: run `kdump_vmcore_check.sh` on a Linux host configured
  with `crashkernel=`. Researched — not run here.
- **SOURCE**: kernel-source (Documentation/admin-guide/kdump/, kernel/kexec_core.c).

## Quick reference table

| Knob | Real location | Status |
|---|---|---|
| ftrace/tracepoints | tracefs `/sys/kernel/tracing` | KNOWN |
| kprobe registration | `$TR/kprobe_events`, `p:name func args` | KNOWN |
| kprobe args | `$arg1..$argN`, `$retval`, `$comm` | KNOWN |
| dynamic_debug | debugfs `/sys/kernel/debug/dynamic_debug/control` | KNOWN |
| kgdb entry | kgdboc param + `echo g > /proc/sysrq-trigger` or kgdbwait | KNOWN |
| kgdb configs | CONFIG_KGDB, KGDB_SERIAL_CONSOLE, MAGIC_SYSRQ | KNOWN |
| kdump | `crashkernel=` cmdline; `/proc/vmcore` only in crash kernel | KNOWN |
| nonexistent knobs | `/proc/sys/kernel/kgdb`, `/proc/kprobes/events`, tracefs dyndbg | KNOWN-nonexistent |
