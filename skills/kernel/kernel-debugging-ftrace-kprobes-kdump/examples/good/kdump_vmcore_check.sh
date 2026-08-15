#!/bin/sh
# kdump: crashkernel= reservation must be in /proc/cmdline and /proc/iomem;
# /proc/vmcore exists ONLY inside the crash kernel, never on a normal boot.
set -u
grep -o "crashkernel=[^ ]*" /proc/cmdline || echo "no crashkernel= reservation"
grep -i crash /proc/iomem || echo "no crash regions in iomem"
if [ -e /proc/vmcore ]; then
    echo "crash kernel active; extract with crash(8) or vmcore-dmesg"
fi
