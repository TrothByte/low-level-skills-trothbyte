#!/bin/sh
# Method: prove scheduler claims against the running kernel instead of a
# memory of the code. Check the version for the CFS->EEVDF boundary, then
# dump the fields /proc/sched_debug actually prints (vruntime, lag, deadline).
# Run on a Linux host with CONFIG_SCHED_DEBUG or CONFIG_SCHED_INFO.
set -u

uname -r

if [ -r /proc/sched_debug ]; then
    grep -E "vruntime|lag|deadline|avg_vruntime" /proc/sched_debug | head -40
else
    echo "not found: /proc/sched_debug"
fi
