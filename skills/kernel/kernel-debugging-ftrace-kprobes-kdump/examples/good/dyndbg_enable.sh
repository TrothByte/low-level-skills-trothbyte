#!/bin/sh
# dyndbg: the control file lives in DEBUGFS, not tracefs
# (/sys/kernel/debug/dynamic_debug/control) and needs CONFIG_DYNAMIC_DEBUG.
set -u
CTRL=/sys/kernel/debug/dynamic_debug/control
[ -f "$CTRL" ] || { echo "debugfs not mounted or CONFIG_DYNAMIC_DEBUG off"; exit 2; }
echo -n "module mydrv +p" > "$CTRL"
grep -c "mydrv" "$CTRL"
