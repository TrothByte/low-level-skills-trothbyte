# Evaluation — kernel-debugging-ftrace-kprobes-kdump

Skill: `skills/kernel/kernel-debugging-ftrace-kprobes-kdump`.
Toolchain status: RESEARCHED. No Linux host, no QEMU, no kernel source tree
on this machine. All commands are exact and documented but were NOT run;
each claim's status is marked in `references/kernel-debugging.md`.

## Synthetic evals (researched — expected behavior documented, not executed)

| Case | Fixture | Expected on Linux | Command |
|---|---|---|---|
| kprobe/negative | `bad/kprobe_wrong_syntax.sh` | write fails: path `/proc/kprobes/events` does not exist | `echo 'do_sys_openat2:arg1' > /proc/kprobes/events` → ENOENT |
| dyndbg/negative | `bad/dyndbg_wrong_fs.sh` | write fails: no tracefs `dynamic_debug/control` | `echo 'module x +p' > /sys/kernel/tracing/dynamic_debug/control` → ENOENT |
| kgdb/negative | `bad/kgdb_sysctl.sh` | write fails: no such sysctl | `echo 1 > /proc/sys/kernel/kgdb` → ENOENT |
| kdump/negative | `bad/kdump_vmcore.sh` | no `/proc/vmcore` on a normal boot | `cat /proc/vmcore` → ENOENT |
| ftrace/positive | `good/ftrace_function_graph.sh` | function_graph captures after sleep+stop | exit 0, `trace` non-empty |
| kprobe/positive | `good/kprobe_event.sh` | event registers and fires on open() | exit 0, trace lines present |
| dyndbg/positive | `good/dyndbg_enable.sh` | control writable, flag applied | exit 0 |
| kgdb/positive | `good/kgdb_serial.sh` | stub enters on sysrq-g | gdb connects over serial |
| kdump/positive | `good/kdump_vmcore_check.sh` | crashkernel= present; vmcore only in crash kernel | exit 0 |

## Verified facts (ACTUAL on this host)

None — no Linux kernel reachable from this Windows host. KNOWN-from-source
facts (see references/): tracefs at `/sys/kernel/tracing`; dyndbg control in
debugfs; kprobe_events grammar `p:name func args`; kgdb entry via kgdboc +
sysrq-g; `/proc/vmcore` only in the crash kernel. Status: UNVERIFIED by
execution.

## False-positive evals (researched)

- A ftrace sequence that stops tracing first, configures, starts, sleeps,
  stops, then reads `trace` — must pass.
- dyndbg writes to `/sys/kernel/debug/dynamic_debug/control` — must pass.
- kprobe lines that name an existing symbol and use `$argN` — must pass.

## Adversarial evals (researched)

- A fake path embedded in otherwise correct advice (tracefs dyndbg control)
  must be caught by the filesystem check.
- A claim that kprobes work without CONFIG_KPROBE_EVENTS must be caught by
  the config-symbol rule.

## Historical evals

- The nonexistent-knob class (`/proc/sys/kernel/kgdb`, `/proc/kprobes/events`)
  matches the documented failure mode of LLM debugging advice; reproduced as
  fixtures rather than fetched from a historical database (UNVERIFIED against
  upstream history).

## Target toolchains (absent, documented)

- Linux host / QEMU guest with kgdb: not present. Planned elevation: boot with
  `-serial stdio kgdbwait crashkernel=256M`, run good fixtures, record output.
- `crash`/`vmcore-dmesg`: not present; documented for the target host.
