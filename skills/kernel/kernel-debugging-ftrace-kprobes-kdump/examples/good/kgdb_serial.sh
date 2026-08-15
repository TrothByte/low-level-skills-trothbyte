#!/bin/sh
# kgdb over serial: kgdboc kernel/module parameter + magic sysrq 'g' to
# enter the gdb stub, or kgdbwait on the boot line. Requires CONFIG_KGDB,
# CONFIG_KGDB_SERIAL_CONSOLE, CONFIG_MAGIC_SYSRQ.
set -u
cat /proc/cmdline
if [ -e /sys/module/kgdboc/parameters/kgdboc ]; then
    echo -n "kgdboc: " && cat /sys/module/kgdboc/parameters/kgdboc
fi
echo g > /proc/sysrq-trigger
echo "gdb stub should now be active on the serial console"
