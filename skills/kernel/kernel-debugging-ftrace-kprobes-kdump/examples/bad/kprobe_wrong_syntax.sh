# intentionally incorrect: kprobe_events syntax invented ("func:arg1") and the
# control file assumed to be /proc/kprobes/events. The real API is tracefs
# /sys/kernel/tracing/kprobe_events with 'p:name func args' lines; this path
# does not exist, so the write fails and the claimed probe never runs.
echo "do_sys_openat2:arg1" > /proc/kprobes/events
