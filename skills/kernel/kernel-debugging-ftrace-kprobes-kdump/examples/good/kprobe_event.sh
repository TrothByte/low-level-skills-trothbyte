#!/bin/sh
# kprobe via tracefs kprobe_events (CONFIG_KPROBE_EVENTS). Function symbols
# come from kallsyms; arguments use $argN/$ctx syntax. A tracepoint can
# replace most kprobes and is cheaper — prefer it when one exists.
set -u
TR=${TRACE_PATH:-/sys/kernel/tracing}
[ -w "$TR/kprobe_events" ] || { echo "no kprobe_events (CONFIG_KPROBE_EVENTS off or not root)"; exit 2; }
echo 'p:my_open do_sys_openat2 dfd=$arg1 filename=$arg2 flags=$arg3 mode=$arg4' > "$TR/kprobe_events"
echo 1 > "$TR/events/kprobes/my_open/enable"
sleep 2
echo 0 > "$TR/events/kprobes/my_open/enable"
head -20 "$TR/trace"
echo > "$TR/kprobe_events"
