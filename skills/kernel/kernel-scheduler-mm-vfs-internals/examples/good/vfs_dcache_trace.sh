#!/bin/sh
# Method: use kernel tracepoints for dcache behavior; never guess a proc line
# name from memory. Requires root and tracingfs mounted.
set -u

TR=/sys/kernel/tracing
if [ ! -d "$TR" ] && [ -d /sys/kernel/debug/tracing ]; then
    TR=/sys/kernel/debug/tracing
fi
if [ ! -d "$TR" ]; then
    echo "tracingfs not mounted: mount -t tracefs tracefs /sys/kernel/tracing"
    exit 2
fi

echo 0 > "$TR/tracing_on"
echo "d_lookup" > "$TR/set_event"
echo 1 > "$TR/tracing_on"
sleep 2
echo 0 > "$TR/tracing_on"
head -25 "$TR/trace"
echo 0 > "$TR/set_event"
