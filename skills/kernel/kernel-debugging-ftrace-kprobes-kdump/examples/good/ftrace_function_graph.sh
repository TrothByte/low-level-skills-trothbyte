#!/bin/sh
# ftrace via tracefs (CONFIG_FTRACE). Modern kernels mount tracefs at
# /sys/kernel/tracing; the legacy debugfs path is a fallback to check, not an
# assumption.
set -u
TR=${TRACE_PATH:-/sys/kernel/tracing}
[ -d "$TR" ] || TR=/sys/kernel/debug/tracing
[ -d "$TR" ] || { echo "tracefs not mounted (mount -t tracefs tracefs /sys/kernel/tracing)"; exit 2; }
echo 0 > "$TR/tracing_on"
echo function_graph > "$TR/current_tracer"
echo 1 > "$TR/tracing_on"
sleep 1
echo 0 > "$TR/tracing_on"
head -30 "$TR/trace"
echo nop > "$TR/current_tracer"
