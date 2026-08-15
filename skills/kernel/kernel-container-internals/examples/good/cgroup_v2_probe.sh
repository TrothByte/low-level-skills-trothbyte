#!/bin/sh
# cgroup v2 probe: unified hierarchy at /sys/fs/cgroup; controllers must be
# enabled in cgroup.subtree_control; v1-style files do not exist here.
set -u
[ -f /sys/fs/cgroup/cgroup.controllers ] || { echo "cgroup v2 not mounted"; exit 2; }
echo "== controllers the kernel knows =="
cat /sys/fs/cgroup/cgroup.controllers
echo "== my cgroup =="
cat /proc/self/cgroup
echo "== memory controller state (if enabled) =="
cat /sys/fs/cgroup/memory.current 2>/dev/null || echo "memory controller not in subtree_control"
echo "== systemd scope with a memory limit (if systemd present) =="
systemd-run --scope -p MemoryMax=16M sleep 0.2 2>/dev/null || echo "systemd-run not usable here"
